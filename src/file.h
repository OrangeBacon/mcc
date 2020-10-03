#ifndef FILE_H
#define FILE_H

#include <stddef.h>
#include "memory.h"

typedef struct Path {
    wchar_t* buf;
    bool valid;
} Path;

typedef struct IncludeSearchPath {
    ARRAY_DEFINE(Path, system);
    ARRAY_DEFINE(Path, user);
} IncludeSearchPath;

// will need to implement #include_next at some point
// - used in mingw include files
typedef struct IncludeSearchState {
    bool hasStarted;
    bool inUser;
    size_t checkedCount;
} IncludeSearchState;

typedef enum SystemType {
    SYSTEM_MINGW_W64 = 0x1,
    SYSTEM_MSVC = 0x2,
    SYSTEM_WINDOWS = SYSTEM_MINGW_W64 | SYSTEM_MSVC,
} SystemType;

void FilesInit();
Path getStartupDirectory();
void IncludeSearchPathInit(IncludeSearchPath* search, SystemType type, const char** includePaths, size_t includeCount);
const char* IncludeSearchPathFindSys(IncludeSearchState* state, IncludeSearchPath* path, const char* fileName);
const char* IncludeSearchPathFindUser(IncludeSearchState* state, IncludeSearchPath* path, const char* fileName);

wchar_t* charToWchar(const char* str, int* lenPtr);
wchar_t* pathToWchar(const char* str);
char* wcharToChar(const wchar_t* str, size_t* lenPtr);

char* readFile(const char* name);
char* readFileLen(const char* name, size_t* len);

bool deepCreateDirectory(wchar_t* path);
void* deepCreateFile(wchar_t* path);
bool deepDeleteDirectory(wchar_t* path);

#endif