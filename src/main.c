#include <stdio.h>
#include <stdbool.h>

#include "parser.h"
#include "analysis.h"
#include "memory.h"
#include "astLower.h"
#include "ir.h"

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

    if(!parser.hadError) {
        Analyse(&parser);
    } else {
        return 1;
    }

    if(!parser.hadError) {
        MemoryPool pool;
        memoryPoolAlloc(&pool, 64ULL*GiB);
        IrContext ctx;
        IrContextCreate(&ctx, &pool);

        ASTPrint(parser.ast);
        astLower(parser.ast, &ctx);
        IrContextPrint(&ctx);
    } else {
        return 2;
    }
}