#include <stdio.h>
#include <stdbool.h>

#include "scanner.h"
#include "token.h"

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("File name argument required");
        return 1;
    }

    printf("Compiling: %s\n", argv[1]);

    Scanner scanner;
    ScannerInit(&scanner, argv[1]);

    int line = -1;
    while(true) {
        Token token;
        ScannerNext(&scanner, &token);
        
        if(token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }

        printf(":%2d %10s '%.*s'\n", token.column, 
            TokenTypeToString(token.type), token.length, token.start);

        if(token.type == TOKEN_EOF) break;
    }
}