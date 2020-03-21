#include "file.h"

#include <stdio.h>
#include <stdlib.h>

char* readFile(const char* name) {
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

    return buf;
}