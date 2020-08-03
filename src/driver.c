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

static struct stringList files = {0};
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
    {0},
};

typedef struct TranslationContext {
    unsigned char* source;
    size_t sourceLength;
    size_t consumed;

    bool trigraphs : 1;
} TranslationContext;

void TranslationContextInit(TranslationContext* ctx, const char* fileName) {
    ctx->source = (unsigned char*)readFileLen(fileName, &ctx->sourceLength);
    ctx->consumed = 0;
}

static unsigned char peek(TranslationContext* ctx) {
    if(ctx->consumed >= ctx->sourceLength) return EOF;
    return ctx->source[ctx->consumed];
}

static unsigned char peekNext(TranslationContext* ctx) {
    if(ctx->consumed - 1 >= ctx->sourceLength) return EOF;
    return ctx->source[ctx->consumed + 1];
}

static unsigned char advance(TranslationContext* ctx) {
    if(ctx->consumed >= ctx->sourceLength) return EOF;
    ctx->consumed++;
    return ctx->source[ctx->consumed - 1];
}

static unsigned char trigraphTranslation[] = {
    ['='] = '#',
    ['('] = '[',
    ['/'] = '\\',
    [')'] = ']',
    ['\''] = '^',
    ['<'] = '{',
    ['!'] = '|',
    ['>'] = '}',
    ['-'] = '}',
};

// implement phase 1
// technically, this should convert the file to utf8, and probably normalise it,
// but I am not implementing that
static unsigned char phase1Get(TranslationContext* ctx) {
    unsigned char c = advance(ctx);
    if(ctx->trigraphs && c == '?') {
        unsigned char c2 = peek(ctx);
        if(c2 == '?') {
            unsigned char c3 = peekNext(ctx);
            switch(c3) {
                case '=':
                case '(':
                case '/':
                case ')':
                case '\'':
                case '<':
                case '!':
                case '>':
                case '-':
                    advance(ctx);
                    advance(ctx);
                    return trigraphTranslation[c3];
                default: return c;
            }
        }
    }

    return c;
}


int driver(int argc, char** argv) {
    struct argParser argparser = {
        argc - 1,
        argv + 1,
        topArguments,
    };
    bool hadError = parseArgs(&argparser);
    if(hadError) return EXIT_FAILURE;

    if(translationPhaseCount != 8) {
        for(unsigned int i = 0; i < files.dataCount; i++) {
            if(translationPhaseCount == 1) {
                TranslationContext ctx = {.trigraphs = true};
                TranslationContextInit(&ctx, files.datas[i]);
                char c;
                while((c = phase1Get(&ctx)) != EOF) {
                    putchar(c);
                }
            }
        }
        return EXIT_SUCCESS;
    }

    MemoryPool pool;
    memoryPoolAlloc(&pool, 1ULL*TiB);

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