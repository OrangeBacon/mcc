#include "test.h"

#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define UNICODE
#include <windows.h>
#include <pathcch.h>

#include "file.h"

#define PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH 0x00000010
#define PATHCCH_ENSURE_TRAILING_SLASH 0x00000020
#define LongPath PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH
#define SlashPath PATHCCH_ENSURE_TRAILING_SLASH

// Simple end-to-end tests.

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

typedef struct testCtx {
    ARRAY_DEFINE(char*, path);
} testCtx;

// check if a file could be a test (i.e. has ".har" extension)
// if valid add to an array
// length = length of the path supplied, on windows can be up to 32k, i.e.
// greater than max path potentially, as using \\?\ paths
static void runSingleTest(char* path, size_t length, void* voidCtx) {
    testCtx* ctx = voidCtx;

    if(length <= 4 || strncmp(path + length - 4, ".har", 4) != 0) {
        return;
    }

    ARRAY_PUSH(*ctx, path, path);
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
    ARRAY_ALLOC(char*, ctx, path);

    bool result = iterateDirectory(folderPath, runSingleTest, &ctx);

    if(result == false) {
        fwprintf(stderr, L"Finding tests failed. Does \"%s\" exist?\n", folderPath);
        LocalFree(folderPath);
        return EXIT_FAILURE;
    }

    LocalFree(folderPath);

    if(ctx.pathCount == 0) {
        fprintf(stderr, "Found no test files. Exiting.\n");
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "Found %d test%s:\n", ctx.pathCount, ctx.pathCount==1?"":"s");
    for(unsigned int i = 0; i < ctx.pathCount; i++) {
        fprintf(stderr, "\t%s\n", ctx.paths[i]);
    }

    return EXIT_SUCCESS;
}
