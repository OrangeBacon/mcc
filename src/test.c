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
#define LongPath PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH
#define SlashPath PATHCCH_ENSURE_TRAILING_SLASH

// Simple? end-to-end tests.

typedef struct testDescriptor {
    char* path;
    bool succeeded;
} testDescriptor;

typedef struct testCtx {
    ARRAY_DEFINE(testDescriptor, test);
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
            if(_wcsicmp(ffd.cFileName, L".") == 0 || _wcsicmp(ffd.cFileName, L"..") == 0) {
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
            size_t len = WideCharToMultiByte(CP_ACP, 0, path, -1, NULL, 0, NULL, NULL);
            char* buf = ArenaAlloc(sizeof(char) * len);
            WideCharToMultiByte(CP_ACP, 0, path, -1, buf, len, NULL, NULL);

            callback(buf, len - 1, ctx);
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

    ARRAY_PUSH(*ctx, test, ((testDescriptor){.path = path, .succeeded = false}));
}

// a single section of a har file
typedef struct harSingleFile {
    bool isDrectory;
    const unsigned char* path;
    size_t pathLength;
    const unsigned char* content;
    size_t contentLength;
    int expectedExitCodeParam;
} harSingleFile;

// a whole har file and related parsing context
typedef struct harContext {
    size_t fileLength;
    unsigned char* file;
    size_t consumed;
    size_t line;
    size_t column;
    ARRAY_DEFINE(harSingleFile, file);
    unsigned char* seperator;
    size_t seperatorLength;
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

// parse the header to a file section in the har file
// examples:
//  main.c
//  "spaces file name.c"
//  cmd exit=15
//  main.c -other seperator stuff ignored till the end of the line
static bool parseFileHeader(harContext* ctx, harSingleFile* file) {
    file->path = &ctx->file[ctx->consumed];
    file->pathLength = 0;

    char seperator = file->path[0] == '"' ? '"' : ' ';
    while(!isEOF(ctx) && peek(ctx) != '\n' && peek(ctx) != seperator) {
        advance(ctx);
        file->pathLength++;
    }

    if(peek(ctx) != seperator && peek(ctx) != '\n') {
        return false;
    }

    if(file->path[file->pathLength - 1] == '/') {
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
        if(strncmp((char*)&ctx->file[ctx->consumed], "exit", 4) == 0) {
            advanceN(ctx, 4); // consume 'exit'
            skipWhitespace(ctx);
            if(peek(ctx) != '=') {
                return false;
            }
            advance(ctx); // consume '='
            skipWhitespace(ctx);
            unsigned char* end;
            intmax_t num = strtoimax((char*)&ctx->file[ctx->consumed], (char**)&end, 0);

            if(num > INT_MAX || (errno == ERANGE && num == INTMAX_MAX)) {
                return false;
            }
            if(num < INT_MIN || (errno == ERANGE && num == INTMAX_MIN)) {
                return false;
            }
            file->expectedExitCodeParam = num;
            advanceN(ctx, end - &ctx->file[ctx->consumed]);
            continue;
        }

        return false;
    }

    if(peek(ctx) == '\n') advance(ctx);

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
    }

    return true;
}

// test files use the human archive format
// see https://github.com/marler8997/har
static void runSingleTest(testDescriptor* test) {
    fprintf(stderr, "\t%s\n", test->path);

    harContext ctx = {.column = 1, .line = 1};
    ctx.file = (unsigned char*)readFileLen(test->path, &ctx.fileLength);
    ARRAY_ALLOC(harSingleFile, ctx, file);

    // detect length of the file's seperator, without consuming it from the stream
    ctx.seperator = ctx.file;
    while(ctx.seperatorLength < ctx.fileLength && ctx.file[ctx.seperatorLength] != ' ') {
        ctx.seperatorLength++;
    }

    while(!isEOF(&ctx) && parseHarFileSection(&ctx)){}

    if(!isEOF(&ctx)) {
        fprintf(stderr, "\t\ttest parsing failed at %lld:%lld\n", ctx.line, ctx.column);
        return;
    }

    fprintf(stderr, "\t\tseplen = %lld\n", ctx.seperatorLength);

    for(unsigned int i = 0; i < ctx.fileCount; i++) {
        harSingleFile* file = &ctx.files[i];
        fprintf(stderr, "\t\t%.*s", (int)file->pathLength, file->path);
        if(file->expectedExitCodeParam != 0) {
            fprintf(stderr, " exit = %d", file->expectedExitCodeParam);
        }
        if(file->isDrectory) {
            fprintf(stderr, " directory");
        }
        fprintf(stderr, "\n");
    }
}

// main function, return exits the program
int runTests(const char* testPath) {
    // command line input is not wide char, so make it so
    int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, testPath, -1, NULL, 0);
    wchar_t* widePath = ArenaAlloc(sizeof(wchar_t) * len);
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, testPath, -1, widePath, len);

    for(int i = 0; i < len; i++) {
        if(widePath[i] == L'/') {
            widePath[i] = L'\\';
        }
    }

    // append startup directory, so not relative to working directory, incase
    // changed somewhere else.
    Path startupDirectory = getStartupDirectory();
    wchar_t* folderPath;
    PathAllocCombine(startupDirectory.buf, widePath, LongPath | SlashPath, &folderPath);

    testCtx ctx;
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

    fprintf(stderr, "Executing %d test%s:\n", ctx.testCount, ctx.testCount==1?"":"s");
    for(unsigned int i = 0; i < ctx.testCount; i++) {
        // TODO: parallelise this
        runSingleTest(&ctx.tests[i]);
    }

    return EXIT_SUCCESS;
}
