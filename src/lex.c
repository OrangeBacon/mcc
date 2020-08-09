#include "lex.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "file.h"

typedef struct LexerString {
    char* buffer;
    size_t capacity;
    size_t count;
} LexerString;

// What sort of token is it, used for both preprocessor and regular
// tokens, however not all values are valid in each scenario
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
    TOKEN_PUNC_OR,
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
    TOKEN_PUNC_LESS_COLON, // [
    TOKEN_PUNC_COLON_GREATER, // ]
    TOKEN_PUNC_LESS_PERCENT, // {
    TOKEN_PUNC_PERCENT_GREATER, // }
    TOKEN_PUNC_PERCENT_COLON, // #
    TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON, // ##
    TOKEN_HEADER_NAME,
    TOKEN_PP_NUMBER,
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_FLOATING,
    TOKEN_CHARACTER,
    TOKEN_STRING,
    TOKEN_UNKNOWN,
    TOKEN_EOF,
} TokenType;

// the smallest non-character unit of code
typedef struct Token {
    TokenType type;

    // required to prevent extra macro expansion
    bool isStartOfLine;

    // required to tell the difference between
    // #define(a)  - function macro
    // #define (a) - value macro
    bool whitespaceBefore;

    // stores the quantity of whitespace before the token on the same
    // line as the token (note whitespaceBefore can be true, while this
    // is equal to 0, so need both of them)
    size_t indent;

    // where the token is in the source file, used for emmitting errors
    // and debugging infomation
    SourceLocation* loc;

    // optional data about the token, what is stored here is dependant
    // upon token type, could be nothing/uninitialised
    union {
        intmax_t integer;
        double floating;
        LexerString string;
        char character;
    } data;
} Token;

static void TokenPrint(TranslationContext* ctx, Token* tok) {
    if(ctx->debugPrint) {
        printf("%llu:%llu", tok->loc->line, tok->loc->column);
        if(tok->isStartOfLine) printf(" bol");
        if(tok->whitespaceBefore) printf(" white=%llu", tok->indent);
        printf(" token=%d", tok->type);
        printf(" data(%llu) ", tok->loc->length);
    } else {
        if(tok->isStartOfLine && !ctx->tokenPrinterAtStart) {
            printf("\n");
        }
        ctx->tokenPrinterAtStart = false;
        if(tok->whitespaceBefore) {
            printf("%*s", (int)tok->indent, "");
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
        case TOKEN_HEADER_NAME: printf("<%s>", tok->data.string.buffer); break;
        case TOKEN_PP_NUMBER: printf("%s", tok->data.string.buffer); break;
        case TOKEN_IDENTIFIER: printf("%s", tok->data.string.buffer); break;
        case TOKEN_INTEGER: printf("%llu", tok->data.integer); break;
        case TOKEN_FLOATING: printf("%f", tok->data.floating); break;
        case TOKEN_CHARACTER: printf("'%s'", tok->data.string.buffer); break;
        case TOKEN_STRING: printf("\"%s\"", tok->data.string.buffer); break;
        case TOKEN_UNKNOWN: printf("%c", tok->data.character); break;
        case TOKEN_EOF: break;
    }

    if(ctx->debugPrint) {
        printf("\n");
    }
}

static void LexerStringInit(LexerString* str, TranslationContext* ctx, size_t size) {
    str->buffer = memoryArrayPushN(&ctx->stringArr, size + 1);
    str->buffer[0] = '\0';
    str->capacity = size;
    str->count = 0;
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
void TranslationContextInit(TranslationContext* ctx, MemoryPool* pool, const char* fileName) {
    ctx->phase1source = readFileLen(fileName, &ctx->phase1sourceLength);

    memoryArrayAlloc(&ctx->stringArr, pool, 4*MiB, sizeof(char));
    memoryArrayAlloc(&ctx->locations, pool, 128*MiB, sizeof(SourceLocation));

    ctx->tokenPrinterAtStart = true;
    ctx->phase1consumed = 0;
    ctx->phase1IgnoreNewLine = '\0';
    ctx->phase1Location = (SourceLocation) {
        .fileName = fileName,
        .column = 0,
        .length = 0,
        .line = 1,
    };
    ctx->phase2Previous = EOF;
    ctx->phase3currentLocation = memoryArrayPush(&ctx->locations);
}

// ------- //
// Phase 1 //
// ------- //
// Physical source characters -> source character set (unimplemented)
// Trigraph conversion

// return the next character without consuming it
static char Phase1Peek(TranslationContext* ctx) {
    if(ctx->phase1consumed >= ctx->phase1sourceLength) return EOF;
    return ctx->phase1source[ctx->phase1consumed];
}

// return the character after next without consuming its
static char Phase1PeekNext(TranslationContext* ctx) {
    if(ctx->phase1consumed - 1 >= ctx->phase1sourceLength) return EOF;
    return ctx->phase1source[ctx->phase1consumed + 1];
}

// process a newline character ('\n' or '\r') to check how to advance the
// line counter.  If '\n' encountered, a following '\r' will not change the
// counter, if '\r', the '\n' will not update.  This allows multiple possible
// line endings: "\n", "\r", "\n\r", "\r\n" that are all considered one line end
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

// get the next character from the previous phase
// increases the SourcLocation's length with the new character
static char Phase1Advance(TranslationContext* ctx) {
    if(ctx->phase1consumed >= ctx->phase1sourceLength) return EOF;
    ctx->phase1consumed++;
    ctx->phase1Location.length++;
    ctx->phase1Location.column++;
    char c = ctx->phase1source[ctx->phase1consumed - 1];
    Phase1NewLine(ctx, c);
    return c;
}

// get the next character from the previous phase
// sets the SourceLocation to begin from the new character's start
static char Phase1AdvanceOverwrite(TranslationContext* ctx) {
    if(ctx->phase1consumed >= ctx->phase1sourceLength) return EOF;
    ctx->phase1consumed++;
    ctx->phase1Location.length = 1;
    ctx->phase1Location.column++;
    char c = ctx->phase1source[ctx->phase1consumed - 1];
    Phase1NewLine(ctx, c);
    return c;
}

// map trigraph "??{index}" -> real character
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
static char Phase2Advance(TranslationContext* ctx) {
    char ret = ctx->phase2Peek;
    ctx->phase2CurrentLoc.length += ctx->phase2PeekLoc.length;
    ctx->phase2Peek = Phase1Get(ctx);
    ctx->phase2PeekLoc = ctx->phase1Location;
    return ret;
}

// get the next character from the previous phase
// sets the SourceLocation to begin from the new character's start
static char Phase2AdvanceOverwrite(TranslationContext* ctx) {
    char ret = ctx->phase2Peek;
    ctx->phase2CurrentLoc = ctx->phase2PeekLoc;
    ctx->phase2Peek = Phase1Get(ctx);
    ctx->phase2PeekLoc = ctx->phase1Location;
    return ret;
}

// return the next character without consuming it
static char Phase2Peek(TranslationContext* ctx) {
    return ctx->phase2Peek;
}

// implements backslash-newline skipping
// if the next character after a backslash/newline is another backslash/newline
// then that should be skiped as well iteratively until the next real character
// emits error for backslash-EOF and /[^\n]/-EOF
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

// setup phase2's buffers
static void Phase2Initialise(TranslationContext* ctx) {
    Phase2AdvanceOverwrite(ctx);
}

// helper to run upto and including phase 2
void runPhase2(TranslationContext* ctx) {
    Phase2Initialise(ctx);
    char c;
    while((c = Phase2Get(ctx)) != EOF) {
        putchar(c);
    }
}

// ------- //
// Phase 3 //
// ------- //
// characters -> preprocessing tokens (unimplemented)
// comments -> whitespace
// tracking begining of line + prior whitespace in tokens

// get the next character from the previous phase
// increases the SourcLocation's length with the new character
static char Phase3Advance(TranslationContext* ctx) {
    char ret = ctx->phase3peek;
    ctx->phase3currentLocation->length += ctx->phase3peekLoc.length;

    ctx->phase3peek = ctx->phase3peekNext;
    ctx->phase3peekLoc = ctx->phase3peekNextLoc;
    ctx->phase3peekNext = Phase2Get(ctx);
    ctx->phase3peekNextLoc = ctx->phase2CurrentLoc;

    return ret;
}

// get the next character from the previous phase
// sets the SourceLocation to begin from the new character's start
static char Phase3AdvanceOverwrite(TranslationContext* ctx) {
    char ret = ctx->phase3peek;
    *ctx->phase3currentLocation = ctx->phase3peekLoc;

    ctx->phase3peek = ctx->phase3peekNext;
    ctx->phase3peekLoc = ctx->phase3peekNextLoc;
    ctx->phase3peekNext = Phase2Get(ctx);
    ctx->phase3peekNextLoc = ctx->phase2CurrentLoc;
    return ret;
}

// return the next character without consuming it
static char Phase3Peek(TranslationContext* ctx) {
    return ctx->phase3peek;
}

// return the character after next without consuming it
static char Phase3PeekNext(TranslationContext* ctx) {
    return ctx->phase3peekNext;
}

// has phase 3 reached the end of the file?
static bool Phase3AtEnd(TranslationContext* ctx) {
    return ctx->phase3peek == EOF;
}

// skip a new line ("\n", "\r", "\n\r", "\r\n") and set that the token is
// at the begining of a line
static void Phase3NewLine(Token* tok, TranslationContext* ctx, char c) {
    Phase3Advance(ctx);
    if(Phase3Peek(ctx) == c) {
        Phase3Advance(ctx);
    }
    tok->isStartOfLine = true;
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
static void skipWhitespace(Token* tok, TranslationContext* ctx) {
    tok->whitespaceBefore = false;
    tok->isStartOfLine = false;
    tok->indent = 0;

    if(ctx->phase3AtStart) tok->isStartOfLine = true;

    while(true) {
        char c = Phase3Peek(ctx);
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
                char next = Phase3PeekNext(ctx);
                if(next == '/') {
                    // single line comment (//)
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
                    // multi line comment (/**/)
                    Phase3AdvanceOverwrite(ctx);
                    Phase3Advance(ctx);
                    tok->indent += 3;
                    while(!Phase3AtEnd(ctx)) {
                        if(Phase3Peek(ctx) == '*' && Phase3PeekNext(ctx) == '/') {
                            break;
                        }
                        if(Phase3Peek(ctx) == '\n') Phase3NewLine(tok, ctx, '\r');
                        if(Phase3Peek(ctx) == '\r') Phase3NewLine(tok, ctx, '\r');
                        tok->indent++;
                        Phase3Advance(ctx);
                    }
                    if(Phase3AtEnd(ctx)) {
                        fprintf(stderr, "Error: Unterminated multi-line comment at %lld:%lld\n", ctx->phase3currentLocation->line, ctx->phase3currentLocation->column);
                        exit(1);
                    }
                    Phase3Advance(ctx);
                    Phase3Advance(ctx);
                    tok->indent += 2;
                    tok->whitespaceBefore = true;
                } else {
                    return;
                }
            }; break;

            default: return;
        }
    }
}

static bool Phase3Match(TranslationContext* ctx, char c) {
    if(Phase3AtEnd(ctx)) return false;
    if(Phase3Peek(ctx) != c) return false;
    Phase3Advance(ctx);
    return true;
}

// this wrapper only exists because of wierd precedence with
// a ? (b = c) : (b = d) requiring those parentheses, so this
// is more clear to read
static void Phase3Make(Token* tok, TokenType type) {
    tok->type = type;
}

static bool isNonDigit(char c) {
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

// character -> preprocessor token conversion
static void Phase3Get(Token* tok, TranslationContext* ctx) {
    SourceLocation* loc = memoryArrayPush(&ctx->locations);
    *loc = *ctx->phase3currentLocation;
    loc->length = 0;
    ctx->phase3currentLocation = loc;

    skipWhitespace(tok, ctx);

    tok->loc = loc;
    if(Phase3AtEnd(ctx)) {
        tok->type = TOKEN_EOF;
        return;
    }

    char c = Phase3AdvanceOverwrite(ctx);

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

        // TODO: header mode, eg <stdio.h>
        case '<': Phase3Make(tok,
            Phase3Match(ctx, '=') ? TOKEN_PUNC_LESS_EQUAL :
            Phase3Match(ctx, ':') ? TOKEN_PUNC_LESS_COLON :
            Phase3Match(ctx, '%') ? TOKEN_PUNC_LESS_PERCENT :
            Phase3Match(ctx, '<') ? (
                Phase3Match(ctx, '=') ?
                TOKEN_PUNC_LESS_LESS_EQUAL :
                TOKEN_PUNC_LESS_LESS
            ) : TOKEN_PUNC_LESS); return;

        // TODO: numbers, eg .05
        case '.':
            if(isDigit(ctx->phase3peek)) break;
            Phase3Make(tok,
                ctx->phase3peek == '.' && ctx->phase3peekNext == '.' ?
                TOKEN_PUNC_ELIPSIS : TOKEN_PUNC_DOT);
            return;
        case '%': Phase3Make(tok,
            Phase3Match(ctx, '=') ? TOKEN_PUNC_PERCENT_EQUAL :
            Phase3Match(ctx, '>') ? TOKEN_PUNC_PERCENT_GREATER :
            Phase3Match(ctx, ':') ? (
                ctx->phase3peek == '%' && ctx->phase3peekNext == ':' ?
                TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON :
                TOKEN_PUNC_PERCENT_COLON
            ) : TOKEN_PUNC_PERCENT); return;
    }

    // identifier
    if(isNonDigit(c)) {
        tok->type = TOKEN_IDENTIFIER;
        LexerStringInit(&tok->data.string, ctx, 10);
        LexerStringAddC(&tok->data.string, ctx, c);

        char c = Phase3Peek(ctx);
        while(!Phase3AtEnd(ctx) && (isNonDigit(c) || isDigit(c))) {
            Phase3Advance(ctx);
            LexerStringAddC(&tok->data.string, ctx, c);
            c = Phase3Peek(ctx);
        }
        return;
    }

    // pp-number
    if(isDigit(c) || c == '.') {
        tok->type = TOKEN_PP_NUMBER;
        LexerStringInit(&tok->data.string, ctx, 10);
        LexerStringAddC(&tok->data.string, ctx, c);

        char c = Phase3Peek(ctx);
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
    Phase3Make(tok, TOKEN_UNKNOWN);
    tok->data.character = c;
}

static void Phase3Initialise(TranslationContext* ctx) {
    Phase2Initialise(ctx);
    ctx->phase3mode = LEX_MODE_NO_HEADER,
    ctx->phase3peek = '\0',
    ctx->phase3peekNext = '\0',
    ctx->phase3peekLoc = *ctx->phase3currentLocation,
    ctx->phase3peekNextLoc = *ctx->phase3currentLocation,
    ctx->phase3AtStart = true;
    Phase3AdvanceOverwrite(ctx);
    Phase3AdvanceOverwrite(ctx);
}

void runPhase3(TranslationContext* ctx) {
    Phase3Initialise(ctx);
    Token tok;
    while(Phase3Get(&tok, ctx), tok.type != TOKEN_EOF) {
        TokenPrint(ctx, &tok);
    }
}
