#ifndef FILE_H
#define FILE_H

#include <stddef.h>

char* readFile(const char* name);
char* readFileLen(const char* name, size_t* len);

#endif