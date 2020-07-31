#include <stdio.h>
#include <stdbool.h>

#include "argParser.h"
#include "parser.h"
#include "analysis.h"
#include "memory.h"
#include "astLower.h"
#include "ir.h"

struct stringList files = {0};
bool printAst = false;
bool printIr = false;

struct argArgument topArguments[] = {
    {"!input", '\0', "file to process", argPush, &files},
    {"-print-ast", 'a', "prints the ast to stdout", argSet, &printAst},
    {"-print-ir", 'i', "prints the ir to stdout", argSet, &printIr},
    {0},
};

int main(int argc, char** argv) {
    ArenaInit();

    struct argParser argparser = {
        argc - 1,
        argv + 1,
        topArguments,
    };
    bool hadError = parseArgs(&argparser);
    if(hadError) return 1;

    for(unsigned int i = 0; i < files.dataCount; i++) {
        Parser parser;
        ParserInit(&parser, (char*)files.datas[i]);
        ParserRun(&parser);

        if(!parser.hadError) {
            Analyse(&parser);
        } else {
            hadError = true;
            continue;
        }

        if(!parser.hadError) {
            MemoryPool pool;
            memoryPoolAlloc(&pool, 64ULL*GiB);
            IrContext ctx;
            IrContextCreate(&ctx, &pool);

            if(printAst) ASTPrint(parser.ast);
            astLower(parser.ast, &ctx);
            if(printIr) IrContextPrint(&ctx);
        } else {
            hadError = true;
        }
    }

    return hadError ? 2 : 0;
}