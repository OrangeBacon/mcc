#include <stdio.h>
#include <stdbool.h>

#include "parser.h"
#include "analysis.h"
#include "memory.h"
#include "x64.h"

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("File name argument required");
        return 1;
    }

    ArenaInit();

    printf("Compiling: %s\n", argv[1]);

    Parser parser;
    ParserInit(&parser, argv[1]);
    ParserRun(&parser);
    Analyse(&parser);

    if(!parser.hadError) {
        ASTPrint(parser.ast);
        x64ASTGen(parser.ast);
    }
}