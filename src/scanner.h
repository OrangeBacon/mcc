#ifndef SCANNER_H
#define SCANNER_H

#include "token.h"

typedef struct Scanner {
    char* fileName;
    char* text;
    char* start;
    char* current;
    int line;
    int column;
} Scanner;

void ScannerInit(Scanner* scanner, char* fileName);
void ScannerNext(Scanner* scanner, Token* token);

#endif