#include "driver.h"

#include <stdbool.h>
#include <stdlib.h>
#include "analysis.h"
#include "argParser.h"
#include "astLower.h"
#include "file.h"
#include "ir.h"
#include "memory.h"
#include "parser.h"
#include "x64Encode.h"
#include "lex.h"

static struct stringList files = {0};
static struct stringList includeFiles = {0};
static bool printAst = false;
static bool printIr = false;
static int translationPhaseCount = 8;

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

static struct argArgument topArguments[] = {
    {"!input", '\0', "file to process", argPush, &files},
    {"-print-ast", 'a', "prints the ast to stdout", argSet, &printAst},
    {"-print-ir", 'i', "prints the ir to stdout", argSet, &printIr},
    {"-phase-count", 'E', "emit preprocessed output", preprocessFlag},
    {"-include", 'I', "add file to the include path", argPush, &includeFiles},
    {0},
};

void (*counts[])(TranslationContext*) = {
    runPhase1, runPhase2, runPhase3, runPhase4,
};

int driver(int argc, char** argv) {
    struct argParser argparser = {
        argc - 1,
        argv + 1,
        topArguments,
    };

    bool hadError = parseArgs(&argparser);
    if(hadError) return EXIT_FAILURE;

    MemoryPool pool;
    memoryPoolAlloc(&pool, 1ULL*TiB);

    IncludeSearchPath search;
    IncludeSearchPathInit(&search, SYSTEM_MINGW_W64, includeFiles.datas, includeFiles.dataCount);
    IncludeSearchState state = {0};
    fprintf(stderr, "1 stdint.h = %s\n", IncludeSearchPathFindUser(&state, &search, "stdint.h"));
    fprintf(stderr, "2 stdint.h = %s\n", IncludeSearchPathFindUser(&state, &search, "stdint.h"));

    if(translationPhaseCount != 8) {
        for(unsigned int i = 0; i < files.dataCount; i++) {
            TranslationContext ctx = {.trigraphs = true, .tabSize = 4, .debugPrint = true};
            TranslationContextInit(&ctx, &pool, (unsigned char*)files.datas[i]);
            counts[translationPhaseCount-1](&ctx);
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
            IrContext ctx;
            IrContextCreate(&ctx, &pool);

            if(printAst) ASTPrint(parser.ast);
            astLower(parser.ast, &ctx);
            if(printIr) IrContextPrint(&ctx);
        } else {
            hadError = true;
        }
    }

    return EXIT_SUCCESS;
}
