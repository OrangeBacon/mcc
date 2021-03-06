#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define __USE_MINGW_ANSI_STDIO 1


#define UNICODE
#define NTDDI_VERSION 0x06000000
#include <windows.h>
#include <pathcch.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <shlwapi.h>

static Path currentDirectory;

// see MSDN
#define PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH 0x00000010
#define PATHCCH_ENSURE_TRAILING_SLASH 0x00000020
#define S_OK ((HRESULT)0x00000000)
#define FLAG_ATTRIBUTE_NORMAL 0x80

// common flags for path processing
#define PathFlags (PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH | PATHCCH_ENSURE_TRAILING_SLASH)

// Initialise the file system wrapper
void FilesInit() {
    unsigned long len = GetCurrentDirectoryW(0, NULL) + 1;
    wchar_t* buf = ArenaAlloc(len * sizeof(wchar_t));
    GetCurrentDirectoryW(len, buf);

    HRESULT r = PathAllocCanonicalize(buf, PathFlags, &currentDirectory.buf);
    if(r != S_OK) {
        printf("Error getting current directory\n");
        exit(0);
    }
}

Path getStartupDirectory() {
    return currentDirectory;
}

// Attempt to findd MinGW based on where it installed itself on my computer
// only detects the 64 bit version, no plans for 32 bit support
static void FindMinGWW64WinBuilds(IncludeSearchPath* search) {
    wchar_t* programFiles;
    SHGetKnownFolderPath(&FOLDERID_ProgramFilesX64, 0, NULL, &programFiles);

    wchar_t* mingwFolder;
    PathAllocCombine(programFiles, TEXT("mingw-w64"), PathFlags, &mingwFolder);
    CoTaskMemFree(programFiles);

    wchar_t* mingwSearch;
    // will error if it ends in backslash
    PathAllocCombine(mingwFolder, TEXT("x86_64-*"), PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH, &mingwSearch);

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(mingwSearch, &ffd);
    LocalFree(mingwSearch);

    if(hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    FindClose(hFind);
    if(!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return;
    }

    wchar_t* mingwVersion;
    PathAllocCombine(mingwFolder, ffd.cFileName, PathFlags, &mingwVersion);
    LocalFree(mingwFolder);

    wchar_t* libgcc;
    PathAllocCombine(mingwVersion, TEXT("mingw64\\lib\\gcc\\x86_64-w64-mingw32"), PathFlags, &libgcc);

    wchar_t* libgccVersionSearch;
    PathAllocCombine(libgcc, TEXT("*.*.0"), PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH, &libgccVersionSearch);

    hFind = FindFirstFileW(libgccVersionSearch, &ffd);
    LocalFree(libgccVersionSearch);

    if(hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    FindClose(hFind);
    if(!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return;
    }

    wchar_t* libgccVersion;
    PathAllocCombine(libgcc, ffd.cFileName, PathFlags, &libgccVersion);
    LocalFree(libgcc);

    wchar_t* libgccInclude;
    PathAllocCombine(libgccVersion, TEXT("include"), PathFlags, &libgccInclude);
    ARRAY_PUSH(*search, system, ((Path){libgccInclude, true}));

    wchar_t* libgccFixedInclude;
    PathAllocCombine(libgccVersion, TEXT("include-fixed"), PathFlags, &libgccFixedInclude);
    ARRAY_PUSH(*search, system, ((Path){libgccFixedInclude, true}));

    LocalFree(libgccVersion);

    wchar_t* mingwInclude;
    PathAllocCombine(mingwVersion, TEXT("mingw64\\x86_64-w64-mingw32\\include"), PathFlags, &mingwInclude);
    ARRAY_PUSH(*search, system, ((Path){mingwInclude, true}));
    LocalFree(mingwVersion);
}

// try adding Chocolatey's install directories, used by github actions
static void FindMinGWW64Chocolatey(IncludeSearchPath* search) {
    wchar_t* programData;
    SHGetKnownFolderPath(&FOLDERID_ProgramData, 0, NULL, &programData);

    wchar_t* mingwFolder;
    PathAllocCombine(programData, TEXT("Chocolatey\\lib\\mingw\\tools\\install\\mingw64"), PathFlags, &mingwFolder);
    CoTaskMemFree(programData);

    wchar_t* libgcc;
    PathAllocCombine(mingwFolder, TEXT("lib\\gcc\\x86_64-w64-mingw32"), PathFlags, &libgcc);

    wchar_t* libgccVersionSearch;
    PathAllocCombine(libgcc, TEXT("*.*.0"), PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH, &libgccVersionSearch);

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(libgccVersionSearch, &ffd);
    LocalFree(libgccVersionSearch);

    if(hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    FindClose(hFind);
    if(!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return;
    }

    wchar_t* libgccVersion;
    PathAllocCombine(libgcc, ffd.cFileName, PathFlags, &libgccVersion);
    LocalFree(libgcc);

    wchar_t* libgccInclude;
    PathAllocCombine(libgccVersion, TEXT("include"), PathFlags, &libgccInclude);
    ARRAY_PUSH(*search, system, ((Path){libgccInclude, true}));

    wchar_t* libgccFixedInclude;
    PathAllocCombine(libgccVersion, TEXT("include-fixed"), PathFlags, &libgccFixedInclude);
    ARRAY_PUSH(*search, system, ((Path){libgccFixedInclude, true}));

    LocalFree(libgccVersion);

    wchar_t* mingwInclude;
    PathAllocCombine(mingwFolder, TEXT("x86_64-w64-mingw32\\include"), PathFlags, &mingwInclude);
    ARRAY_PUSH(*search, system, ((Path){mingwInclude, true}));
    LocalFree(mingwFolder);
}

// adds all values from %path% to the search path
static void FindPath(IncludeSearchPath* search) {
    DWORD len = GetEnvironmentVariableW(TEXT("path"), NULL, 0);
    if(len == 0) {
        return;
    }

    wchar_t* path = ArenaAlloc(sizeof(wchar_t) * (len + 1));
    if(path == NULL) {
        return;
    }
    len = GetEnvironmentVariableW(TEXT("path"), path, len + 1);
    if(len == 0) {
        return;
    }

    while(true) {
        wchar_t* ptr = wcschr(path, ';');
        if(ptr) *ptr = '\0';

        wchar_t* newPath;
        PathAllocCanonicalize(path, PathFlags, &newPath);

        ARRAY_PUSH(*search, system, ((Path){newPath, true}));

        if(!ptr) return;
        path = ptr + 1;
        if(path[0] == '\0') return;
    }
}

// adds all user specified (-I) paths to the search path
// if specifed as `-I-`, then adds as a system directory, else user
static void AddIncludes(IncludeSearchPath* search, const char** includePaths, size_t includeCount) {
    for(size_t i = 0; i < includeCount; i++) {

        int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, includePaths[i], -1, NULL, 0);
        wchar_t* buf = ArenaAlloc(sizeof(wchar_t) * len);
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, includePaths[i], -1, buf, len);

        for(int i = 0; i < len; i++) {
            if(buf[i] == L'/') {
                buf[i] = L'\\';
            }
        }

        if(buf[0] == '-') {
            wchar_t* path;
            PathAllocCombine(currentDirectory.buf, buf + 1, PathFlags, &path);
            ARRAY_PUSH(*search, system, ((Path){path, true}));
        } else {
            wchar_t* path;
            PathAllocCombine(currentDirectory.buf, buf, PathFlags, &path);
            ARRAY_PUSH(*search, user, ((Path){path, true}));
        }
    }
}

// removes some include paths
// - ones that do not exist
// - ones that contain `bin\` in their path - likely to be binary, not
//   source files
static void FilterPaths(Path* list, size_t count, bool filterBin) {
    for(unsigned int i = 0; i < count; i++) {
        Path* path = &list[i];

        DWORD attrs = GetFileAttributesW(path->buf);
        if(attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            path->valid = false;
            continue;
        }

        if(!filterBin) continue;

        wchar_t* ptr = path->buf;
        while(true) {
            ptr++;
            if(*ptr == '\0') break;
            if(_wcsnicmp(TEXT("bin\\"), ptr, 4) == 0) {
                path->valid = false;
                break;
            }
            ptr = wcschr(ptr, '\\');
        }
    }
}

// initialise a set of search paths with all include paths
// - tries to detect system paths
// - adds all of %path%
// - adds arguments specified in -I
void IncludeSearchPathInit(IncludeSearchPath* search, SystemType type, const char** includePaths, size_t includeCount) {
    // alloc, add system include paths, %path%
    ARRAY_ALLOC(Path, *search, system);
    ARRAY_ALLOC(Path, *search, user);

    if(type & SYSTEM_MINGW_W64) {
        FindMinGWW64WinBuilds(search);
        FindMinGWW64Chocolatey(search);
    }

    if(type & SYSTEM_MINGW_W64 || type & SYSTEM_MSVC) {
        FindPath(search);
    }

    AddIncludes(search, includePaths, includeCount);
    FilterPaths(search->systems, search->systemCount, true);
    FilterPaths(search->users, search->userCount, false);
}

static const char* includeValidCheck(Path* currentPath, const char* fileName) {
    if(!currentPath->valid) return NULL;
    int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fileName, -1, NULL, 0);
    wchar_t* wbuf = ArenaAlloc(sizeof(wchar_t) * len);
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fileName, -1, wbuf, len);

    wchar_t* fullPath;
    PathAllocCombine(currentPath->buf, wbuf, PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH, &fullPath);

    DWORD attrs = GetFileAttributesW(fullPath);
    if(attrs == INVALID_FILE_ATTRIBUTES || attrs & FILE_ATTRIBUTE_DIRECTORY) {
        return NULL;
    }

    len = WideCharToMultiByte(CP_ACP, 0, fullPath, -1, NULL, 0, NULL, NULL);
    char* cbuf = ArenaAlloc(sizeof(char) * len);
    WideCharToMultiByte(CP_ACP, 0, fullPath, -1, cbuf, len, NULL, NULL);

    LocalFree(fullPath);

    return cbuf;
}

const char* IncludeSearchPathFindSys(IncludeSearchState* state, IncludeSearchPath* path, const char* fileName) {
    size_t checkedCount = 0;
    if(state->hasStarted) {
        checkedCount = state->checkedCount;
        if(state->inUser) return IncludeSearchPathFindUser(state, path, fileName);
    } else {
        state->checkedCount = 0;
        state->inUser = false;
        state->hasStarted = true;
    }

    for(unsigned int i = checkedCount; i < path->systemCount; i++) {
        state->checkedCount++;
        const char* res = includeValidCheck(&path->systems[i], fileName);
        if(res != NULL) return res;
    }

    return NULL;
}

const char* IncludeSearchPathFindUser(IncludeSearchState* state, IncludeSearchPath* path, const char* fileName) {
    size_t checkedCount = 0;
    if(state->hasStarted) {
        checkedCount = state->checkedCount;
        if(!state->inUser) return IncludeSearchPathFindSys(state, path, fileName);
    } else {
        state->checkedCount = 0;
        state->inUser = true;
        state->hasStarted = true;
    }

    for(unsigned int i = checkedCount; i < path->userCount; i++) {
        state->checkedCount = i;
        const char* res = includeValidCheck(&path->users[i], fileName);
        if(res != NULL) return res;
    }

    state->checkedCount = 0;
    state->inUser = false;
    return IncludeSearchPathFindSys(state, path, fileName);
}

wchar_t* charToWchar(const char* str, int* lenPtr) {
    int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, -1, NULL, 0);
    wchar_t* wideStr = ArenaAlloc(sizeof(wchar_t) * len);
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, -1, wideStr, len);

    if(lenPtr != NULL) {
        *lenPtr = len;
    }

    return wideStr;
}

wchar_t* pathToWchar(const char* str) {
    int len;
    wchar_t* wideStr = charToWchar(str, &len);

    for(int i = 0; i < len; i++) {
        if(wideStr[i] == L'/') {
            wideStr[i] = L'\\';
        }
    }

    return wideStr;
}

char* wcharToChar(const wchar_t* str, size_t* lenPtr) {
    size_t len = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
    char* buf = ArenaAlloc(sizeof(char) * len);
    WideCharToMultiByte(CP_ACP, 0, str, -1, buf, len, NULL, NULL);

    if(lenPtr != NULL) {
        *lenPtr = len - 1;
    }

    return buf;
}

char* readFile(const char* name) {
    size_t _;
    return readFileLen(name, &_);
}

char* readFileLen(const char* name, size_t* len) {
    FILE* f = fopen(name, "r");

    if(f == NULL) {
        fprintf(stderr, "Could not read file: %s - %s\n", name, strerror(errno));
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char* buf = ArenaAlloc((size + 1) * sizeof(char));
    size_t read = fread(buf, sizeof(char), size, f);
    buf[read] = '\0';

    fclose(f);

    *len = read;

    return buf;
}

static bool createDirRecurse(wchar_t* path, size_t pathLen) {
    DWORD attrs = GetFileAttributesW(path);

    if(attrs == INVALID_FILE_ATTRIBUTES) {
        for(size_t i = pathLen - 2; i > 4; i--) {
            if(path[i] == L'\\') {
                wchar_t character = path[i + 1];
                path[i + 1] = L'\0';
                createDirRecurse(path, i);
                path[i + 1] = character;
                break;
            }
        }

        return CreateDirectoryW(path, NULL);
    } else {
        return true;
    }
}

// note: only compatable with UNC \\?\ paths that are slash terminated
bool deepCreateDirectory(wchar_t* path) {
    size_t len = wcslen(path);
    if(wcsncmp(path, L"\\\\?\\", 4) != 0) return false;
    if(path[len - 1] != L'\\') return false;
    return createDirRecurse(path, len);
}

// create file and all previous directories in the path
// returns write only syncronous handle to the file
// if the file already exists, it is overwritten.
HANDLE deepCreateFile(wchar_t* path) {
    size_t len = wcslen(path);

    for(size_t i = len - 1; i >= 1; i--) {
        if(path[i] == L'\\') {
            wchar_t character = path[i + 1];
            path[i + 1] = L'\0';
            deepCreateDirectory(path);
            path[i + 1] = character;
            break;
        }
    }

    return CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FLAG_ATTRIBUTE_NORMAL, NULL);
}

// recursively delete all files and folders in a directory
// errors if read-only files are found.
// no error if the provided directory does not exist
bool deepDeleteDirectory(wchar_t* path) {
    wchar_t* searchPath;
    PathAllocCombine(path, TEXT("*"), PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH, &searchPath);

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(searchPath, &ffd);

    if(hFind == INVALID_HANDLE_VALUE) {
        // not a problem if the directory already doesnt exist
        LocalFree(searchPath);
        return true;
    }

    bool succeeded = true;

    do {
        if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if(wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
                continue;
            }
            wchar_t* newDir;
            PathAllocCombine(path, ffd.cFileName, PathFlags, &newDir);
            succeeded &= deepDeleteDirectory(newDir);
            if(!succeeded) return false;
            succeeded &= RemoveDirectoryW(newDir);
            if(!succeeded) return false;
            LocalFree(newDir);

            if(!succeeded) {
                return false;
            }

        } else {
            wchar_t* newPath;
            PathAllocCombine(path, ffd.cFileName, PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH, &newPath);

            succeeded &= DeleteFileW(newPath);
            LocalFree(newPath);
            if(!succeeded) return false;
        }
    } while(FindNextFile(hFind, &ffd) != 0);

    DWORD error = GetLastError();
    if(error != ERROR_NO_MORE_FILES) {
        LocalFree(searchPath);
        return false;
    }

    LocalFree(searchPath);
    FindClose(hFind);
    return succeeded;
}
