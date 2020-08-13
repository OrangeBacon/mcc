#ifndef FILE_H
#define FILE_H

#include <stddef.h>
#include "memory.h"

typedef struct Path {
    wchar_t* buf;
    bool exists;
} Path;

typedef struct IncludeSearchPath {
    ARRAY_DEFINE(Path, system);
    ARRAY_DEFINE(Path, user);
} IncludeSearchPath;

typedef enum SystemType {
    SYSTEM_MINGW_W64 = 0x1,
    SYSTEM_MSVC = 0x2,
    SYSTEM_WINDOWS = SYSTEM_MINGW_W64 | SYSTEM_MSVC,
} SystemType;

void FilesInit();
void IncludeSearchPathInit(IncludeSearchPath* search, SystemType type, const char** includePaths, size_t includeCount);


char* readFile(const char* name);
char* readFileLen(const char* name, size_t* len);

#endif