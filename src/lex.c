#include "lex.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "file.h"

// End of file
// default is -1, which is the same as 0xff, but type conversions work
// better with unsigned char this way
#define END_OF_FILE 0xff

static void StringTypePrint(LexerStringType t) {
    switch(t) {
        case STRING_NONE: return;
        case STRING_U8: printf("u8"); return;
        case STRING_WCHAR: printf("L"); return;
        case STRING_16: printf("u"); return;
        case STRING_32: printf("U"); return;
    }
}

static LexerTokenType stringLikeTokens[] = {
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
    TOKEN_PP_NUMBER,
    TOKEN_IDENTIFIER_L,
    TOKEN_CHARACTER_L,
    TOKEN_STRING_L,
};

static LexerTokenType puncLikeTokens[] = {
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
    TOKEN_PUNC_OR,
    TOKEN_PUNC_AND_AND,
    TOKEN_PUNC_OR_OR,
    TOKEN_PUNC_QUESTION,
    TOKEN_PUNC_COLON,
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
    TOKEN_PUNC_HASH,
    TOKEN_PUNC_HASH_HASH,
    TOKEN_PUNC_LESS_COLON, // [
    TOKEN_PUNC_COLON_GREATER, // ]
    TOKEN_PUNC_LESS_PERCENT, // {
    TOKEN_PUNC_PERCENT_GREATER, // }
    TOKEN_PUNC_PERCENT_COLON, // #
    TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON, // ##
};

static LexerTokenType couldBeInNumber[] = {
    TOKEN_PUNC_DOT,
    TOKEN_PUNC_ARROW,
    TOKEN_PUNC_PLUS_PLUS,
    TOKEN_PUNC_MINUS_MINUS,
    TOKEN_PUNC_PLUS,
    TOKEN_PUNC_MINUS,
    TOKEN_PUNC_PLUS_EQUAL,
    TOKEN_PUNC_MINUS_EQUAL,
};

bool TokenPasteAvoidance(LexerTokenType left, LexerTokenType right) {
    // token spacing adjustments, so re-lexing works properly
    bool leftIncluded = false;
    bool rightIncluded = false;
    for(unsigned int i = 0; i < sizeof(stringLikeTokens)/sizeof(LexerTokenType); i++) {
        if(stringLikeTokens[i] == left) leftIncluded = true;
        if(stringLikeTokens[i] == right) rightIncluded = true;
        if(leftIncluded && rightIncluded) break;
    }

    if(leftIncluded && rightIncluded) {
        return true;
    }

    if(left == TOKEN_PP_NUMBER) {
        for(unsigned int i = 0; i < sizeof(couldBeInNumber)/sizeof(LexerTokenType); i++) {
            if(couldBeInNumber[i] == right) return true;
        }
    }

    leftIncluded = false;
    rightIncluded = false;

    for(unsigned int i = 0; i < sizeof(puncLikeTokens)/sizeof(LexerTokenType); i++) {
        if(puncLikeTokens[i] == left) leftIncluded = true;
        if(puncLikeTokens[i] == right) rightIncluded = true;
        if(leftIncluded && rightIncluded) break;
    }

    return leftIncluded && rightIncluded;
}

static void TokenPrint(TranslationContext* ctx, LexerToken* tok) {
    if(ctx->debugPrint) {
        printf("%llu:%llu", tok->loc->line, tok->loc->column);
        if(tok->renderStartOfLine) printf(" bol");
        if(tok->whitespaceBefore) printf(" white=%llu", tok->indent);
        printf(" token=%d", tok->type);
        printf(" data(%llu) ", tok->loc->length);
    } else {
        bool printedWhitespace = false;
        if(tok->renderStartOfLine && !ctx->tokenPrinterAtStart) {
            printf("\n");
            printedWhitespace = true;
        }
        ctx->tokenPrinterAtStart = false;
        if(tok->whitespaceBefore) {
            printf("%*s", (int)tok->indent, "");
            printedWhitespace |= tok->indent > 0;
        }
        if(!printedWhitespace && TokenPasteAvoidance(ctx->previousPrinted, tok->type)) {
            printf(" ");
        }
    }
    switch(tok->type) {
        case TOKEN_KW_AUTO: printf("auto"); break;
        case TOKEN_KW_BREAK: printf("break"); break;
        case TOKEN_KW_CASE: printf("case"); break;
        case TOKEN_KW_CHAR: printf("char"); break;
        case TOKEN_KW_CONST: printf("const"); break;
        case TOKEN_KW_CONTINUE: printf("continue"); break;
        case TOKEN_KW_DEFAULT: printf("default"); break;
        case TOKEN_KW_DO: printf("do"); break;
        case TOKEN_KW_DOUBLE: printf("double"); break;
        case TOKEN_KW_ELSE: printf("else"); break;
        case TOKEN_KW_ENUM: printf("enum"); break;
        case TOKEN_KW_EXTERN: printf("extern"); break;
        case TOKEN_KW_FLOAT: printf("float"); break;
        case TOKEN_KW_FOR: printf("for"); break;
        case TOKEN_KW_GOTO: printf("goto"); break;
        case TOKEN_KW_IF: printf("if"); break;
        case TOKEN_KW_INLINE: printf("inline"); break;
        case TOKEN_KW_INT: printf("int"); break;
        case TOKEN_KW_LONG: printf("long"); break;
        case TOKEN_KW_REGISTER: printf("register"); break;
        case TOKEN_KW_RESTRICT: printf("restrict"); break;
        case TOKEN_KW_RETURN: printf("return"); break;
        case TOKEN_KW_SHORT: printf("short"); break;
        case TOKEN_KW_SIGNED: printf("signed"); break;
        case TOKEN_KW_SIZEOF: printf("sizeof"); break;
        case TOKEN_KW_STATIC: printf("static"); break;
        case TOKEN_KW_STRUCT: printf("struct"); break;
        case TOKEN_KW_SWITCH: printf("switch"); break;
        case TOKEN_KW_TYPEDEF: printf("typedef"); break;
        case TOKEN_KW_UNION: printf("union"); break;
        case TOKEN_KW_UNSIGNED: printf("unsigned"); break;
        case TOKEN_KW_VOID: printf("void"); break;
        case TOKEN_KW_VOLATILE: printf("volatile"); break;
        case TOKEN_KW_WHILE: printf("while"); break;
        case TOKEN_KW_ALIGNAS: printf("_Alignas"); break;
        case TOKEN_KW_ALIGNOF: printf("_Alignof"); break;
        case TOKEN_KW_ATOMIC: printf("_Atomic"); break;
        case TOKEN_KW_BOOL: printf("_Bool"); break;
        case TOKEN_KW_COMPLEX: printf("_Complex"); break;
        case TOKEN_KW_GENERIC: printf("_Generic"); break;
        case TOKEN_KW_IMAGINARY: printf("_Imaginary"); break;
        case TOKEN_KW_NORETURN: printf("_Noreturn"); break;
        case TOKEN_KW_STATICASSERT: printf("_Static_assert"); break;
        case TOKEN_KW_THREADLOCAL: printf("_Thread_local"); break;
        case TOKEN_PUNC_LEFT_SQUARE: printf("["); break;
        case TOKEN_PUNC_RIGHT_SQUARE: printf("]"); break;
        case TOKEN_PUNC_LEFT_PAREN: printf("("); break;
        case TOKEN_PUNC_RIGHT_PAREN: printf(")"); break;
        case TOKEN_PUNC_LEFT_BRACE: printf("{"); break;
        case TOKEN_PUNC_RIGHT_BRACE: printf("}"); break;
        case TOKEN_PUNC_DOT: printf("."); break;
        case TOKEN_PUNC_ARROW: printf("->"); break;
        case TOKEN_PUNC_PLUS_PLUS: printf("++"); break;
        case TOKEN_PUNC_MINUS_MINUS: printf("--"); break;
        case TOKEN_PUNC_AND: printf("&"); break;
        case TOKEN_PUNC_STAR: printf("*"); break;
        case TOKEN_PUNC_PLUS: printf("+"); break;
        case TOKEN_PUNC_MINUS: printf("-"); break;
        case TOKEN_PUNC_TILDE: printf("~"); break;
        case TOKEN_PUNC_BANG: printf("!"); break;
        case TOKEN_PUNC_SLASH: printf("/"); break;
        case TOKEN_PUNC_PERCENT: printf("%%"); break;
        case TOKEN_PUNC_LESS_LESS: printf("<<"); break;
        case TOKEN_PUNC_GREATER_GREATER: printf(">>"); break;
        case TOKEN_PUNC_LESS: printf("<"); break;
        case TOKEN_PUNC_GREATER: printf(">"); break;
        case TOKEN_PUNC_LESS_EQUAL: printf("<="); break;
        case TOKEN_PUNC_GREATER_EQUAL: printf(">="); break;
        case TOKEN_PUNC_EQUAL_EQUAL: printf("=="); break;
        case TOKEN_PUNC_BANG_EQUAL: printf("!="); break;
        case TOKEN_PUNC_CARET: printf("^"); break;
        case TOKEN_PUNC_OR: printf("|"); break;
        case TOKEN_PUNC_AND_AND: printf("&&"); break;
        case TOKEN_PUNC_OR_OR: printf("||"); break;
        case TOKEN_PUNC_QUESTION: printf("?"); break;
        case TOKEN_PUNC_COLON: printf(":"); break;
        case TOKEN_PUNC_SEMICOLON: printf(";"); break;
        case TOKEN_PUNC_ELIPSIS: printf("..."); break;
        case TOKEN_PUNC_EQUAL: printf("="); break;
        case TOKEN_PUNC_STAR_EQUAL: printf("*="); break;
        case TOKEN_PUNC_SLASH_EQUAL: printf("/="); break;
        case TOKEN_PUNC_PERCENT_EQUAL: printf("%%="); break;
        case TOKEN_PUNC_PLUS_EQUAL: printf("+="); break;
        case TOKEN_PUNC_MINUS_EQUAL: printf("-="); break;
        case TOKEN_PUNC_LESS_LESS_EQUAL: printf("<<="); break;
        case TOKEN_PUNC_GREATER_GREATER_EQUAL: printf(">>="); break;
        case TOKEN_PUNC_AND_EQUAL: printf("&="); break;
        case TOKEN_PUNC_CARET_EQUAL: printf("^="); break;
        case TOKEN_PUNC_PIPE_EQUAL: printf("|="); break;
        case TOKEN_PUNC_COMMA: printf(","); break;
        case TOKEN_PUNC_HASH: printf("#"); break;
        case TOKEN_PUNC_HASH_HASH: printf("##"); break;
        case TOKEN_PUNC_LESS_COLON: printf("<:"); break;
        case TOKEN_PUNC_COLON_GREATER: printf(":>"); break;
        case TOKEN_PUNC_LESS_PERCENT: printf("<%%"); break;
        case TOKEN_PUNC_PERCENT_GREATER: printf("%%>"); break;
        case TOKEN_PUNC_PERCENT_COLON: printf("%%:"); break;
        case TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON: printf("%%:%%:"); break;
        case TOKEN_HEADER_NAME: printf("\"%s\"", tok->data.string.buffer); break;
        case TOKEN_SYS_HEADER_NAME: printf("<%s>", tok->data.string.buffer); break;
        case TOKEN_PP_NUMBER: printf("%s", tok->data.string.buffer); break;
        case TOKEN_IDENTIFIER_L: printf("%s", tok->data.node->name.data.string.buffer); break;
        case TOKEN_INTEGER_L: printf("%llu", tok->data.integer); break;
        case TOKEN_FLOATING_L: printf("%f", tok->data.floating); break;
        case TOKEN_CHARACTER_L: StringTypePrint(tok->data.string.type); printf("'%s'", tok->data.string.buffer); break;
        case TOKEN_STRING_L: StringTypePrint(tok->data.string.type); printf("\"%s\"", tok->data.string.buffer); break;
        case TOKEN_MACRO_ARG: printf("argument(%lld)", tok->data.integer); break;
        case TOKEN_UNKNOWN_L: printf("%c", tok->data.character); break;
        case TOKEN_ERROR_L: printf("error token"); break;
        case TOKEN_EOF_L: break;
    }

    if(ctx->debugPrint) {
        printf("\n");
    }
    ctx->previousPrinted = tok->type;
}

static void LexerStringInit(LexerString* str, TranslationContext* ctx, size_t size) {
    str->buffer = memoryArrayPushN(&ctx->stringArr, size + 1);
    str->buffer[0] = '\0';
    str->capacity = size;
    str->count = 0;
    str->type = STRING_NONE;
}

static void LexerStringAddC(LexerString* str, TranslationContext* ctx, char c) {
    if(str->count >= str->capacity) {
        char* buffer = memoryArrayPushN(&ctx->stringArr, str->capacity * 2 + 1);
        strncpy(buffer, str->buffer, str->capacity);
        str->buffer = buffer;
        str->capacity *= 2;
    }

    str->buffer[str->count] = c;
    str->buffer[str->count + 1] = '\0';
    str->count++;
}

// Setup function
void TranslationContextInit(TranslationContext* ctx, MemoryPool* pool, const unsigned char* fileName) {
    ctx->phase1source = (unsigned char*)readFileLen((char*)fileName, &ctx->phase1sourceLength);

    memoryArrayAlloc(&ctx->stringArr, pool, 4*MiB, sizeof(unsigned char));
    memoryArrayAlloc(&ctx->locations, pool, 128*MiB, sizeof(SourceLocation));

    ctx->pool = pool;
    ctx->tokenPrinterAtStart = true;
    ctx->fileName = fileName;
    ctx->phase1consumed = 0;
    ctx->phase1IgnoreNewLine = '\0';
    ctx->previousPrinted = TOKEN_EOF_L;
    ctx->phase1Location = (SourceLocation) {
        .fileName = fileName,
        .column = 0,
        .length = 0,
        .line = 1,
    };
}

// ------- //
// Phase 1 //
// ------- //
// Physical source characters -> source character set (unimplemented)
// Trigraph conversion
// internaly uses out parameter instead of END_OF_FILE to avoid reading
// END_OF_FILE as a control character, so emitting an error for it

// return the next character without consuming it
static unsigned char Phase1Peek(TranslationContext* ctx, bool* succeeded) {
    if(ctx->phase1consumed >= ctx->phase1sourceLength) {
        *succeeded = false;
        return '\0';
    }
    *succeeded = true;
    return ctx->phase1source[ctx->phase1consumed];
}

// return the character after next without consuming its
static unsigned char Phase1PeekNext(TranslationContext* ctx, bool* succeeded) {
    if(ctx->phase1consumed - 1 >= ctx->phase1sourceLength) {
        *succeeded = false;
        return '\0';
    }
    *succeeded = true;
    return ctx->phase1source[ctx->phase1consumed + 1];
}

// process a newline character ('\n' or '\r') to check how to advance the
// line counter.  If '\n' encountered, a following '\r' will not change the
// counter, if '\r', the '\n' will not update.  This allows multiple possible
// line endings: "\n", "\r", "\n\r", "\r\n" that are all considered one line end
static void Phase1NewLine(TranslationContext* ctx, unsigned char c) {
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

// get the next character from the previous phase
// increases the SourcLocation's length with the new character
static unsigned char Phase1Advance(TranslationContext* ctx, bool* succeeded) {
    if(ctx->phase1consumed >= ctx->phase1sourceLength) {
        *succeeded = false;
        return '\0';
    }
    ctx->phase1consumed++;
    ctx->phase1Location.length++;
    ctx->phase1Location.column++;
    unsigned char c = ctx->phase1source[ctx->phase1consumed - 1];
    Phase1NewLine(ctx, c);
    *succeeded = true;
    return c;
}

// get the next character from the previous phase
// sets the SourceLocation to begin from the new character's start
static unsigned char Phase1AdvanceOverwrite(TranslationContext* ctx, bool* succeeded) {
    if(ctx->phase1consumed >= ctx->phase1sourceLength) {
        *succeeded = false;
        return '\0';
    }
    ctx->phase1consumed++;
    ctx->phase1Location.length = 1;
    ctx->phase1Location.column++;
    unsigned char c = ctx->phase1source[ctx->phase1consumed - 1];
    Phase1NewLine(ctx, c);
    *succeeded = true;
    return c;
}

// map trigraph "??{index}" -> real character
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
static unsigned char Phase1Get(TranslationContext* ctx) {
    bool succeeded;
    unsigned char c = Phase1AdvanceOverwrite(ctx, &succeeded);

    if(!succeeded) {
        return END_OF_FILE;
    }

    if(ctx->phase1consumed == 1) {
        // start of file - ignore BOM

        // do not need to check succeded due to length check in condition
        if(ctx->phase1sourceLength >= 3 && c == 0xEF
        && Phase1Peek(ctx, &succeeded) == 0xBB
        && Phase1PeekNext(ctx, &succeeded) == 0xBF) {
            Phase1Advance(ctx, &succeeded);
            Phase1Advance(ctx, &succeeded);
        }
    }

    // invalid bytes
    if(c == 0xC0 || c == 0xC1 || c >= 0xF5) {
        fprintf(stderr, "Error: found invalid byte for utf8 text\n");
        return '\0';
    }

    if((c <= 0x1F || c == 0x7F) && c != '\n' && c != '\r' && c != '\t' && c != '\v' && c != '\f') {
        fprintf(stderr, "Error: found control character in source file - %lld:%lld\n", ctx->phase1Location.line, ctx->phase1Location.column);
        return '\0';
    }

    if(ctx->trigraphs && c == '?') {
        unsigned char c2 = Phase1Peek(ctx, &succeeded);
        if(succeeded && c2 == '?') {
            unsigned char c3 = Phase1PeekNext(ctx, &succeeded);

            if(!succeeded) {
                return c;
            }

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
                    Phase1Advance(ctx, &succeeded);
                    Phase1Advance(ctx, &succeeded);
                    return trigraphTranslation[c3];
                default:
                    return c;
            }
        }
    }

    return c;
}

// helper to run only phase 1
void runPhase1(TranslationContext* ctx) {
    char c;
    while((c = Phase1Get(ctx)) != EOF) {
        putchar(c);
    }
}

// ------- //
// Phase 2 //
// ------- //
// Backslash-newline removal
// Error if file ends in non-newline character

// get the next character from the previous phase
// increases the SourcLocation's length with the new character
static unsigned char Phase2Advance(TranslationContext* ctx) {
    unsigned char ret = ctx->phase2Peek;
    ctx->phase2CurrentLoc.length += ctx->phase2PeekLoc.length;
    ctx->phase2Peek = Phase1Get(ctx);
    ctx->phase2PeekLoc = ctx->phase1Location;
    return ret;
}

// get the next character from the previous phase
// sets the SourceLocation to begin from the new character's start
static unsigned char Phase2AdvanceOverwrite(TranslationContext* ctx) {
    unsigned char ret = ctx->phase2Peek;
    ctx->phase2CurrentLoc = ctx->phase2PeekLoc;
    ctx->phase2Peek = Phase1Get(ctx);
    ctx->phase2PeekLoc = ctx->phase1Location;
    return ret;
}

// return the next character without consuming it
static unsigned char Phase2Peek(TranslationContext* ctx) {
    return ctx->phase2Peek;
}

// implements backslash-newline skipping
// if the next character after a backslash/newline is another backslash/newline
// then that should be skiped as well iteratively until the next real character
// emits error for backslash-END_OF_FILE and /[^\n]/-END_OF_FILE
static unsigned char Phase2Get(TranslationContext* ctx) {
    unsigned char c = Phase2AdvanceOverwrite(ctx);
    do {
        if(c == '\\') {
            unsigned char c1 = Phase2Peek(ctx);
            if(c1 == END_OF_FILE) {
                fprintf(stderr, "Error: unexpected '\\' at end of file\n");
                return END_OF_FILE;
            } else if(c1 != '\n') {
                ctx->phase2Previous = c;
                return c;
            } else {
                Phase2Advance(ctx);
                // exit if statement
            }
        } else if(c == END_OF_FILE && ctx->phase2Previous != '\n' && ctx->phase2Previous != END_OF_FILE) {
            // error iso c
            ctx->phase2Previous = END_OF_FILE;
            fprintf(stderr, "Error: ISO C11 requires newline at end of file\n");
            return END_OF_FILE;
        } else {
            ctx->phase2Previous = c;
            return c;
        }
        c = Phase2Advance(ctx);
    } while(c != END_OF_FILE);

    ctx->phase2Previous = c;
    return c;
}

// setup phase2's buffers
static void Phase2Initialise(TranslationContext* ctx) {
    ctx->phase2Previous = END_OF_FILE;
    Phase2AdvanceOverwrite(ctx);
}

// helper to run upto and including phase 2
void runPhase2(TranslationContext* ctx) {
    Phase2Initialise(ctx);
    unsigned char c;
    while((c = Phase2Get(ctx)) != END_OF_FILE) {
        putchar(c);
    }
}

// ------- //
// Phase 3 //
// ------- //
// characters -> preprocessing tokens (unimplemented)
// comments -> whitespace
// tracking begining of line + prior whitespace in tokens

static unsigned char Phase3GetFromPhase2(TranslationContext* ctx, SourceLocation* loc) {
    unsigned char c = Phase2Get(ctx);
    *loc = ctx->phase2CurrentLoc;
    return c;
}

// get the next character from the previous phase
// increases the SourcLocation's length with the new character
static unsigned char Phase3Advance(TranslationContext* ctx) {
    unsigned char ret = ctx->phase3.peek;
    ctx->phase3.currentLocation->length += ctx->phase3.peekLoc.length;

    ctx->phase3.peek = ctx->phase3.peekNext;
    ctx->phase3.peekLoc = ctx->phase3.peekNextLoc;
    ctx->phase3.peekNext = ctx->phase3.getter(ctx, &ctx->phase3.peekNextLoc);

    return ret;
}

// get the next character from the previous phase
// sets the SourceLocation to begin from the new character's start
static unsigned char Phase3AdvanceOverwrite(TranslationContext* ctx) {
    unsigned char ret = ctx->phase3.peek;
    *ctx->phase3.currentLocation = ctx->phase3.peekLoc;

    ctx->phase3.peek = ctx->phase3.peekNext;
    ctx->phase3.peekLoc = ctx->phase3.peekNextLoc;
    ctx->phase3.peekNext = ctx->phase3.getter(ctx, &ctx->phase3.peekNextLoc);
    return ret;
}

// return the next character without consuming it
static unsigned char Phase3Peek(TranslationContext* ctx) {
    return ctx->phase3.peek;
}

// return the character after next without consuming it
static unsigned char Phase3PeekNext(TranslationContext* ctx) {
    return ctx->phase3.peekNext;
}

// has phase 3 reached the end of the file?
static bool Phase3AtEnd(TranslationContext* ctx) {
    return ctx->phase3.peek == END_OF_FILE;
}

// skip a new line ("\n", "\r", "\n\r", "\r\n") and set that the token is
// at the begining of a line
static void Phase3NewLine(LexerToken* tok, TranslationContext* ctx, unsigned char c) {
    Phase3Advance(ctx);
    if(Phase3Peek(ctx) == c) {
        Phase3Advance(ctx);
    }
    tok->isStartOfLine = true;
    tok->renderStartOfLine = true;
    tok->whitespaceBefore = true;
    tok->indent = 0;
}

// skip characters until non-whitespace character encountered
// also skips all comments, replacing them with whitespace
// errors on unterminated multi-line comment
// tracks whether a token has whitespace before it and if it is the first token
// on a source line
// tracks the count of prior whitespace on the same line, hopefully that can
// be used for error better error recovery in the parser
static void skipWhitespace(LexerToken* tok, TranslationContext* ctx) {
    tok->whitespaceBefore = false;
    tok->isStartOfLine = false;
    tok->renderStartOfLine = false;
    tok->indent = 0;

    if(ctx->phase3.AtStart) {
        tok->isStartOfLine = true;
        tok->renderStartOfLine = true;
        ctx->phase3.AtStart = false;
    }

    while(!Phase3AtEnd(ctx)) {
        unsigned char c = Phase3Peek(ctx);
        switch(c) {
            case ' ':
            case '\t':
            case '\v':
            case '\f':
                tok->whitespaceBefore = true;
                Phase3Advance(ctx);
                if(c == ' ') tok->indent++;
                if(c == '\t') tok->indent += ctx->tabSize;
                break;
            case '\n':
                Phase3NewLine(tok, ctx, '\r');
                break;
            case '\r':
                Phase3NewLine(tok, ctx, '\n');
                break;

            case '/': {
                unsigned char next = Phase3PeekNext(ctx);
                if(next == '/') {
                    // single line comment (//)
                    Phase3AdvanceOverwrite(ctx);
                    unsigned char c = '\0';
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
                    // multi line comment (/**/)
                    Phase3AdvanceOverwrite(ctx);
                    Phase3Advance(ctx);
                    while(!Phase3AtEnd(ctx)) {
                        if(Phase3Peek(ctx) == '*' && Phase3PeekNext(ctx) == '/') {
                            break;
                        }
                        bool advanced = false;
                        if(Phase3Peek(ctx) == '\n') {
                            Phase3NewLine(tok, ctx, '\r');
                            advanced = true;
                        }
                        if(Phase3Peek(ctx) == '\r') {
                            Phase3NewLine(tok, ctx, '\r');
                            advanced = true;
                        }
                        if(!advanced) {
                            Phase3Advance(ctx);
                        }
                    }
                    if(Phase3AtEnd(ctx)) {
                        fprintf(stderr, "Error: Unterminated multi-line comment at %lld:%lld\n", ctx->phase3.currentLocation->line, ctx->phase3.currentLocation->column);
                        return;
                    }
                    Phase3Advance(ctx);
                    Phase3Advance(ctx);
                    tok->whitespaceBefore = true;
                    tok->indent++;
                } else {
                    return;
                }
            }; break;

            default: return;
        }
    }
}

static bool Phase3Match(TranslationContext* ctx, unsigned char c) {
    if(Phase3AtEnd(ctx)) return false;
    if(Phase3Peek(ctx) != c) return false;
    Phase3Advance(ctx);
    return true;
}

// this wrapper only exists because of wierd precedence with
// a ? (b = c) : (b = d) requiring those parentheses, so this
// is more clear to read
static void Phase3Make(LexerToken* tok, LexerTokenType type) {
    tok->type = type;
}

static bool isNonDigit(unsigned char c) {
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool isDigit(unsigned char c) {
    return c >= '0' && c <= '9';
}

static bool isHexDigit(unsigned char c) {
    return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || (c >= '0' && c <= '9');
}

// parse a universal character name
static void ParseUniversalCharacterName(TranslationContext* ctx, LexerToken* tok) {
    // '\\' already consumed

    // 'u' vs 'U' check already done
    unsigned char initial = Phase3Advance(ctx);

    char buffer[9];
    int length = initial == 'u' ? 4 : 8;

    for(int i = 0; i < length; i++) {
        unsigned char c = Phase3Advance(ctx);
        if(!isHexDigit(c)) {
            fprintf(stderr, "Error: non-hex digit found in universal character name\n");
            tok->type = TOKEN_ERROR_L;
            return;
        }
        buffer[i] = c;
    }
    buffer[length] = '\0';

    // call should not error as we have already checked everything being
    // parsed by it
    char* end;
    intmax_t num = strtoimax(buffer, &end, 16);

    if(num >= 0xD800 && num <= 0xDFFF) {
        fprintf(stderr, "Error: surrogate pair specified by universal character name\n");
        tok->type = TOKEN_ERROR_L;
        return;
    }

    if(num < 0x00A0 && num != '$' && num != '@' && num != '`') {
        fprintf(stderr, "Error: universal character specified out of allowable range\n");
        tok->type = TOKEN_ERROR_L;
        return;
    }

    // UTF-8 Encoder - see ISO/IEC 10646:2017 p15
    if(num < 0x007F) {
        LexerStringAddC(&tok->data.string, ctx, num);
    } else if(num < 0x07FF) {
        unsigned char o1 = 0xC0 | (num >> 6);
        unsigned char o2 = 0x80 | (num & 0x3F);
        LexerStringAddC(&tok->data.string, ctx, o1);
        LexerStringAddC(&tok->data.string, ctx, o2);
    } else if(num < 0xFFFF) {
        unsigned char o1 = 0xE0 | (num >> 12);
        unsigned char o2 = 0x80 | ((num >> 6) & 0x3F);
        unsigned char o3 = 0x80 | (num & 0x3F);
        LexerStringAddC(&tok->data.string, ctx, o1);
        LexerStringAddC(&tok->data.string, ctx, o2);
        LexerStringAddC(&tok->data.string, ctx, o3);
    } else if(num < 0x10FFFF) {
        unsigned char o1 = 0xF0 | (num >> 18);
        unsigned char o2 = 0x80 | ((num >> 12) & 0x3F);
        unsigned char o3 = 0x80 | ((num >> 6) & 0x3F);
        unsigned char o4 = 0x80 | (num & 0x3F);
        LexerStringAddC(&tok->data.string, ctx, o1);
        LexerStringAddC(&tok->data.string, ctx, o2);
        LexerStringAddC(&tok->data.string, ctx, o3);
        LexerStringAddC(&tok->data.string, ctx, o4);
    } else {
        fprintf(stderr, "Error: UCS code point out of range: Maximum = 0x10FFFF\n");
        tok->type = TOKEN_ERROR_L;
        return;
    }
}

static bool isStringLike(TranslationContext* ctx, unsigned char c, unsigned char start) {
    unsigned char next = Phase3Peek(ctx);
    unsigned char nextNext = Phase3PeekNext(ctx);
    return c == start ||
        ((c == 'u' || c == 'U' || c == 'L') && next == start) ||
        (c == 'u' && next == '8' && nextNext == start);
}

// Generic string literal ish token parser
// used for character and string literals
// does not deal with escape sequences properly, that is for phase 5
static void ParseString(TranslationContext* ctx, LexerToken* tok, unsigned char c, unsigned char start) {
    unsigned char next = Phase3Peek(ctx);

    tok->type = start == '"' ? TOKEN_STRING_L : TOKEN_CHARACTER_L;
    LexerStringInit(&tok->data.string, ctx, 10);
    LexerStringType t =
        c == start ? STRING_NONE :
        c == 'u' && next == '8' ? STRING_U8 :
        c == 'u' ? STRING_16 :
        c == 'U' ? STRING_32 :
        STRING_WCHAR;
    tok->data.string.type = t;

    // skip prefix characters
    if(t == STRING_U8) {
        Phase3Advance(ctx);
        Phase3Advance(ctx);
    } else if(t == STRING_16 || t == STRING_32 || t == STRING_WCHAR) {
        Phase3Advance(ctx);
    }

    c = Phase3Peek(ctx);
    while(!Phase3AtEnd(ctx) && c != start) {
        Phase3Advance(ctx);
        LexerStringAddC(&tok->data.string, ctx, c);

        // skip escape sequences so that \" does not end a string
        if(c == '\\') {
            LexerStringAddC(&tok->data.string, ctx, Phase3Advance(ctx));
        } else if(c == '\n') {
            fprintf(stderr, "Error: %s literal unterminated at end of line\n", start == '\'' ? "character" : "string");
            tok->type = TOKEN_ERROR_L;
            return;
        }

        c = Phase3Peek(ctx);
    }

    if(start == '\'' && tok->data.string.count == 0) {
        fprintf(stderr, "Error: character literal requires at least one character\n");
        tok->type = TOKEN_ERROR_L;
    }

    if(Phase3Advance(ctx) != start) {
        fprintf(stderr, "Error: %s literal unterminated at end of file\n", start == '\'' ? "character" : "string");
        tok->type = TOKEN_ERROR_L;
        return;
    }
}

// parses
//  < h-char-sequence >
//  " q-char-sequence "
// as in n1570/6.4.7
static void ParseHeaderName(TranslationContext* ctx, LexerToken* tok, unsigned char end) {
    tok->type = end == '>' ? TOKEN_SYS_HEADER_NAME : TOKEN_HEADER_NAME;

    LexerStringInit(&tok->data.string, ctx, 20);

    unsigned char c = Phase3Peek(ctx);
    while(!Phase3AtEnd(ctx) && c != end && c != '\n') {
        Phase3Advance(ctx);
        if(c == '\'' || c == '\\' || (end == '>' && c == '"')) {
            fprintf(stderr, "Error: encountered `%c` while parsing header name "
                " - this is undefined behaviour\n", c);
            tok->type = TOKEN_ERROR_L;
            return;
        }

        LexerStringAddC(&tok->data.string, ctx, c);
        c = Phase3Peek(ctx);
    }

    unsigned char last = Phase3Advance(ctx);

    if(last == END_OF_FILE) {
        fprintf(stderr, "Error: encountered error while parsing header name\n");
        tok->type = TOKEN_ERROR_L;
        return;
    } else if(last == '\n') {
        fprintf(stderr, "Error: encounterd new-line while parsing header name\n");
        tok->type = TOKEN_ERROR_L;
        return;
    }

    if(tok->data.string.count == 0) {
        fprintf(stderr, "Error: empty file name in header file name\n");
        tok->type = TOKEN_ERROR_L;
        return;
    }
}

// character -> preprocessor token conversion
static void Phase3Get(LexerToken* tok, TranslationContext* ctx) {
    SourceLocation* loc = memoryArrayPush(&ctx->locations);
    *loc = *ctx->phase3.currentLocation;
    loc->length = 0;
    ctx->phase3.currentLocation = loc;

    skipWhitespace(tok, ctx);

    tok->loc = loc;
    if(Phase3AtEnd(ctx)) {
        tok->type = TOKEN_EOF_L;
        return;
    }

    unsigned char c = Phase3AdvanceOverwrite(ctx);

    switch(c) {
        case '[': Phase3Make(tok, TOKEN_PUNC_LEFT_SQUARE); return;
        case ']': Phase3Make(tok, TOKEN_PUNC_RIGHT_SQUARE); return;
        case '(': Phase3Make(tok, TOKEN_PUNC_LEFT_PAREN); return;
        case ')': Phase3Make(tok, TOKEN_PUNC_RIGHT_PAREN); return;
        case '{': Phase3Make(tok, TOKEN_PUNC_LEFT_BRACE); return;
        case '}': Phase3Make(tok, TOKEN_PUNC_RIGHT_BRACE); return;
        case '?': Phase3Make(tok, TOKEN_PUNC_QUESTION); return;
        case ';': Phase3Make(tok, TOKEN_PUNC_SEMICOLON); return;
        case ',': Phase3Make(tok, TOKEN_PUNC_COMMA); return;
        case '~': Phase3Make(tok, TOKEN_PUNC_TILDE); return;

        case '*': Phase3Make(tok, Phase3Match(ctx, '=')?
            TOKEN_PUNC_STAR_EQUAL : TOKEN_PUNC_STAR); return;
        case '/': Phase3Make(tok, Phase3Match(ctx, '=')?
            TOKEN_PUNC_SLASH_EQUAL : TOKEN_PUNC_SLASH); return;
        case '^': Phase3Make(tok, Phase3Match(ctx, '=')?
            TOKEN_PUNC_CARET_EQUAL : TOKEN_PUNC_CARET); return;
        case '=': Phase3Make(tok, Phase3Match(ctx, '=')?
            TOKEN_PUNC_EQUAL_EQUAL : TOKEN_PUNC_EQUAL); return;
        case '!': Phase3Make(tok, Phase3Match(ctx, '=')?
            TOKEN_PUNC_BANG_EQUAL : TOKEN_PUNC_BANG); return;
        case '#': Phase3Make(tok, Phase3Match(ctx, '#')?
            TOKEN_PUNC_HASH_HASH : TOKEN_PUNC_HASH); return;
        case ':': Phase3Make(tok, Phase3Match(ctx, '>')?
            TOKEN_PUNC_COLON_GREATER : TOKEN_PUNC_COLON); return;

        case '+': Phase3Make(tok,
            Phase3Match(ctx, '+') ? TOKEN_PUNC_PLUS_PLUS :
            Phase3Match(ctx, '=') ? TOKEN_PUNC_PLUS_EQUAL :
            TOKEN_PUNC_PLUS); return;
        case '|': Phase3Make(tok,
            Phase3Match(ctx, '|') ? TOKEN_PUNC_OR_OR :
            Phase3Match(ctx, '=') ? TOKEN_PUNC_EQUAL :
            TOKEN_PUNC_OR); return;
        case '&': Phase3Make(tok,
            Phase3Match(ctx, '&') ? TOKEN_PUNC_AND_AND :
            Phase3Match(ctx, '=') ? TOKEN_PUNC_AND_EQUAL :
            TOKEN_PUNC_AND); return;

        case '-': Phase3Make(tok,
            Phase3Match(ctx, '>') ? TOKEN_PUNC_ARROW :
            Phase3Match(ctx, '-') ? TOKEN_PUNC_MINUS_MINUS :
            Phase3Match(ctx, '=') ? TOKEN_PUNC_MINUS_EQUAL :
            TOKEN_PUNC_MINUS); return;
        case '>': Phase3Make(tok,
            Phase3Match(ctx, '=') ? TOKEN_PUNC_GREATER_EQUAL :
            Phase3Match(ctx, '>') ? (
                Phase3Match(ctx, '=') ?
                TOKEN_PUNC_GREATER_GREATER_EQUAL:
                TOKEN_PUNC_GREATER_GREATER
            ) : TOKEN_PUNC_GREATER); return;

        case '<':
            if(ctx->phase3.mode == LEX_MODE_MAYBE_HEADER) {
                ParseHeaderName(ctx, tok, '>');
                return;
            }
            Phase3Make(tok,
                Phase3Match(ctx, '=') ? TOKEN_PUNC_LESS_EQUAL :
                Phase3Match(ctx, ':') ? TOKEN_PUNC_LESS_COLON :
                Phase3Match(ctx, '%') ? TOKEN_PUNC_LESS_PERCENT :
                Phase3Match(ctx, '<') ? (
                    Phase3Match(ctx, '=') ?
                    TOKEN_PUNC_LESS_LESS_EQUAL :
                    TOKEN_PUNC_LESS_LESS
                ) : TOKEN_PUNC_LESS);
            return;

        case '.':
            if(isDigit(ctx->phase3.peek)) break;
            if(ctx->phase3.peek == '.' && ctx->phase3.peekNext == '.') {
                Phase3Advance(ctx);
                Phase3Advance(ctx);
                Phase3Make(tok, TOKEN_PUNC_ELIPSIS);
            } else {
                Phase3Make(tok, TOKEN_PUNC_DOT);
            }
            return;
        case '%': Phase3Make(tok,
            Phase3Match(ctx, '=') ? TOKEN_PUNC_PERCENT_EQUAL :
            Phase3Match(ctx, '>') ? TOKEN_PUNC_PERCENT_GREATER :
            Phase3Match(ctx, ':') ? (
                ctx->phase3.peek == '%' && ctx->phase3.peekNext == ':' ?
                TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON :
                TOKEN_PUNC_PERCENT_COLON
            ) : TOKEN_PUNC_PERCENT); return;
    }

    unsigned char next = Phase3Peek(ctx);

    if(ctx->phase3.mode == LEX_MODE_MAYBE_HEADER && c == '"') {
        ParseHeaderName(ctx, tok, '"');
        return;
    }

    // string literals
    if(isStringLike(ctx, c, '"')) {
        ParseString(ctx, tok, c, '"');
        return;
    }

    // character literals
    if(isStringLike(ctx, c, '\'')) {
        ParseString(ctx, tok, c, '\'');
        return;
    }


    // identifier
    // is identifier start character or universal character name
    if(isNonDigit(c) || (c == '\\' && (next == 'u' || next == 'U'))) {

        // has the current character been consumed (true for first character,
        // false otherwise
        bool consumedCharacter = true;

        // initialisation
        tok->type = TOKEN_IDENTIFIER_L;
        LexerStringInit(&tok->data.string, ctx, 10);

        // while is identifier character or slash
        while(!Phase3AtEnd(ctx) && (isNonDigit(c) || isDigit(c) || c == '\\')) {

            // get next character, depending if the current one was consumed
            unsigned char next = consumedCharacter ? Phase3Peek(ctx) : Phase3PeekNext(ctx);

            // found \u or \U => universal character name
            if(c == '\\' && (next == 'u' || next == 'U')) {
                if(!consumedCharacter) Phase3Advance(ctx);
                ParseUniversalCharacterName(ctx, tok);
            } else if(c == '\\') {
                // found backslash not in escape sequence, will not be at
                // start so do not need to un-consume it
                // finish token, the backslash will be the next token
                break;
            } else {
                // regular character
                // if not already done, consume it and join to the identifier
                if(!consumedCharacter) Phase3Advance(ctx);
                LexerStringAddC(&tok->data.string, ctx, c);
            }

            // advance
            c = Phase3Peek(ctx);
            consumedCharacter = false;
        }

        HashNode* node = tableGet(ctx->phase3.hashNodes, tok->data.string.buffer, tok->data.string.count);
        if(node == NULL) {
            node = ArenaAlloc(sizeof(*node));
            node->name = *tok;
            node->type = NODE_VOID;
            node->hash = stringHash(tok->data.string.buffer, tok->data.string.count);
            node->macroExpansionEnabled = true;
            tableSet(ctx->phase3.hashNodes, tok->data.string.buffer, tok->data.string.count, node);
        }
        tok->data.node = node;

        return;
    }

    // pp-number
    if(isDigit(c) || c == '.') {
        tok->type = TOKEN_PP_NUMBER;
        LexerStringInit(&tok->data.string, ctx, 10);
        LexerStringAddC(&tok->data.string, ctx, c);

        unsigned char c = Phase3Peek(ctx);
        while(!Phase3AtEnd(ctx)) {
            char next = Phase3PeekNext(ctx);
            if((c == 'e' || c == 'E' || c == 'p' || c == 'P') &&
                (next == '+' || next == '-')) {
                Phase3Advance(ctx);
                Phase3Advance(ctx);
            } else if(isDigit(c) || isNonDigit(c) || c == '.') {
                Phase3Advance(ctx);
            } else {
                break;
            }

            LexerStringAddC(&tok->data.string, ctx, c);
            c = Phase3Peek(ctx);
        }
        return;
    }

    // default
    Phase3Make(tok, TOKEN_UNKNOWN_L);
    tok->data.character = c;
}

static void PredefinedMacros(TranslationContext* ctx) {
    ctx->phase3.hashNodes = ArenaAlloc(sizeof(Table));
    TABLE_INIT(*ctx->phase3.hashNodes, HashNode*);

    time_t currentTime = time(NULL);
    struct tm timeStruct;
    localtime_s(&timeStruct, &currentTime);

    char* stringTime = ArenaAlloc(sizeof(char)*9);
    strftime(stringTime, 9, "%H:%M:%S", &timeStruct);
    HashNode* time = ArenaAlloc(sizeof(HashNode));
    time->type = NODE_MACRO_STRING;
    time->as.string = stringTime;
    time->macroExpansionEnabled = true;
    time->hash = stringHash("__TIME__", 8);
    time->name.data.string.buffer = "__TIME__";
    time->name.data.string.count = 8;
    TABLE_SET(*ctx->phase3.hashNodes, "__TIME__", 8, time);

    char* stringDate = ArenaAlloc(sizeof(char) * 128);
    strftime(stringDate, 128, "%b %d %Y", &timeStruct);
    HashNode* date = ArenaAlloc(sizeof(HashNode));
    date->type = NODE_MACRO_STRING;
    date->as.string = stringDate;
    date->macroExpansionEnabled = true;
    date->hash = stringHash("__DATE__", 8);
    date->name.data.string.buffer = "__DATE__";
    date->name.data.string.count = 8;
    TABLE_SET(*ctx->phase3.hashNodes, "__DATE__", 8, date);

    HashNode* file = ArenaAlloc(sizeof(HashNode));
    file->type = NODE_MACRO_FILE;
    file->macroExpansionEnabled = true;
    file->hash = stringHash("__FILE__", 8);
    file->name.data.string.buffer = "__FILE__";
    file->name.data.string.count = 8;
    TABLE_SET(*ctx->phase3.hashNodes, "__FILE__", 8, file);

    HashNode* line = ArenaAlloc(sizeof(HashNode));
    line->type = NODE_MACRO_LINE;
    line->macroExpansionEnabled = true;
    line->hash = stringHash("__LINE__", 8);
    line->name.data.string.buffer = "__LINE__";
    line->name.data.string.count = 8;
    TABLE_SET(*ctx->phase3.hashNodes, "__LINE__", 8, line);

#define INT_MACRO(pre, n, post, value) \
    HashNode* n = ArenaAlloc(sizeof(HashNode)); \
    n->type = NODE_MACRO_INTEGER; \
    n->as.integer = (value); \
    n->macroExpansionEnabled = true; \
    n->hash = stringHash(#pre#n#post, strlen(#pre#n#post)); \
    n->name.data.string.buffer = #pre#n#post; \
    n->name.data.string.count = strlen(#pre#n#post); \
    TABLE_SET(*ctx->phase3.hashNodes, #pre#n#post, strlen(#pre#n#post), n);

    INT_MACRO(__, STDC, __, 1);
    INT_MACRO(__, STDC_HOSTED, __, 1);
    INT_MACRO(__, STDC_VERSION, __, 201112L);
    INT_MACRO(__, STDC_UTF_16, __, 1);
    INT_MACRO(__, STDC_UTF_32, __, 1);
    INT_MACRO(__, STDC_NO_ATOMICS, __, 1);
    INT_MACRO(__, STDC_NO_COMPLEX, __, 1);
    INT_MACRO(__, STDC_NO_THREADS, __, 1);
    INT_MACRO(__, STDC_NO_VLA, __, 1);
    INT_MACRO(__, STDC_LIB_EXT1, __, 201112L);
    INT_MACRO(__, x86_64_, _, 1);
    INT_MACRO(__, x86_64, , 1);
    INT_MACRO(W, IN32, , 1);
    INT_MACRO(_WI, N32, , 1);
    INT_MACRO(__W, IN32_, _, 1);
    INT_MACRO(__, WIN32_, _, 1);
    INT_MACRO(W, IN64, , 1);
    INT_MACRO(_WI, N64, , 1);
    INT_MACRO(__W, IN64_, _, 1);
    INT_MACRO(__, WIN64_, _, 1);

#undef INT_MACRO
}

// initialise the context for running phase 3
static void Phase3Initialise(TranslationContext* ctx) {
    Phase2Initialise(ctx);
    ctx->phase3.mode = LEX_MODE_NO_HEADER,
    ctx->phase3.peek = '\0',
    ctx->phase3.peekNext = '\0',
    ctx->phase3.currentLocation = memoryArrayPush(&ctx->locations);
    ctx->phase3.peekLoc = *ctx->phase3.currentLocation,
    ctx->phase3.peekNextLoc = *ctx->phase3.currentLocation,
    ctx->phase3.AtStart = true;
    ctx->phase3.getter = Phase3GetFromPhase2;

    if(ctx->phase3.hashNodes == NULL) {
        PredefinedMacros(ctx);
    }

    Phase3AdvanceOverwrite(ctx);
    Phase3AdvanceOverwrite(ctx);
}

// helper to run upto and including phase 3
void runPhase3(TranslationContext* ctx) {
    Phase3Initialise(ctx);
    LexerToken tok;
    while(Phase3Get(&tok, ctx), tok.type != TOKEN_EOF_L) {
        TokenPrint(ctx, &tok);
    }
}

// ------- //
// Phase 4 //
// ------- //
// macro expansion
// directive execution
// _Pragma expansion
// include resolution

static void Phase4Initialise(TranslationContext* ctx, TranslationContext* parent, bool phase3Req) {
    if(parent != NULL) {
        ctx->phase3.hashNodes = parent->phase3.hashNodes;
    } else {
        ctx->phase3.hashNodes = NULL;
    }

    ctx->phase4.mode = LEX_MODE_NO_HEADER;
    ctx->phase4.searchState = (IncludeSearchState){0};
    ctx->phase4.parent = parent;
    ctx->phase4.macroCtx = (MacroContext){0};
    if(parent != NULL) {
        ctx->trigraphs = parent->trigraphs;
        ctx->tabSize = parent->tabSize;
        ctx->debugPrint = parent->debugPrint;
        ctx->search = parent->search;
        ctx->pool = parent->pool;
        ctx->phase4.depth = parent->phase4.depth + 1;
    } else {
        ctx->phase4.depth = 0;
        ctx->phase4.previous.loc = &ctx->phase1Location;
    }

    if(phase3Req) Phase3Initialise(ctx);
    Phase3Get(&ctx->phase4.peek, ctx);
}

static bool Phase4AtEnd(TranslationContext* ctx) {
    return ctx->phase4.peek.type == TOKEN_EOF_L;
}

static LexerToken* Phase4Advance(LexerToken* tok, void* ctx) {
    TranslationContext* t = ctx;
    *tok = t->phase4.peek;
    Phase3Get(&t->phase4.peek, ctx);
    return tok;
}

static LexerToken* Phase4Peek(LexerToken* tok, void* ctx) {
    TranslationContext* t = ctx;
    *tok = t->phase4.peek;
    return tok;
}

static void Phase4SkipLine(LexerToken* tok, TranslationContext* ctx) {
    while(!Phase4AtEnd(ctx) && !ctx->phase4.peek.isStartOfLine) {
        Phase4Advance(tok, ctx);
    }
}

static void Phase4Get(LexerToken* tok, TranslationContext* ctx);
static bool includeFile(LexerToken* tok, TranslationContext* ctx, bool isUser, bool isNext) {
    Phase4Advance(tok, ctx);

    IncludeSearchState* state;
    if(isNext) {
        if(ctx->phase4.parent) {
            state = &ctx->phase4.parent->phase4.searchState;
        } else {
            state = &(IncludeSearchState){0};
            fprintf(stderr, "Warning: #include_next at top level\n");
        }
    } else {
        state = &ctx->phase4.searchState;
        *state = (IncludeSearchState){0};
    }

    const char* fileName;
    if(isUser) {
        fileName = IncludeSearchPathFindUser(state, &ctx->search, tok->data.string.buffer);
    } else {
        fileName = IncludeSearchPathFindSys(state, &ctx->search, tok->data.string.buffer);
    }

    if(fileName == NULL) {
        fprintf(stderr, "Error: Cannot resolve include\n");
        Phase4SkipLine(tok, ctx);
        return false;
    }

    // See n1570.5.2.4.1
    if(ctx->phase4.depth > 15) {
        fprintf(stderr, "Error: include depth limit reached\n");
        Phase4SkipLine(tok, ctx);
        return false;
    }
    TranslationContext* ctx2 = ArenaAlloc(sizeof(*ctx2));
    memset(ctx2, 0, sizeof(*ctx2));
    ctx->phase4.mode = LEX_MODE_INCLUDE;
    ctx->phase4.includeContext = ctx2;

    ctx2->phase4.previous = ctx->phase4.previous;
    TranslationContextInit(ctx2, ctx->pool, (const unsigned char*)fileName);
    Phase4Initialise(ctx2, ctx, true);
    Phase4Get(tok, ctx2);
    return true;
}

static bool parseInclude(LexerToken* tok, TranslationContext* ctx, bool isNext) {
    ctx->phase3.mode = LEX_MODE_MAYBE_HEADER;
    Phase4Advance(tok, ctx);
    ctx->phase3.mode = LEX_MODE_NO_HEADER;

    LexerToken* peek = &ctx->phase4.peek;
    bool retVal = false;
    if(peek->type == TOKEN_HEADER_NAME) {
        retVal = includeFile(tok, ctx, true, isNext);
    } else if(peek->type == TOKEN_SYS_HEADER_NAME) {
        retVal = includeFile(tok, ctx, false, isNext);
    } else {
        fprintf(stderr, "Error: macro #include not implemented\n");
        Phase4SkipLine(tok, ctx);
    }

    if(!ctx->phase4.peek.isStartOfLine) {
        fprintf(stderr, "Error: Unexpected token after include location\n");
        Phase4SkipLine(tok, ctx);
    }

    return retVal;
}

static void parseDefine(TranslationContext* ctx) {
    LexerToken name;
    Phase4Advance(&name, ctx); // consume "define"
    Phase4Advance(&name, ctx); // consume name to define

    if(name.type != TOKEN_IDENTIFIER_L) {
        fprintf(stderr, "Error: Unexpected token inside #define\n");
        Phase4SkipLine(&name, ctx);
        return;
    }

    HashNode* node = name.data.node;
    if(node->type != NODE_VOID) {
        // error produces too many errors - enable when #if etc implemented
        // fprintf(stderr, "Error: redefinition of existing macro\n");
        Phase4SkipLine(&name, ctx);
        return;
    }

    LexerToken* tok = &ctx->phase4.peek;

    if(tok->type == TOKEN_PUNC_LEFT_PAREN && !tok->whitespaceBefore) {
        node->type = NODE_MACRO_FUNCTION;
        ARRAY_ALLOC(LexerToken, node->as.function, argument);
        ARRAY_ALLOC(LexerToken, node->as.function, replacement);
        node->as.function.isVariadac = false;

        LexerToken currentToken;
        Phase4Advance(&currentToken, ctx); // consume '('

        while(!tok->isStartOfLine) {
            Phase4Advance(&currentToken, ctx);
            if(currentToken.type == TOKEN_PUNC_ELIPSIS) {
                node->as.function.isVariadac = true;
                break;
            }
            if(currentToken.type != TOKEN_IDENTIFIER_L) {
                break;
            }
            ARRAY_PUSH(node->as.function, argument, currentToken);
            Phase4Advance(&currentToken, ctx);
            if(currentToken.type != TOKEN_PUNC_COMMA) {
                break;
            }
        }

        if(currentToken.type != TOKEN_PUNC_RIGHT_PAREN) {
            fprintf(stderr, "Error: unexpected token at end of macro argument list\n");
            Phase4SkipLine(&currentToken, ctx);
            return;
        }

    } else {
        node->type = NODE_MACRO_OBJECT;
        ARRAY_ALLOC(LexerToken, node->as.object, item);

        if(!tok->whitespaceBefore) {
            fprintf(stderr, "Error: ISO C requires whitespace after macro name\n");
        }
    }

    size_t i = 0;
    while(!tok->isStartOfLine) {
        LexerToken* addr;
        if(node->type == NODE_MACRO_FUNCTION) {
            addr = ARRAY_PUSH_PTR(node->as.function, replacement);
        } else {
            addr = ARRAY_PUSH_PTR(node->as.object, item);
        }
        addr->type = TOKEN_ERROR_L;
        Phase4Advance(addr, ctx);

        if(i == 0) {
            addr->indent = 0;
        }


        // replace identifiers that correspond to an argument with a token
        // representing the index of that argument
        if(addr->type == TOKEN_IDENTIFIER_L && node->type == NODE_MACRO_FUNCTION) {
            for(unsigned int i = 0; i < node->as.function.argumentCount; i++) {
                HashNode* current = addr->data.node;
                HashNode* check = node->as.function.arguments[i].data.node;
                if(current->name.data.string.count == check->name.data.string.count && current->hash == check->hash) {
                    addr->type = TOKEN_MACRO_ARG;
                    addr->data.integer = i;
                    break;
                }
            }
        }

        if(addr->type == TOKEN_IDENTIFIER_L &&
        (node->type == NODE_MACRO_OBJECT || (node->type == NODE_MACRO_FUNCTION && !node->as.function.isVariadac)) &&
        addr->data.node->name.data.string.count == 12 &&
        addr->data.node->hash == stringHash("__VA_ARGS__", 12)) {
            fprintf(stderr, "Error: __VA_ARGS__ is invalid unless in a variadac function macro\n");
        }

        i++;
    }
}

static void parseUndef(TranslationContext* ctx) {
    LexerToken name;
    Phase4Advance(&name, ctx); // consume "undef"
    Phase4Advance(&name, ctx); // consume name to undef

    if(name.type != TOKEN_IDENTIFIER_L) {
        fprintf(stderr, "Error: Unexpected token inside #undef\n");
        Phase4SkipLine(&name, ctx);
        return;
    }

    name.data.node->type = NODE_VOID;
}

static LexerToken* TokenListAdvance(LexerToken* tok, void* ctx) {
    TokenList* list = ctx;
    if(list->itemCount > 0) {
        *tok = list->items[0];
        list->items++;
        list->itemCount--;
    } else {
        tok->type = TOKEN_EOF_L;
    }
    return tok;
}

static LexerToken* TokenListPeek(LexerToken* tok, void* ctx) {
    TokenList* list = ctx;
    if(list->itemCount > 0) {
        *tok = list->items[0];
    } else {
        tok->type = TOKEN_EOF_L;
    }
    return tok;
}

static bool ReturnFalse(void __UNUSED_PARAM(*ctx)) {
    return false;
}

typedef LexerToken* (*Phase4GetterFn)(LexerToken* tok, void* ctx);
typedef bool (*Phase4BoolFn)(void* ctx);

typedef struct JointTokenStream {
    TokenList* list;
    Phase4GetterFn secondAdvance;
    Phase4GetterFn secondPeek;
    void* second;
    HashNode* macroContext;
} JointTokenStream;

static LexerToken* JointTokenAdvance(LexerToken* tok, void* ctx) {
    JointTokenStream* stream = ctx;
    TokenListAdvance(tok, stream->list);
    if(tok->type == TOKEN_EOF_L) {
        stream->macroContext->macroExpansionEnabled = true;
        stream->secondAdvance(tok, stream->second);
    }
    return tok;
}

static LexerToken* JointTokenPeek(LexerToken* tok, void* ctx) {
    JointTokenStream* stream = ctx;
    TokenListPeek(tok, stream->list);
    if(tok->type == TOKEN_EOF_L) {
        stream->secondPeek(tok, stream->second);
    }
    return tok;
}

static bool JointTokenEarlyExit(void* ctx) {
    JointTokenStream* stream = ctx;
    return stream->list->itemCount == 0;
}

static void Phase4Get(LexerToken* tok, TranslationContext* ctx);

typedef enum EnterContextResult {
    CONTEXT_MACRO_TOKEN,
    CONTEXT_MACRO_NULL,
    CONTEXT_NOT_MACRO,
} EnterContextResult;

/*

Algorithm:
When getting token from phase 4
when token is macro
- if special, return one token
- if object, expand replacement list, set buffer, return first token
- if function, expand arguments, substitute, expand, return first token

if first token was null (i.e. expanded to list len = 0), return null

*/

#define EXPAND_LINE_FN \
static EnterContextResult ExpandSingleMacro( \
    LexerToken* tok, \
    TranslationContext* ctx, \
    MacroContext* macro, \
    Phase4GetterFn advance, \
    Phase4GetterFn peek, \
    void* getCtx \
)

EXPAND_LINE_FN;

static void ExpandTokenList(
    TranslationContext* ctx,
    TokenList* result,
    Phase4GetterFn advance,
    Phase4GetterFn peek,
    Phase4BoolFn earlyExit,
    void* getCtx) {
    ARRAY_ALLOC(LexerToken, *result, item);
    LexerToken* t = ARRAY_PUSH_PTR(*result, item);
    while(true) {
        advance(t, getCtx);

        // expand the new macro
        MacroContext macro = {0};
        ExpandSingleMacro(t, ctx, &macro, advance, peek, getCtx);

        // append the new tokens to the current buffer
        for(unsigned int i = 0; i < macro.tokenCount; i++){
            ARRAY_PUSH(*result, item, macro.tokens[i]);
        }

        if(t->type == TOKEN_EOF_L) {
            result->itemCount--;
            break;
        }
        if(earlyExit(getCtx)) {
            break;
        }
        t = ARRAY_PUSH_PTR(*result, item);
    }
}

// expand an object macro
static EnterContextResult ParseObjectMacro(
    MacroContext* macro,
    LexerToken* tok,
    TranslationContext* ctx,
    Phase4GetterFn advance,
    Phase4GetterFn peek,
    void* getCtx
) {
    if(tok->data.node->as.object.itemCount <= 0) {
        return CONTEXT_MACRO_NULL;
    }

    // copy token list otherwise the tokens are globably removed
    // from the macro's hash node.
    TokenList objCopy = tok->data.node->as.object;
    JointTokenStream stream = {
        .list = &objCopy,
        .macroContext = tok->data.node,
        .second = getCtx,
        .secondAdvance = advance,
        .secondPeek = peek,
    };
    TokenList result;
    tok->data.node->macroExpansionEnabled = false;
    ExpandTokenList(ctx, &result, JointTokenAdvance, JointTokenPeek, JointTokenEarlyExit, &stream);
    tok->data.node->macroExpansionEnabled = true;

    if(result.itemCount <= 0) {
        return CONTEXT_MACRO_NULL;
    }

    if(result.itemCount > 0) {
        result.items[0].indent = tok->indent;
        result.items[0].renderStartOfLine = tok->renderStartOfLine;
        result.items[0].whitespaceBefore = tok->whitespaceBefore;
    }

    *tok = result.items[0];
    macro->tokens = result.items + 1;
    macro->tokenCount = result.itemCount - 1;

    return CONTEXT_MACRO_TOKEN;
}

static EnterContextResult ParseFunctionMacro(
    MacroContext* macro,
    LexerToken* tok,
    TranslationContext* ctx,
    Phase4GetterFn advance,
    Phase4GetterFn peek,
    void* getCtx
) {
    LexerToken peekToken;
    if(peek(&peekToken, getCtx)->type != TOKEN_PUNC_LEFT_PAREN) {
        return CONTEXT_NOT_MACRO;
    }

    LexerToken lparen;
    advance(&lparen, getCtx);

    TokenListList args;
    ARRAY_ALLOC(TokenList, args, item);

    // gather arguments and macro expand them
    LexerToken* next;
    while(true) {
        TokenList arg;
        ARRAY_ALLOC(LexerToken, arg, item);

        int bracketDepth = 0;
        while(true) {
            next = ARRAY_PUSH_PTR(arg, item);
            advance(next, getCtx);

            if(next->type == TOKEN_PUNC_COMMA && bracketDepth == 0) {
                arg.itemCount--;
                break;
            } else if(next->type == TOKEN_PUNC_LEFT_PAREN) {
                bracketDepth++;
            } else if(next->type == TOKEN_PUNC_RIGHT_PAREN) {
                if(bracketDepth == 0) {
                    arg.itemCount--;
                    break;
                } else {
                    bracketDepth--;
                }
            } else if(next->type == TOKEN_EOF_L) {
                break;
            }
        }

        if(arg.itemCount > 0) {
            arg.items[0].indent = 0;
        }

        TokenList* result = ARRAY_PUSH_PTR(args, item);
        ExpandTokenList(ctx, result, TokenListAdvance, TokenListPeek, ReturnFalse, &arg);

        if(next->type == TOKEN_PUNC_RIGHT_PAREN || next->type == TOKEN_EOF_L) {
            break;
        }
    }

    if(next->type != TOKEN_PUNC_RIGHT_PAREN) {
        fprintf(stderr, "Error: Unterminated function macro call [%lld:%lld]\n", tok->loc->line, tok->loc->column);
        return CONTEXT_MACRO_NULL;
    }

    TokenList substituted;
    ARRAY_ALLOC(LexerToken, substituted, item);

    // substitute arguments into replacement list
    FnMacro* fn = &tok->data.node->as.function;
    for(unsigned int i = 0; i < fn->replacementCount; i++) {
        LexerToken* tok = &fn->replacements[i];
        if(tok->type == TOKEN_MACRO_ARG) {
            if(tok->data.integer > args.itemCount) {
                fprintf(stderr, "Error: bad arguments\n");
                continue;
            }
            TokenList arg = args.items[tok->data.integer];
            for(unsigned int j = 0; j < arg.itemCount; j++) {
                ARRAY_PUSH(substituted, item, arg.items[j]);
                if(j == 0) {
                    substituted.items[substituted.itemCount-1].indent = tok->indent;
                    substituted.items[substituted.itemCount-1].renderStartOfLine = tok->renderStartOfLine;
                    substituted.items[substituted.itemCount-1].whitespaceBefore = tok->whitespaceBefore;
                }
            }
        } else {
            ARRAY_PUSH(substituted, item, *tok);
        }
    }

    if(substituted.itemCount <= 0) {
        return CONTEXT_MACRO_NULL;
    }

    // macro expand the substituted list

    JointTokenStream stream = {
        .list = &substituted,
        .macroContext = tok->data.node,
        .second = getCtx,
        .secondAdvance = advance,
        .secondPeek = peek,
    };
    TokenList result;
    tok->data.node->macroExpansionEnabled = false;
    ExpandTokenList(ctx, &result, JointTokenAdvance, JointTokenPeek, JointTokenEarlyExit, &stream);
    tok->data.node->macroExpansionEnabled = true;

    if(result.itemCount > 0) {
        result.items[0].indent = tok->indent;
        result.items[0].renderStartOfLine = tok->renderStartOfLine;
        result.items[0].whitespaceBefore = tok->whitespaceBefore;
    }

    if(result.itemCount <= 0) {
        return CONTEXT_MACRO_NULL;
    }

    macro->tokens = result.items + 1;
    macro->tokenCount = result.itemCount - 1;
    *tok = result.items[0];

    return CONTEXT_MACRO_TOKEN;
}


// macro expands a token
// returns a buffer containing the expaned tokens
// assumes the first token has already been consumed from the relavant stream
EXPAND_LINE_FN {
    if(tok->type != TOKEN_IDENTIFIER_L || tok->data.node->type == NODE_VOID || !tok->data.node->macroExpansionEnabled) {
        return CONTEXT_NOT_MACRO;
    }

    switch(tok->data.node->type) {
        case NODE_MACRO_OBJECT:
            return ParseObjectMacro(macro, tok, ctx, advance, peek, getCtx);
        case NODE_MACRO_FUNCTION:
            return ParseFunctionMacro(macro, tok, ctx, advance, peek, getCtx);
        case NODE_MACRO_INTEGER:
            tok->type = TOKEN_INTEGER_L;
            tok->data.integer = tok->data.node->as.integer;
            macro->tokenCount = 0;
            return CONTEXT_MACRO_TOKEN;
        case NODE_MACRO_STRING: {
            tok->type = TOKEN_STRING_L;
            LexerString str;
            str.buffer = (char*)tok->data.node->as.string;
            str.count = strlen(tok->data.node->as.string);
            str.type = STRING_NONE;
            tok->data.string = str;
            macro->tokenCount = 0;
            return CONTEXT_MACRO_TOKEN;
        }
        case NODE_MACRO_LINE: {
            tok->type = TOKEN_INTEGER_L;
            tok->data.integer = ctx->phase4.previous.loc->line;
            macro->tokenCount = 0;
            return CONTEXT_MACRO_TOKEN;
        }
        case NODE_MACRO_FILE: {
            tok->type = TOKEN_STRING_L;
            LexerString str;
            str.buffer = (char*)ctx->phase4.previous.loc->fileName;
            str.count = strlen((char*)ctx->phase4.previous.loc->fileName);
            str.type = STRING_NONE;
            tok->data.string = str;
            macro->tokenCount = 0;
            return CONTEXT_MACRO_TOKEN;
        }
        case NODE_VOID:
            fprintf(stderr, "enter ctx err\n"); exit(1);
    }

    fprintf(stderr, "node type error\n");
    exit(1);
}

// called on new identifier to be expanded, if needed parses function call
// runs all the expansion and puts the fully expanded macro into a buffer
// returns the first item of the buffer of NULL_TOKEN
static EnterContextResult __attribute__((warn_unused_result)) EnterMacroContext(LexerToken* tok, TranslationContext* ctx) {
    return ExpandSingleMacro(tok, ctx, &ctx->phase4.macroCtx, Phase4Advance, Phase4Peek, ctx);
}

// gets next token from existing buffer,
// does not do any macro expansion
static void AdvanceMacroContext(LexerToken* tok, TranslationContext* ctx) {
    *tok = ctx->phase4.macroCtx.tokens[0];
    ctx->phase4.macroCtx.tokens++;
    ctx->phase4.macroCtx.tokenCount--;
}

// stops using a buffer to store the expanded macro, if at the end of the macro
static void TryExitMacroContext(TranslationContext* ctx) {
    if(ctx->phase4.macroCtx.tokens != NULL && ctx->phase4.macroCtx.tokenCount == 0) {
        ctx->phase4.macroCtx.tokens= NULL;
    }
}

static void Phase4Get(LexerToken* tok, TranslationContext* ctx) {
    if(ctx->phase4.mode == LEX_MODE_INCLUDE) {
        Phase4Get(tok, ctx->phase4.includeContext);
        if(tok->type == TOKEN_EOF_L) {
            ctx->phase4.mode = LEX_MODE_NO_HEADER;
        } else {
            ctx->phase4.previous = *tok;
            return;
        }
    }

    TryExitMacroContext(ctx);
    if(ctx->phase4.macroCtx.tokens != NULL) {
        AdvanceMacroContext(tok, ctx);
        return;
    }

    tok->type = TOKEN_ERROR_L;

    // loop over multiple directives (instead of reccursion)
    while(tok->type != TOKEN_EOF_L) {
        Phase4Advance(tok, ctx);

        if((tok->type == TOKEN_PUNC_HASH || tok->type == TOKEN_PUNC_PERCENT_COLON) && tok->isStartOfLine) {
            LexerToken* peek = &ctx->phase4.peek;

            if(peek->isStartOfLine) {
                // NULL directive
                continue;
            }

            if(peek->type != TOKEN_IDENTIFIER_L) {
                fprintf(stderr, "Error: Unexpected token at start of directive\n");
                Phase4SkipLine(tok, ctx);
                continue;
            }

            uint32_t hash = peek->data.node->hash;
            size_t len = peek->data.node->name.data.string.count;

            if(len == 7 && hash == stringHash("include", 7)) {
                bool success = parseInclude(tok, ctx, false);
                if(success) {
                    ctx->phase4.previous = *tok;
                    return;
                }
                continue;
            } else if(len == 12 && hash == stringHash("include_next", 12)) {
                // See https://gcc.gnu.org/onlinedocs/cpp/Wrapper-Headers.html
                bool success = parseInclude(tok, ctx, true);
                if(success) {
                    ctx->phase4.previous = *tok;
                    return;
                }
                continue;
            } else if(len == 6 && hash == stringHash("define", 6)) {
                parseDefine(ctx);
                continue;
            } else if(len == 5 && hash == stringHash("undef", 5)) {
                parseUndef(ctx);
                continue;
            }

            //fprintf(stderr, "Error: Unknown preprocessing directive\n");
            //Phase4SkipLine(tok, ctx);
            ctx->phase4.previous = *tok;
            return;
        }

        if(EnterMacroContext(tok, ctx) == CONTEXT_MACRO_NULL) {
            continue;
        }

        if(tok->type == TOKEN_IDENTIFIER_L &&
        tok->data.node->name.data.string.count == 12 &&
        tok->data.node->hash == stringHash("__VA_ARGS__", 12)) {
            fprintf(stderr, "Warning: Unexpected identifier __VA_ARGS__ outisde of variadac function macro");
        }
        ctx->phase4.previous = *tok;
        return;
    }
}

// helper to run upto and including phase 4
void runPhase4(TranslationContext* ctx) {
    Phase4Initialise(ctx, NULL, true);
    LexerToken tok;
    while(Phase4Get(&tok, ctx), tok.type != TOKEN_EOF_L) {
        TokenPrint(ctx, &tok);
    }
    printf("\n");
}
