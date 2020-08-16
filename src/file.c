#include "file.h"

#include <stdio.h>
#include <stdlib.h>

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

// common flags for path processing
#define PathFlags (PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH | PATHCCH_ENSURE_TRAILING_SLASH)

// Initialise the file system wrapper
void FilesInit() {
    unsigned long len = GetCurrentDirectoryW(0, NULL) + 1;
    wchar_t* buf = malloc(len * sizeof(wchar_t));
    GetCurrentDirectoryW(len, buf);

    HRESULT r = PathAllocCanonicalize(buf, PathFlags, &currentDirectory.buf);
    free(buf);
    if(r != S_OK) {
        printf("Error getting current directory\n");
        exit(0);
    }
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

    wchar_t* mingwInclude;
    PathAllocCombine(mingwVersion, TEXT("mingw64\\x86_64-w64-mingw32\\include"), PathFlags, &mingwInclude);
    ARRAY_PUSH(*search, system, ((Path){mingwInclude, true}));

    wchar_t* libgcc;
    PathAllocCombine(mingwVersion, TEXT("mingw64\\lib\\gcc\\x86_64-w64-mingw32"), PathFlags, &libgcc);
    LocalFree(mingwVersion);

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
}

// try adding Chocolatey's install directories, used by github actions
static void FindMinGWW64Chocolatey(IncludeSearchPath* search) {
    wchar_t* programData;
    SHGetKnownFolderPath(&FOLDERID_ProgramData, 0, NULL, &programData);

    wchar_t* mingwFolder;
    PathAllocCombine(programData, TEXT("Chocolatey\\lib\\mingw\\tools\\install\\mingw64"), PathFlags, &mingwFolder);
    CoTaskMemFree(programData);

    wchar_t* mingwInclude;
    PathAllocCombine(mingwFolder, TEXT("x86_64-w64-mingw32\\include"), PathFlags, &mingwInclude);
    ARRAY_PUSH(*search, system, ((Path){mingwInclude, true}));

    wchar_t* libgcc;
    PathAllocCombine(mingwFolder, TEXT("lib\\gcc\\x86_64-w64-mingw32"), PathFlags, &libgcc);
    LocalFree(mingwFolder);

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

    (void) search;
}

// adds all values from %path% to the search path
static void FindPath(IncludeSearchPath* search) {
    DWORD len = GetEnvironmentVariableW(TEXT("path"), NULL, 0);
    if(len == 0) {
        return;
    }

    wchar_t* path = malloc(sizeof(wchar_t) * (len + 1));
    if(path == 0) {
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
        wchar_t* buf = malloc(sizeof(wchar_t) * len);
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, includePaths[i], -1, buf, len);

        if(buf[0] == '-') {
            wchar_t* path;
            PathAllocCombine(currentDirectory.buf, buf + 1, PathFlags, &path);
            ARRAY_PUSH(*search, system, ((Path){path, true}));
        } else {
            wchar_t* path;
            PathAllocCombine(currentDirectory.buf, buf, PathFlags, &path);
            ARRAY_PUSH(*search, user, ((Path){path, true}));
        }
        free(buf);
    }
}

// removes some include paths
// - ones that do not exist
// - ones that contain `bin\` in their path - likely to be binary, not
//   source files
static void FilterPaths(Path* list, size_t count, bool filterBin) {
    for(unsigned int i = 0; i < count; i++) {
        Path* path = &list[i];
        if(!(path->exists = PathFileExistsW(path->buf))) {
            continue;
        }

        if(!filterBin) continue;

        wchar_t* ptr = path->buf;
        while(true) {
            ptr++;
            if(*ptr == '\0') break;
            if(_wcsnicmp(TEXT("bin\\"), ptr, 4) == 0) {
                path->exists = false;
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

    fprintf(stderr, "sys count: %d\n", search->systemCount);
    for(unsigned int i = 0; i < search->systemCount; i++) {
        if(!search->systems[i].exists) continue;
        fprintf(stderr, "sys %d: %ls\n", i, search->systems[i].buf);
    }
    fprintf(stderr, "user count: %d\n", search->userCount);
    for(unsigned int i = 0; i < search->userCount; i++) {
        if(!search->users[i].exists) continue;
        fprintf(stderr, "user %d: %ls\n", i, search->users[i].buf);
    }
}

/*
void pathFindSys() {

}

void pathFindUser() {
    if(!found) {
        pathFindSys();
    }
}*/

char* readFile(const char* name) {
    size_t _;
    return readFileLen(name, &_);
}

char* readFileLen(const char* name, size_t* len) {
    FILE* f = fopen(name, "r");

    if(f == NULL) {
        printf("Could not read file: %s\n", name);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char* buf = malloc((size + 1) * sizeof(char));
    size_t read = fread(buf, sizeof(char), size, f);
    buf[read] = '\0';

    *len = read;

    return buf;
}
