#include <stdio.h>
#include <stdbool.h>

#include "scanner.h"
#include "token.h"
#include "parser.h"
#include "memory.h"
#include "x64.h"

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("File name argument required");
        return 1;
    }

    ArenaInit();

    printf("Compiling: %s\n", argv[1]);

    Scanner scanner;
    ScannerInit(&scanner, argv[1]);

    Parser parser;
    ParserInit(&parser, &scanner);

    ParserRun(&parser);

    ASTPrint(parser.ast);
    x64ASTGen(parser.ast);
}