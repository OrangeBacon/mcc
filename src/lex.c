#include "lex.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "file.h"

typedef enum TokenType {
    TOKEN_KW_AUTO,
    TOKEN_KW_BREAK,
    TOKEN_KW_CASE,
    TOKEN_KW_CHAR,
    TOKEN_KW_CONST,
    TOKEN_KW_CONTINUE,
    TOKEN_KW_DEFAULT,
    TOKEN_KW_DO,
    TOKEN_KW_DOUBLE,
    TOKEN_KW_ELSE,
    TOKEN_KW_ENUM,
    TOKEN_KW_EXTERN,
    TOKEN_KW_FLOAT,
    TOKEN_KW_FOR,
    TOKEN_KW_GOTO,
    TOKEN_KW_IF,
    TOKEN_KW_INLINE,
    TOKEN_KW_INT,
    TOKEN_KW_LONG,
    TOKEN_KW_REGISTER,
    TOKEN_KW_RESTRICT,
    TOKEN_KW_RETURN,
    TOKEN_KW_SHORT,
    TOKEN_KW_SIGNED,
    TOKEN_KW_SIZEOF,
    TOKEN_KW_STATIC,
    TOKEN_KW_STRUCT,
    TOKEN_KW_SWITCH,
    TOKEN_KW_TYPEDEF,
    TOKEN_KW_UNION,
    TOKEN_KW_UNSIGNED,
    TOKEN_KW_VOID,
    TOKEN_KW_VOLATILE,
    TOKEN_KW_WHILE,
    TOKEN_KW_ALIGNAS,
    TOKEN_KW_ALIGNOF,
    TOKEN_KW_ATOMIC,
    TOKEN_KW_BOOL,
    TOKEN_KW_COMPLEX,
    TOKEN_KW_GENERIC,
    TOKEN_KW_IMAGINARY,
    TOKEN_KW_NORETURN,
    TOKEN_KW_STATICASSERT,
    TOKEN_KW_THREADLOCAL,
    TOKEN_PUNC_LEFT_SQUARE,
    TOKEN_PUNC_RIGHT_SQUARE,
    TOKEN_PUNC_LEFT_PAREN,
    TOKEN_PUNC_RIGHT_PAREN,
    TOKEN_PUNC_LEFT_BRACE,
    TOKEN_PUNC_RIGHT_BRACE,
    TOKEN_PUNC_DOT,
    TOKEN_PUNC_ARROW,
    TOKEN_PUNC_PLUS_PLUS,
    TOKEN_PUNC_MINUS_MINUS,
    TOKEN_PUNC_AND,
    TOKEN_PUNC_STAR,
    TOKEN_PUNC_PLUS,
    TOKEN_PUNC_MINUS,
    TOKEN_PUNC_TILDE,
    TOKEN_PUNC_BANG,
    TOKEN_PUNC_SLASH,
    TOKEN_PUNC_PERCENT,
    TOKEN_PUNC_LESS_LESS,
    TOKEN_PUNC_GREATER_GREATER,
    TOKEN_PUNC_LESS,
    TOKEN_PUNC_GREATER,
    TOKEN_PUNC_LESS_EQUAL,
    TOKEN_PUNC_GREATER_EQUAL,
    TOKEN_PUNC_EQUAL_EQUAL,
    TOKEN_PUNC_BANG_EQUAL,
    TOKEN_PUNC_CARET,
    TOKEN_PUNC_PIPE,
    TOKEN_PUNC_AND_AND,
    TOKEN_PUNC_OR_OR,
    TOKEN_PUNC_QUESTION,
    TOKEN_PUNC_COLON,
    TOKEN_PUNC_SEMICOLON,
    TOKEN_PUNC_ELIPSIS,
    TOKEN_PUNC_EQUAL,
    TOKEN_PUNC_STAR_EQUAL,
    TOKEN_PUNC_SLASH_EQUAL,
    TOKEN_PUNC_PERCENT_EQUAL,
    TOKEN_PUNC_PLUS_EQUAL,
    TOKEN_PUNC_MINUS_EQUAL,
    TOKEN_PUNC_LESS_LESS_EQUAL,
    TOKEN_PUNC_GREATER_GREATER_EQUAL,
    TOKEN_PUNC_AND_EQUAL,
    TOKEN_PUNC_CARET_EQUAL,
    TOKEN_PUNC_PIPE_EQUAL,
    TOKEN_PUNC_COMMA,
    TOKEN_PUNC_HASH,
    TOKEN_PUNC_HASH_HASH,
    TOKEN_HEADER_NAME,
    TOKEN_PP_NUMBER,
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_FLOATING,
    TOKEN_CHARACTER,
    TOKEN_STRING,
    TOKEN_UNKNOWN,
    TOKEN_ERROR,
    TOKEN_EOF,
} TokenType;

typedef struct Token {
    TokenType type;

    bool isStartOfLine;
    bool whitespaceBefore;
    bool isUsed;

    SourceLocation* loc;

    union {
        intmax_t integer;
        double floating;
        char* string;
    } data;
} Token;

void TranslationContextInit(TranslationContext* ctx, MemoryPool* pool, const char* fileName) {
    ctx->source = readFileLen(fileName, &ctx->sourceLength);
    memoryArrayAlloc(&ctx->sourceArr, pool, 4*MiB, sizeof(char));
    memoryArrayAlloc(&ctx->locations, pool, 128*MiB, sizeof(SourceLocation));
    ctx->consumed = 0;
    ctx->phase2Previous = EOF;
    ctx->phase3currentLocation = memoryArrayPush(&ctx->locations);
    *ctx->phase3currentLocation = ctx->phase1Location = (SourceLocation) {
        .fileName = fileName,
        .column = 0,
        .length = 0,
        .line = 1,
    };

    ctx->phase1IgnoreNewLine = '\0';
}

static char Phase1Peek(TranslationContext* ctx) {
    if(ctx->consumed >= ctx->sourceLength) return EOF;
    return ctx->source[ctx->consumed];
}

static char Phase1PeekNext(TranslationContext* ctx) {
    if(ctx->consumed - 1 >= ctx->sourceLength) return EOF;
    return ctx->source[ctx->consumed + 1];
}

static void Phase1NewLine(TranslationContext* ctx, char c) {
    if(ctx->phase1IgnoreNewLine != '\0') {
        if(c == ctx->phase1IgnoreNewLine) {
            ctx->phase1Location.line++;
            ctx->phase1Location.column = 1;
        }
        ctx->phase1IgnoreNewLine = '\0';
    }
    if(c == '\n') {
        ctx->phase1IgnoreNewLine = '\r';
        ctx->phase1Location.line++;
        ctx->phase1Location.column = 0;
    }
    if(c == '\r') {
        ctx->phase1IgnoreNewLine = '\n';
        ctx->phase1Location.line++;
        ctx->phase1Location.column = 0;
    }
}

static char Phase1Advance(TranslationContext* ctx) {
    if(ctx->consumed >= ctx->sourceLength) return EOF;
    ctx->consumed++;
    ctx->phase1Location.length++;
    ctx->phase1Location.column++;
    char c = ctx->source[ctx->consumed - 1];
    Phase1NewLine(ctx, c);
    return c;
}

static char Phase1AdvanceOverwrite(TranslationContext* ctx) {
    if(ctx->consumed >= ctx->sourceLength) return EOF;
    ctx->consumed++;
    ctx->phase1Location.length = 1;
    ctx->phase1Location.column++;
    char c = ctx->source[ctx->consumed - 1];
    Phase1NewLine(ctx, c);
    return c;
}

static char trigraphTranslation[] = {
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
static char Phase1Get(TranslationContext* ctx) {
    char c = Phase1AdvanceOverwrite(ctx);
    if(ctx->trigraphs && c == '?') {
        char c2 = Phase1Peek(ctx);
        if(c2 == '?') {
            char c3 = Phase1PeekNext(ctx);
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
                    Phase1Advance(ctx);
                    Phase1Advance(ctx);
                    return trigraphTranslation[(unsigned char)c3];
                default: return c;
            }
        }
    }

    return c;
}

void runPhase1(TranslationContext* ctx) {
    char c;
    while((c = Phase1Get(ctx)) != EOF) {
        putchar(c);
    }
}

static char Phase2Advance(TranslationContext* ctx) {
    char ret = ctx->phase2Peek;
    ctx->phase2CurrentLoc.length += ctx->phase2PeekLoc.length;
    ctx->phase2Peek = Phase1Get(ctx);
    ctx->phase2PeekLoc = ctx->phase1Location;
    return ret;
}

static char Phase2AdvanceOverwrite(TranslationContext* ctx) {
    char ret = ctx->phase2Peek;
    ctx->phase2CurrentLoc = ctx->phase2PeekLoc;
    ctx->phase2Peek = Phase1Get(ctx);
    ctx->phase2PeekLoc = ctx->phase1Location;
    return ret;
}

static char Phase2Peek(TranslationContext* ctx) {
    return ctx->phase2Peek;
}

static unsigned char Phase2Get(TranslationContext* ctx) {
    char c = Phase2AdvanceOverwrite(ctx);
    do {
        if(c == '\\') {
            char c1 = Phase2Peek(ctx);
            if(c1 == EOF) {
                fprintf(stderr, "Error: unexpected '\\' at end of file\n");
                exit(EXIT_FAILURE);
            } else if(c1 != '\n') {
                ctx->phase2Previous = c;
                return c;
            } else {
                Phase2Advance(ctx);
                // exit if statement
            }
        } else if(c == EOF && ctx->phase2Previous != '\n' && ctx->phase2Previous != EOF) {
            // error iso c
            fprintf(stderr, "Error: ISO C11 requires newline at end of file\n");
            exit(EXIT_FAILURE);
        } else {
            ctx->phase2Previous = c;
            return c;
        }
        c = Phase2Advance(ctx);
    } while(c != EOF);

    ctx->phase2Previous = c;
    return c;
}

static void Phase2Initialise(TranslationContext* ctx) {
    Phase2AdvanceOverwrite(ctx);
}

void runPhase2(TranslationContext* ctx) {
    Phase2Initialise(ctx);
    char c;
    while((c = Phase2Get(ctx)) != EOF) {
        putchar(c);
    }
}

static char Phase3Advance(TranslationContext* ctx) {
    char ret = ctx->phase3peek;
    ctx->phase3currentLocation->length += ctx->phase3peekLoc.length;

    ctx->phase3peek = ctx->phase3peekNext;
    ctx->phase3peekLoc = ctx->phase3peekNextLoc;
    ctx->phase3peekNext = Phase2Get(ctx);
    ctx->phase3peekNextLoc = ctx->phase2CurrentLoc;

    return ret;
}

static char Phase3AdvanceOverwrite(TranslationContext* ctx) {
    char ret = ctx->phase3peek;
    *ctx->phase3currentLocation = ctx->phase3peekLoc;

    ctx->phase3peek = ctx->phase3peekNext;
    ctx->phase3peekLoc = ctx->phase3peekNextLoc;
    ctx->phase3peekNext = Phase2Get(ctx);
    ctx->phase3peekNextLoc = ctx->phase2CurrentLoc;
    return ret;
}

static char Phase3Peek(TranslationContext* ctx) {
    return ctx->phase3peek;
}

static char Phase3PeekNext(TranslationContext* ctx) {
    return ctx->phase3peekNext;
}

static bool Phase3AtEnd(TranslationContext* ctx) {
    return ctx->phase3peek == EOF;
}

static void Phase3NewLine(Token* tok, TranslationContext* ctx, char c) {
    Phase3Advance(ctx);
    if(Phase3Peek(ctx) == c) {
        Phase3Advance(ctx);
    }
    tok->isStartOfLine = true;
    tok->whitespaceBefore = true;
}

static void skipWhitespace(Token* tok, TranslationContext* ctx) {
    while(true) {
        char c = Phase3Peek(ctx);
        switch(c) {
            case ' ':
            case '\t':
            case '\v':
            case '\f':
                tok->whitespaceBefore = true;
                Phase3Advance(ctx);
                break;
            case '\n':
                Phase3NewLine(tok, ctx, '\r');
                break;
            case '\r':
                Phase3NewLine(tok, ctx, '\n');
                break;

            case '/': {
                char next = Phase3PeekNext(ctx);
                if(next == '/') {
                    Phase3AdvanceOverwrite(ctx);
                    char c = '\0';
                    while((c = Phase3Peek(ctx)), (c != '\n' && c != '\r' && !Phase3AtEnd(ctx))) {
                        Phase3Advance(ctx);
                    }
                    if(c == '\n') {
                        Phase3NewLine(tok, ctx, '\r');
                    } else if(c == '\r') {
                        Phase3NewLine(tok, ctx, '\n');
                    }
                    tok->whitespaceBefore = true;
                } else if(next == '*') {
                    Phase3AdvanceOverwrite(ctx);
                    Phase3Advance(ctx);
                    while(!Phase3AtEnd(ctx)) {
                        if(Phase3Peek(ctx) == '*' && Phase3PeekNext(ctx) == '/') {
                            break;
                        }
                        Phase3Advance(ctx);
                    }
                    if(Phase3AtEnd(ctx)) {
                        fprintf(stderr, "Error: Unterminated multi-line comment at %lld:%lld\n", ctx->phase3currentLocation->line, ctx->phase3currentLocation->column);
                        exit(1);
                    }
                    Phase3Advance(ctx);
                    Phase3Advance(ctx);
                    tok->whitespaceBefore = true;
                } else {
                    return;
                }
            }; break;

            default: return;
        }
    }
}

static void Phase3Get(Token* tok, TranslationContext* ctx) {
    SourceLocation* loc = memoryArrayPush(&ctx->locations);
    *loc = *ctx->phase3currentLocation;
    loc->length = 0;
    ctx->phase3currentLocation = loc;

    skipWhitespace(tok, ctx);

    char c = Phase3AdvanceOverwrite(ctx);

    tok->loc = loc;
    tok->type = c == EOF ? TOKEN_EOF : TOKEN_UNKNOWN;
}

static void Phase3Initialise(TranslationContext* ctx) {
    Phase2Initialise(ctx);
    ctx->phase3mode = LEX_MODE_NO_HEADER,
    ctx->phase3peek = '\0',
    ctx->phase3peekNext = '\0',
    ctx->phase3peekLoc = *ctx->phase3currentLocation,
    ctx->phase3peekNextLoc = *ctx->phase3currentLocation,
    Phase3AdvanceOverwrite(ctx);
    Phase3AdvanceOverwrite(ctx);
}

void runPhase3(TranslationContext* ctx) {
    Phase3Initialise(ctx);
    Token tok;
    while(Phase3Get(&tok, ctx), tok.type != TOKEN_EOF) {
        printf("i");
    }
}