#include "scanner.h"

#include "file.h"
#include "token.h"

void ScannerInit(Scanner* scanner, char* fileName) {
    scanner->fileName = fileName;
    scanner->text = readFile(fileName);
    scanner->start = scanner->text;
    scanner->current = scanner->text;
    scanner->line = 1;
    scanner->column = 0;
}

void ScannerNext(Scanner* scanner, Token* token) {
    (void)scanner;
    (void)token;
}