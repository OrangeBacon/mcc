#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "argParser.h"
#include "parser.h"
#include "analysis.h"
#include "memory.h"
#include "astLower.h"
#include "ir.h"
#include "stream.h"
#include "file.h"

struct stringList files = {0};
bool printAst = false;
bool printIr = false;
int translationPhaseCount = 8;

static void preprocessFlag(struct argParser* parser, void* _) {
    (void)_;

    bool didError = false;
    int value = argNextInt(parser, false, &didError);

    if(didError) {
        translationPhaseCount = 6;
        return;
    }

    if(value < 1) {
        argError(parser, "translation phase out of range (got %d) - minimum = 1", value);
        return;
    }

    if(value > 8) {
        argError(parser, "translation phase out of range (got %d) - maximum = 8", value);
        return;
    }

    translationPhaseCount = value;
}

struct argArgument topArguments[] = {
    {"!input", '\0', "file to process", argPush, &files},
    {"-print-ast", 'a', "prints the ast to stdout", argSet, &printAst},
    {"-print-ir", 'i', "prints the ir to stdout", argSet, &printIr},
    {"-phase-count", 'E', "emit preprocessed output", preprocessFlag},
    {0},
};

streamError fileLayer(STREAM_LAYER) {
    (void)stream;
    (void)getNextArg;

    static bool called = false;
    if(called) return STREAM_OUT_OF_DATA;
    called = true;

    *data = readFileLen(ctx, len);
    return STREAM_NO_ERROR;
}

streamError printLayer(STREAM_LAYER) {
    (void)data;
    (void)len;
    void* value;
    while(getNextArg(stream, &value) == STREAM_NO_ERROR) {
        char c = *(char*)value;
        if(c == '\0') break;
        putc(c, ctx);
    }
    return STREAM_NO_ERROR;
}

int main(int argc, char** argv) {
    ArenaInit();

    struct argParser argparser = {
        argc - 1,
        argv + 1,
        topArguments,
    };
    bool hadError = parseArgs(&argparser);
    if(hadError) return EXIT_FAILURE;

    if(translationPhaseCount != 8) {
        for(unsigned int i = 0; i < files.dataCount; i++) {
            struct stream data = {(struct streamFn[]){
                {fileLayer, 1, (char*)files.datas[i]},
                {printLayer, 0, stdout},
                {0},
            }};

            streamRun(&data, NULL, 0);
        }
        return EXIT_SUCCESS;
    }

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

    return hadError ? EXIT_FAILURE : EXIT_SUCCESS;
}