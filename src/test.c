#include "test.h"

#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>
#include <inttypes.h>

#define UNICODE
#include <windows.h>
#include <pathcch.h>

#include "file.h"

#define PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH 0x00000010
#define PATHCCH_ENSURE_TRAILING_SLASH 0x00000020
#define FLAG_ATTRIBUTE_NORMAL 0x80
#define LongPath PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH
#define SlashPath PATHCCH_ENSURE_TRAILING_SLASH

#define stdOutFileName "processOut.txt"
#define stdErrFileName "processErr.txt"

// Simple? end-to-end tests.
// note: This most definitely uses the Win32 api, good luck to myself
// when attempting to port it to a non-windows system!

typedef struct testDescriptor {
    char* path;
    bool succeeded;
    const char* testNamePath;
} testDescriptor;

typedef struct testCtx {
    ARRAY_DEFINE(testDescriptor, test);
    size_t basePathLen;
} testCtx;

typedef void (*directoryIterCallback)(char* path, size_t length, void* ctx);

// recursively iterate directory, calling the call back with the absolute
// path of every valid file within the base path.
// returns false if a file system error occured (e.g. file not found)
static bool iterateDirectory(wchar_t* basePath, directoryIterCallback callback, void* ctx) {
    wchar_t* searchPath;
    PathAllocCombine(basePath, TEXT("*"), LongPath, &searchPath);

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(searchPath, &ffd);

    if(hFind == INVALID_HANDLE_VALUE) {
        LocalFree(searchPath);
        return false;
    }

    bool succeeded = true;

    do {
        if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if(wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
                continue;
            }
            wchar_t* newDir;
            PathAllocCombine(basePath, ffd.cFileName, LongPath | SlashPath, &newDir);
            succeeded &= iterateDirectory(newDir, callback, ctx);
            LocalFree(newDir);

        } else {
            wchar_t* path;
            PathAllocCombine(basePath, ffd.cFileName, LongPath, &path);

            // remove wchar_t from callback
            size_t len;
            char* buf = wcharToChar(path, &len);
            callback(buf, len, ctx);
            LocalFree(path);
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

// check if a file could be a test (i.e. has ".har" extension)
// if valid add to an array
// length = length of the path supplied, on windows can be up to 32k, i.e.
// greater than max path potentially, as using \\?\ paths
static void gatherTests(char* path, size_t length, void* voidCtx) {
    testCtx* ctx = voidCtx;

    if(length <= 4 || strncmp(path + length - 4, ".har", 4) != 0) {
        return;
    }

    ARRAY_PUSH(*ctx, test, ((testDescriptor){
        .path = path,
        .succeeded = false,
        .testNamePath = path + ctx->basePathLen,
    }));
}

// a single section of a har file
typedef struct harSingleFile {
    bool isDrectory;
    unsigned char* path;
    unsigned char* content;
    size_t contentLength;
    int expectedExitCodeParam;
    bool trimTrailingWhitespace;
    int timeout;
} harSingleFile;

// a whole har file and related parsing context
typedef struct harContext {
    testDescriptor* test;
    size_t fileLength;
    unsigned char* file;
    size_t consumed;
    size_t line;
    size_t column;
    ARRAY_DEFINE(harSingleFile, file);
    unsigned char* seperator;
    size_t seperatorLength;
    wchar_t* basePath;
} harContext;

static bool isEOF(harContext* ctx) {
    return ctx->consumed >= ctx->fileLength;
}

// peek and advance fold \n, \r, \r\n, \n\r into \n
static unsigned char peek(harContext* ctx) {
    if(isEOF(ctx)) return '\0';

    unsigned char val = ctx->file[ctx->consumed];
    if(val == '\r') return '\n';
    return val;
}

// consume the next character from the stream and return it.
// updates the line and column counters
// treats '\r\n' and '\n\r' and '\r' as all equal to a return of '\n'
static unsigned char advance(harContext* ctx) {
    if(isEOF(ctx)) return '\0';
    unsigned char val = ctx->file[ctx->consumed];

    ctx->consumed++;
    ctx->column++;
    if(val == '\n' || val == '\r') {
        ctx->line++;
        ctx->column = 1;
        if(!isEOF(ctx)) {
            unsigned char next = ctx->file[ctx->consumed];
            if(val == '\n' && next == '\r') ctx->consumed++;
            if(val == '\r' && next == '\n') ctx->consumed++;
        }
    }

    return val;
}

// advance by n characters
static void advanceN(harContext* ctx, size_t n) {
    for(size_t i = 0; i < n; i++) advance(ctx);
}

// skip spaces and tabs in the context
static void skipWhitespace(harContext* ctx) {
    while(!isEOF(ctx) && (peek(ctx) == ' ' || peek(ctx) == '\t')) {
        advance(ctx);
    }
}

typedef enum propertyState {
    PROPERTY_NOT_FOUND,
    PROPERTY_FAILED,
    PROPERTY_SUCCEEDED,
} propertyState;

// parse header property <name> = <signed integer value>
static propertyState parseIntProperty(harContext* ctx, int* property, const char* name) {
    size_t len = strlen(name);
    if(strncmp((char*)&ctx->file[ctx->consumed], name, len) == 0) {
        advanceN(ctx, len); // consume name
        skipWhitespace(ctx);
        if(peek(ctx) != '=') {
            return PROPERTY_FAILED;
        }
        advance(ctx); // consume '='
        skipWhitespace(ctx);
        unsigned char* end;
        intmax_t num = strtoimax((char*)&ctx->file[ctx->consumed], (char**)&end, 0);

        if(num > INT_MAX || (errno == ERANGE && num == INTMAX_MAX)) {
            return PROPERTY_FAILED;
        }
        if(num < INT_MIN || (errno == ERANGE && num == INTMAX_MIN)) {
            return PROPERTY_FAILED;
        }
        *property = num;
        advanceN(ctx, end - &ctx->file[ctx->consumed]);

        return PROPERTY_SUCCEEDED;
    }

    return PROPERTY_NOT_FOUND;
}

// parse the header to a file section in the har file
// examples:
//  main.c
//  "spaces file name.c"
//  cmd exit=15
//  main.c -other seperator stuff ignored till the end of the line
static bool parseFileHeader(harContext* ctx, harSingleFile* file) {
    file->path = &ctx->file[ctx->consumed];
    file->expectedExitCodeParam = 0;
    file->timeout = 0;
    file->trimTrailingWhitespace = false;
    size_t pathLength = 0;

    char seperator = file->path[0] == '"' ? '"' : ' ';
    while(!isEOF(ctx) && peek(ctx) != '\n' && peek(ctx) != seperator) {
        advance(ctx);
        pathLength++;
    }

    if(peek(ctx) != seperator && peek(ctx) != '\n') {
        return false;
    }

    if(file->path[pathLength - 1] == '/') {
        file->isDrectory = true;
    } else {
        file->isDrectory = false;
    }

    while(!isEOF(ctx)) {
        skipWhitespace(ctx);

        // end of header line
        if(peek(ctx) == '\n') break;

        // end of line seperator
        if(peek(ctx) == ctx->seperator[0]) {
            while(!isEOF(ctx) && peek(ctx) != '\n') {
                advance(ctx);
            }
            break;
        }

        // exit property - used to specify command expected exit codes
        propertyState s = parseIntProperty(ctx, &file->expectedExitCodeParam, "exit");
        if(s == PROPERTY_FAILED) {
            return false;
        } else if(s == PROPERTY_SUCCEEDED) {
            continue;
        }

        // timeout property - used to specify command maximum timeout in milliseconds,
        // if timeout == -1, infinite used, if timeout == 0, default used
        s = parseIntProperty(ctx, &file->timeout, "timeout");
        if(s == PROPERTY_FAILED) {
            return false;
        } else if(s == PROPERTY_SUCCEEDED) {
            continue;
        }

        // trim-trailing-whitespace property - if specified, '\n', ' ' and '\t'
        // at the end of the file (not line) will be removed before storage
        if(strncmp((char*)&ctx->file[ctx->consumed], "trim-trailing-whitespace", 24) == 0) {
            advanceN(ctx, 24);
            file->trimTrailingWhitespace = true;
            continue;
        }

        return false;
    }

    if(peek(ctx) == '\n') advance(ctx);

    // Make the path a null terminated string.  This edits the har file's
    // buffer, however the area edited will have already been read by this
    // point, or an error will have occured and the function will have returned
    // so this is safe to do.
    file->path[pathLength] = '\0';

    return true;
}

// parse all of a file section in the har archive
static bool parseHarFileSection(harContext* ctx) {
    if(strncmp((char*)&ctx->file[ctx->consumed], (char*)ctx->seperator, ctx->seperatorLength) != 0) {
        return false;
    }
    advanceN(ctx, ctx->seperatorLength);
    skipWhitespace(ctx);

    harSingleFile* file = ARRAY_PUSH_PTR(*ctx, file);
    if(!parseFileHeader(ctx, file)) {
        return false;
    }

    if(file->isDrectory) {
        return true;
    }

    file->content = &ctx->file[ctx->consumed];
    file->contentLength = 0;
    while(!isEOF(ctx)) {
        if(strncmp((char*)&ctx->file[ctx->consumed], (char*)ctx->seperator, ctx->seperatorLength) == 0) {
            break;
        }
        while(!isEOF(ctx) && advance(ctx) != '\n') {
            file->contentLength++;
        }
        file->contentLength++;
    }

    // apply trim-trailing-whitespace modifier if required
    if(file->trimTrailingWhitespace) {
        // reverse file iteration from file->contentLength-1 downto 0 both inclusive
        for(size_t i = file->contentLength; i-- > 0;) {
            char current = file->content[i];
            if(current == '\n' || current == ' ' || current == '\t') {
                file->contentLength--;
            } else {
                break;
            }
        }
    }

    return true;
}

// extract har file onto the file systems
static bool createDirectoryFromTest(harContext* ctx, const char* tempPath) {
    wchar_t* longTempPath = pathToWchar(tempPath);
    wchar_t* longNamePath = pathToWchar(ctx->test->testNamePath);
    wchar_t* path;
    PathAllocCombine(longTempPath, longNamePath, LongPath | SlashPath, &path);
    bool result = deepCreateDirectory(path);
    ctx->basePath = path;

    // for each file in the archive
    for(unsigned int i = 0; i < ctx->fileCount; i++) {
        harSingleFile* harFile = &ctx->files[i];
        wchar_t* filePath;
        PathAllocCombine(path, pathToWchar((char*)harFile->path), LongPath, &filePath);

        if(harFile->isDrectory) {
            if(!deepCreateDirectory(filePath)) {
                return false;
            }
        } else {
            HANDLE hFile = deepCreateFile(filePath);
            LocalFree(filePath);

            DWORD bytesWritten;
            if(!WriteFile(hFile, harFile->content, harFile->contentLength, &bytesWritten, NULL)) {
                CloseHandle(hFile);
                return false;
            }
            if(bytesWritten != harFile->contentLength) {
                CloseHandle(hFile);
                return false;
            }
            CloseHandle(hFile);
        }
    }

    return result;
}

static harSingleFile* findFile(harContext* ctx, const char* name) {
    for(unsigned int i = 0; i < ctx->fileCount; i++) {
        harSingleFile* file = &ctx->files[i];
        if(strcmp(name, (char*)file->path) == 0) {
            return file;
        }
    }
    return NULL;
}

// Workaround GetModuleFileNameW - if the buffer is not big enough
// the call truncates the result and returns how much was written,
// so there is no way of getting the correct buffer length required.
// This repeatedly calls it with bigger buffers until one is used that
// is long enough to store all the characters.
static wchar_t* getCurrentExecutableName() {
    unsigned int bufLen = 256;

    while(bufLen < 32768) {
        wchar_t* buf = ArenaAlloc(sizeof(wchar_t) * bufLen);

        unsigned int retLen = GetModuleFileNameW(NULL, buf, bufLen);

        // call failed
        if(retLen == 0) {
            return NULL;
        } else if(retLen <= bufLen) {
            return buf;
        }
        bufLen <<= 1;
    }

    return NULL;
}

// create temporary file that will be deleted when its handle is closed
// the handle is inheritable, shareable, readable and writable
static bool createTempFile(wchar_t* folder, HANDLE* out) {
    wchar_t fileName[MAX_PATH];
    if(!GetTempFileNameW(folder, TEXT("mcctest"), 0, fileName)) {
        return false;
    }

    // allow handle inheritance
    SECURITY_ATTRIBUTES secAttr;
    secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAttr.bInheritHandle = true;
    secAttr.lpSecurityDescriptor = NULL;

    HANDLE file = CreateFileW(fileName,
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        &secAttr, // inherited handle
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
        NULL); // no template

    *out = file;
    return file != INVALID_HANDLE_VALUE;
}

static bool createChildProcess(harContext* ctx) {

    HANDLE childOut = INVALID_HANDLE_VALUE;
    HANDLE childErr = INVALID_HANDLE_VALUE;
    HANDLE childIn = INVALID_HANDLE_VALUE;

    if(!createTempFile(ctx->basePath, &childOut)) {
        return false;
    }
    if(!createTempFile(ctx->basePath, &childErr)) {
        CloseHandle(childOut);
        return false;
    }

    harSingleFile* stdinFile = findFile(ctx, "stdin");
    if(stdinFile != NULL) {
        if(!createTempFile(ctx->basePath, &childIn)) {
            CloseHandle(childOut);
            CloseHandle(childErr);
            return false;
        }
        DWORD bytesWritten;
        bool success = WriteFile(childIn, stdinFile->content,
            stdinFile->contentLength, &bytesWritten, NULL);
        if(!success || bytesWritten != stdinFile->contentLength) {
            CloseHandle(childOut);
            CloseHandle(childErr);
            CloseHandle(childIn);
            return false;
        }
    }


    // create command line arguments
    wchar_t* wideProgramName = getCurrentExecutableName();

    harSingleFile* commandFile = findFile(ctx, "cmd");
    wchar_t* commandFileContent = charToWchar((char*)commandFile->content, NULL);

    size_t len = 1 + wcslen(wideProgramName) + 2 + wcslen(commandFileContent) + 1;
    wchar_t* wideCommandLine = ArenaAlloc(len * sizeof(wchar_t));
    wideCommandLine[0] = L'\0';
    wcscat(wideCommandLine, L"\"");
    wcscat(wideCommandLine, wideProgramName);
    wcscat(wideCommandLine, L"\" ");
    wcscat(wideCommandLine, commandFileContent);

    // windows metadata infomation, specifies the previously created
    // stdin/stdout handles that should be used
    PROCESS_INFORMATION procInfo = {0};
    STARTUPINFOW startupinfo = {0};
    startupinfo.cb = sizeof(STARTUPINFOW);
    startupinfo.hStdError = childErr;
    startupinfo.hStdOutput = childOut;
    startupinfo.hStdInput = childIn;
    startupinfo.dwFlags |= STARTF_USESTDHANDLES;

    bool success = CreateProcessW(
        wideProgramName, // executable file path
        wideCommandLine, // command line arguments, space seperated
        NULL, // process security
        NULL, // thread security
        true, // handle inheritance
        0, // no flags specified
        NULL, // no environment variable changes
        ctx->basePath, // working directory
        &startupinfo, // standard handle redirection, etc.
        &procInfo); // info about the process

    if(!success) {
        DWORD err = GetLastError();
        LPTSTR buffer;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buffer, 0, NULL);

        fprintf(stderr, "CreateProcess: %ls\n", buffer);
        LocalFree(buffer);

        return false;
    }

    int timeout = 10000;
    if(commandFile->timeout == -1) timeout = INFINITE;
    if(commandFile->timeout != 0) timeout = commandFile->timeout;

    // default timeout = 10 seconds
    DWORD result = WaitForSingleObject(procInfo.hProcess, timeout);
    if(result == WAIT_TIMEOUT) {
        fprintf(stderr, "test timedout\n");
        return false;
    } else if(result != WAIT_OBJECT_0) {
        return false;
    }

    DWORD exitCode;
    if(!GetExitCodeProcess(procInfo.hProcess, &exitCode)) {
        return false;
    }

    if(exitCode != (DWORD)commandFile->expectedExitCodeParam) {
        fprintf(stderr, "test unexpected exit code\n");
        return false;
    }

    CloseHandle(procInfo.hProcess);
    CloseHandle(procInfo.hThread);

    char fileData[BUFSIZ];
    wchar_t* fileName;

    // copy output temp file to real file (todo: only run if errored)
    PathAllocCombine(ctx->basePath, TEXT(stdOutFileName), LongPath, &fileName);
    HANDLE outputFile = CreateFileW(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(!SetFilePointerEx(childOut, (LARGE_INTEGER){.QuadPart=0}, NULL, FILE_BEGIN)) {
        return false;
    }
    while(true) {
        DWORD bytesRead;
        if(!ReadFile(childOut, fileData, BUFSIZ, &bytesRead, NULL)) break;
        if(bytesRead == 0) break;
        DWORD bytesWritten;
        if(!WriteFile(outputFile, fileData, bytesRead, &bytesWritten, NULL)) {
            return false;
        }
        if(bytesWritten != bytesRead) return false;
    }
    CloseHandle(outputFile);

    // same as above but for stderr
    PathAllocCombine(ctx->basePath, TEXT(stdErrFileName), LongPath, &fileName);
    HANDLE errFile = CreateFileW(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(!SetFilePointerEx(childErr, (LARGE_INTEGER){.QuadPart=0}, NULL, FILE_BEGIN)) {
        return false;
    }
    while(true) {
        DWORD bytesRead;
        if(!ReadFile(childErr, fileData, BUFSIZ, &bytesRead, NULL)) break;
        if(bytesRead == 0) break;
        DWORD bytesWritten;
        if(!WriteFile(errFile, fileData, bytesRead, &bytesWritten, NULL)) {
            return false;
        }
        if(bytesWritten != bytesRead) return false;
    }
    CloseHandle(errFile);

    CloseHandle(childOut);
    CloseHandle(childErr);
    if(childIn != INVALID_HANDLE_VALUE) CloseHandle(childIn);

    return true;
}

// test files are based on the human archive format
// see https://github.com/marler8997/har
static void runSingleTest(testDescriptor* test, const char* tempPath) {
    fprintf(stderr, "\t%s\n", test->path);

    harContext ctx = {.column = 1, .line = 1, .test = test};
    ctx.file = (unsigned char*)readFileLen(test->path, &ctx.fileLength);
    ARRAY_ALLOC(harSingleFile, ctx, file);

    // detect length of the file's seperator, without consuming it from the stream
    ctx.seperator = ctx.file;
    while(ctx.seperatorLength < ctx.fileLength && ctx.file[ctx.seperatorLength] != ' ') {
        ctx.seperatorLength++;
    }

    while(!isEOF(&ctx)) {
        if(!parseHarFileSection(&ctx)) {
            fprintf(stderr, "\t\ttest parsing failed at %lld:%lld\n", ctx.line, ctx.column);
            return;
        }
    }

    if(findFile(&ctx, stdOutFileName)) {
        fprintf(stderr, "\t\ttest file cannot specify files named \""stdOutFileName"\"\n");
        return;
    }
    if(findFile(&ctx, stdErrFileName)) {
        fprintf(stderr, "\t\ttest file cannot specify files named \""stdErrFileName"\"\n");
        return;
    }

    for(unsigned int i = 0; i < ctx.fileCount; i++) {
        harSingleFile* file = &ctx.files[i];
        file->content[file->contentLength] = '\0';
    }

    if(!createDirectoryFromTest(&ctx, tempPath)) {
        fprintf(stderr, "\t\ttest directory setup failed\n");
        return;
    }

    for(unsigned int i = 0; i < ctx.fileCount; i++) {
        harSingleFile* file = &ctx.files[i];
        fprintf(stderr, "\t\t%s", file->path);
        if(file->expectedExitCodeParam != 0) {
            fprintf(stderr, " exit = %d", file->expectedExitCodeParam);
        }
        if(file->isDrectory) {
            fprintf(stderr, " directory");
        }
        fprintf(stderr, "\n");
    }

    createChildProcess(&ctx);
    LocalFree(ctx.basePath);
}

// main function, return exits the program
int runTests(const char* testPath, const char* tempPath) {

    // append startup directory, so not relative to working directory, incase
    // changed somewhere else.
    Path startupDirectory = getStartupDirectory();
    wchar_t* folderPath;
    PathAllocCombine(startupDirectory.buf, pathToWchar(testPath), LongPath | SlashPath, &folderPath);

    testCtx ctx = {0};
    wcharToChar(folderPath, &ctx.basePathLen);
    ARRAY_ALLOC(testDescriptor, ctx, test);

    bool result = iterateDirectory(folderPath, gatherTests, &ctx);

    if(result == false) {
        fwprintf(stderr, L"Finding tests failed. Does \"%s\" exist?\n", folderPath);
        LocalFree(folderPath);
        return EXIT_FAILURE;
    }

    LocalFree(folderPath);

    if(ctx.testCount == 0) {
        fprintf(stderr, "Found no test files. Exiting.\n");
        return EXIT_SUCCESS;
    }

    wchar_t* fullTempPath;
    PathAllocCombine(startupDirectory.buf, pathToWchar(tempPath), LongPath | SlashPath, &fullTempPath);
    if(!deepDeleteDirectory(fullTempPath)) {
        fprintf(stderr, "Failed to empty test directory.\n");
        LocalFree(fullTempPath);
        return EXIT_FAILURE;
    }

    if(!deepCreateDirectory(fullTempPath)) {
        fprintf(stderr, "Failed to create test directory.\n");
        LocalFree(fullTempPath);
        return EXIT_FAILURE;
    }
    const char* charTempPath = wcharToChar(fullTempPath, NULL);
    LocalFree(fullTempPath);

    fprintf(stderr, "Executing %d test%s:\n", ctx.testCount, ctx.testCount==1?"":"s");
    for(unsigned int i = 0; i < ctx.testCount; i++) {
        // TODO: parallelise this
        runSingleTest(&ctx.tests[i], charTempPath);
    }

    return EXIT_SUCCESS;
}
