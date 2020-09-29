#include "test.h"

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

typedef void (*directoryIterCallback)(wchar_t* path, wchar_t* file, void* ctx);

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
            callback(basePath, ffd.cFileName, ctx);
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
    int testCount;
} testCtx;

static void runSingleTest(wchar_t* path, wchar_t* file, void* voidCtx) {
    testCtx* ctx = voidCtx;
    fwprintf(stderr, L"%s%s\n", path, file);
    ctx->testCount++;
}

int runTests(const char* testPath) {
    fprintf(stderr, "running tests: \n");
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

    testCtx ctx = {0};

    bool result = iterateDirectory(folderPath, runSingleTest, &ctx);
    LocalFree(folderPath);

    if(result == false) {
        fprintf(stderr, "finding tests failed\n");
    }

    fprintf(stderr, "Found %d tests\n", ctx.testCount);

    return result;
}
