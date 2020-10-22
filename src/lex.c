#include "lex.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "file.h"

#include "lextoken.h"
#include "lextoken.h"

// End of file
// default is -1, which is the same as 0xff, but type conversions work
// better with unsigned char this way
#define END_OF_FILE 0xff

// Setup function
void TranslationContextInit(TranslationContext* ctx, MemoryPool* pool) {
    ctx->pool = pool;

    memoryArrayAlloc(&ctx->stringArr, pool, 4*MiB, sizeof(unsigned char));
    memoryArrayAlloc(&ctx->locations, pool, 128*MiB, sizeof(SourceLocation));
}

// ------- //
// Phase 1 //
// ------- //
// Physical source characters -> source character set (unimplemented)
// Trigraph conversion
// internaly uses out parameter instead of END_OF_FILE to avoid reading
// END_OF_FILE as a control character, so emitting an error for it

// return the next character without consuming it
static unsigned char Phase1Peek(Phase1Context* ctx, bool* succeeded) {
    if(ctx->consumed >= ctx->sourceLength) {
        *succeeded = false;
        return '\0';
    }
    *succeeded = true;
    return ctx->source[ctx->consumed];
}

// return the character after next without consuming its
static unsigned char Phase1PeekNext(Phase1Context* ctx, bool* succeeded) {
    if(ctx->consumed - 1 >= ctx->sourceLength) {
        *succeeded = false;
        return '\0';
    }
    *succeeded = true;
    return ctx->source[ctx->consumed + 1];
}

// process a newline character ('\n' or '\r') to check how to advance the
// line counter.  If '\n' encountered, a following '\r' will not change the
// counter, if '\r', the '\n' will not update.  This allows multiple possible
// line endings: "\n", "\r", "\n\r", "\r\n" that are all considered one line end
static void Phase1NewLine(Phase1Context* ctx, unsigned char c) {
    if(ctx->ignoreNewLine != '\0') {
        if(c == ctx->ignoreNewLine) {
            ctx->location.line++;
            ctx->location.column = 1;
        }
        ctx->ignoreNewLine = '\0';
    }
    if(c == '\n') {
        ctx->ignoreNewLine = '\r';
        ctx->location.line++;
        ctx->location.column = 0;
    }
    if(c == '\r') {
        ctx->ignoreNewLine = '\n';
        ctx->location.line++;
        ctx->location.column = 0;
    }
}

// get the next character from the previous phase
// increases the SourcLocation's length with the new character
static unsigned char Phase1Advance(Phase1Context* ctx, bool* succeeded) {
    if(ctx->consumed >= ctx->sourceLength) {
        *succeeded = false;
        return '\0';
    }
    ctx->consumed++;
    ctx->location.length++;
    ctx->location.column++;
    unsigned char c = ctx->source[ctx->consumed - 1];
    Phase1NewLine(ctx, c);
    *succeeded = true;
    return c;
}

// get the next character from the previous phase
// sets the SourceLocation to begin from the new character's start
static unsigned char Phase1AdvanceOverwrite(Phase1Context* ctx, bool* succeeded) {
    if(ctx->consumed >= ctx->sourceLength) {
        *succeeded = false;
        return '\0';
    }
    ctx->consumed++;
    ctx->location.length = 1;
    ctx->location.column++;
    ctx->location.sourceText = &ctx->source[ctx->consumed - 1];
    unsigned char c = ctx->source[ctx->consumed - 1];
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
    ['-'] = '~',
};

// implement phase 1
// technically, this should convert the file to utf8, and probably normalise it,
// but I am not implementing that
static unsigned char Phase1Get(Phase1Context* ctx) {
    bool succeeded;
    unsigned char c = Phase1AdvanceOverwrite(ctx, &succeeded);

    if(!succeeded) {
        return END_OF_FILE;
    }

    if(ctx->consumed == 1) {
        // start of file - ignore BOM

        // do not need to check succeded due to length check in condition
        if(ctx->sourceLength >= 3 && c == 0xEF
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
        fprintf(stderr, "Error: found control character in source file - %lld:%lld\n", ctx->location.line, ctx->location.column);
        return '\0';
    }

    if(ctx->settings->trigraphs && c == '?') {
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

static void Phase1Initialise(Phase1Context* ctx, TranslationContext* settings) {
    ctx->source = (unsigned char*)readFileLen((char*)settings->fileName, &ctx->sourceLength);
    ctx->settings = settings;
    ctx->consumed = 0;
    ctx->ignoreNewLine = '\0';
    ctx->location = (SourceLocation) {
        .fileName = settings->fileName,
        .column = 0,
        .length = 0,
        .line = 1,
        .sourceText = ctx->source,
    };
}

// helper to run only phase 1
void runPhase1(TranslationContext* settings) {
    char c;
    Phase1Context ctx;
    Phase1Initialise(&ctx, settings);

    while((c = Phase1Get(&ctx)) != EOF) {
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
static unsigned char Phase2Advance(Phase2Context* ctx) {
    unsigned char ret = ctx->peek;
    ctx->currentLoc.length += ctx->peekLoc.length;
    ctx->peek = Phase1Get(&ctx->phase1);
    ctx->peekLoc = ctx->phase1.location;
    return ret;
}

// get the next character from the previous phase
// sets the SourceLocation to begin from the new character's start
static unsigned char Phase2AdvanceOverwrite(Phase2Context* ctx) {
    unsigned char ret = ctx->peek;
    ctx->currentLoc = ctx->peekLoc;
    ctx->peek = Phase1Get(&ctx->phase1);
    ctx->peekLoc = ctx->phase1.location;
    return ret;
}

// return the next character without consuming it
static unsigned char Phase2Peek(Phase2Context* ctx) {
    return ctx->peek;
}

// implements backslash-newline skipping
// if the next character after a backslash/newline is another backslash/newline
// then that should be skiped as well iteratively until the next real character
// emits error for backslash-END_OF_FILE and /[^\n]/-END_OF_FILE
static unsigned char Phase2Get(Phase2Context* ctx) {
    unsigned char c = Phase2AdvanceOverwrite(ctx);
    do {
        if(c == '\\') {
            unsigned char c1 = Phase2Peek(ctx);
            if(c1 == END_OF_FILE) {
                fprintf(stderr, "Error: unexpected '\\' at end of file\n");
                return END_OF_FILE;
            } else if(c1 != '\n') {
                ctx->previous = c;
                return c;
            } else {
                Phase2Advance(ctx);
                // exit if statement
            }
        } else if(c == END_OF_FILE && ctx->previous != '\n' && ctx->previous != END_OF_FILE) {
            // error iso c
            ctx->previous = END_OF_FILE;
            fprintf(stderr, "Error: ISO C11 requires newline at end of file\n");
            return END_OF_FILE;
        } else {
            ctx->previous = c;
            return c;
        }
        c = Phase2Advance(ctx);
    } while(c != END_OF_FILE);

    ctx->previous = c;
    return c;
}

// setup phase2's buffers
static void Phase2Initialise(Phase2Context* ctx, TranslationContext* settings) {
    ctx->previous = END_OF_FILE;
    Phase1Initialise(&ctx->phase1, settings);
    Phase2AdvanceOverwrite(ctx);
}

// helper to run upto and including phase 2
void runPhase2(TranslationContext* settings) {
    Phase2Context ctx;
    Phase2Initialise(&ctx, settings);
    unsigned char c;
    while((c = Phase2Get(&ctx)) != END_OF_FILE) {
        putchar(c);
    }
}

// ------- //
// Phase 3 //
// ------- //
// characters -> preprocessing tokens (unimplemented)
// comments -> whitespace
// tracking begining of line + prior whitespace in tokens

static unsigned char Phase3GetFromPhase2(void* voidCtx, SourceLocation* loc) {
    Phase3Context* ctx = voidCtx;
    unsigned char c = Phase2Get(&ctx->phase2);
    *loc = ctx->phase2.currentLoc;
    return c;
}

// get the next character from the previous phase
// increases the SourcLocation's length with the new character
static unsigned char Phase3Advance(Phase3Context* ctx) {
    unsigned char ret = ctx->peek;
    ctx->currentLocation->length += ctx->peekLoc.length;

    ctx->peek = ctx->peekNext;
    ctx->peekLoc = ctx->peekNextLoc;
    ctx->peekNext = ctx->getter(ctx->getterCtx, &ctx->peekNextLoc);

    return ret;
}

// get the next character from the previous phase
// sets the SourceLocation to begin from the new character's start
static unsigned char Phase3AdvanceOverwrite(Phase3Context* ctx) {
    unsigned char ret = ctx->peek;
    *ctx->currentLocation = ctx->peekLoc;

    ctx->peek = ctx->peekNext;
    ctx->peekLoc = ctx->peekNextLoc;
    ctx->peekNext = ctx->getter(ctx->getterCtx, &ctx->peekNextLoc);
    return ret;
}

// return the next character without consuming it
static unsigned char Phase3Peek(Phase3Context* ctx) {
    return ctx->peek;
}

// return the character after next without consuming it
static unsigned char Phase3PeekNext(Phase3Context* ctx) {
    return ctx->peekNext;
}

// has phase 3 reached the end of the file?
static bool Phase3AtEnd(Phase3Context* ctx) {
    return ctx->peek == END_OF_FILE;
}

// skip a new line ("\n", "\r", "\n\r", "\r\n") and set that the token is
// at the begining of a line
static void Phase3NewLine(LexerToken* tok, Phase3Context* ctx, unsigned char c) {
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
static void skipWhitespace(LexerToken* tok, Phase3Context* ctx) {
    tok->whitespaceBefore = false;
    tok->isStartOfLine = false;
    tok->renderStartOfLine = false;
    tok->indent = 0;

    if(ctx->AtStart) {
        tok->isStartOfLine = true;
        tok->renderStartOfLine = true;
        ctx->AtStart = false;
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
                if(c == '\t') tok->indent += ctx->settings->tabSize;
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
                        fprintf(stderr, "Error: Unterminated multi-line comment at %lld:%lld\n", ctx->currentLocation->line, ctx->currentLocation->column);
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

static bool Phase3Match(Phase3Context* ctx, unsigned char c) {
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
static void ParseUniversalCharacterName(Phase3Context* ctx, LexerToken* tok) {
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
        LexerStringAddChar(&tok->data.string, ctx->settings, num);
    } else if(num < 0x07FF) {
        unsigned char o1 = 0xC0 | (num >> 6);
        unsigned char o2 = 0x80 | (num & 0x3F);
        LexerStringAddChar(&tok->data.string, ctx->settings, o1);
        LexerStringAddChar(&tok->data.string, ctx->settings, o2);
    } else if(num < 0xFFFF) {
        unsigned char o1 = 0xE0 | (num >> 12);
        unsigned char o2 = 0x80 | ((num >> 6) & 0x3F);
        unsigned char o3 = 0x80 | (num & 0x3F);
        LexerStringAddChar(&tok->data.string, ctx->settings, o1);
        LexerStringAddChar(&tok->data.string, ctx->settings, o2);
        LexerStringAddChar(&tok->data.string, ctx->settings, o3);
    } else if(num < 0x10FFFF) {
        unsigned char o1 = 0xF0 | (num >> 18);
        unsigned char o2 = 0x80 | ((num >> 12) & 0x3F);
        unsigned char o3 = 0x80 | ((num >> 6) & 0x3F);
        unsigned char o4 = 0x80 | (num & 0x3F);
        LexerStringAddChar(&tok->data.string, ctx->settings, o1);
        LexerStringAddChar(&tok->data.string, ctx->settings, o2);
        LexerStringAddChar(&tok->data.string, ctx->settings, o3);
        LexerStringAddChar(&tok->data.string, ctx->settings, o4);
    } else {
        fprintf(stderr, "Error: UCS code point out of range: Maximum = 0x10FFFF\n");
        tok->type = TOKEN_ERROR_L;
        return;
    }
}

static bool isStringLike(Phase3Context* ctx, unsigned char c, unsigned char start) {
    unsigned char next = Phase3Peek(ctx);
    unsigned char nextNext = Phase3PeekNext(ctx);
    return c == start ||
        ((c == 'u' || c == 'U' || c == 'L') && next == start) ||
        (c == 'u' && next == '8' && nextNext == start);
}

// Generic string literal ish token parser
// used for character and string literals
// does not deal with escape sequences properly, that is for phase 5
static void ParseString(Phase3Context* ctx, LexerToken* tok, unsigned char c, unsigned char start) {
    unsigned char next = Phase3Peek(ctx);

    tok->type = start == '"' ? TOKEN_STRING_L : TOKEN_CHARACTER_L;
    LexerStringInit(&tok->data.string, ctx->settings, 10);
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
        LexerStringAddChar(&tok->data.string, ctx->settings, c);

        // skip escape sequences so that \" does not end a string
        if(c == '\\') {
            LexerStringAddChar(&tok->data.string, ctx->settings, Phase3Advance(ctx));
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
static void ParseHeaderName(Phase3Context* ctx, LexerToken* tok, unsigned char end) {
    tok->type = end == '>' ? TOKEN_SYS_HEADER_NAME : TOKEN_HEADER_NAME;

    LexerStringInit(&tok->data.string, ctx->settings, 20);

    unsigned char c = Phase3Peek(ctx);
    while(!Phase3AtEnd(ctx) && c != end && c != '\n') {
        Phase3Advance(ctx);
        if(c == '\'' || c == '\\' || (end == '>' && c == '"')) {
            fprintf(stderr, "Error: encountered `%c` while parsing header name "
                " - this is undefined behaviour\n", c);
            tok->type = TOKEN_ERROR_L;
            return;
        }

        LexerStringAddChar(&tok->data.string, ctx->settings, c);
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
static void Phase3Get(LexerToken* tok, Phase3Context* ctx) {
    SourceLocation* loc = memoryArrayPush(&ctx->settings->locations);
    *loc = *ctx->currentLocation;
    loc->length = 0;
    ctx->currentLocation = loc;

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
            if(ctx->mode == LEX_MODE_MAYBE_HEADER) {
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
            if(isDigit(ctx->peek)) break;
            if(ctx->peek == '.' && ctx->peekNext == '.') {
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
                ctx->peek == '%' && ctx->peekNext == ':' ?
                TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON :
                TOKEN_PUNC_PERCENT_COLON
            ) : TOKEN_PUNC_PERCENT); return;
    }

    unsigned char next = Phase3Peek(ctx);

    if(ctx->mode == LEX_MODE_MAYBE_HEADER && c == '"') {
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
        LexerStringInit(&tok->data.string, ctx->settings, 10);

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
                LexerStringAddChar(&tok->data.string, ctx->settings, c);
            }

            // advance
            c = Phase3Peek(ctx);
            consumedCharacter = false;
        }

        HashNode* node = tableGet(ctx->hashNodes, tok->data.string.buffer, tok->data.string.count);
        if(node == NULL) {
            node = ArenaAlloc(sizeof(*node));
            node->name = *tok;
            node->type = NODE_VOID;
            node->hash = stringHash(tok->data.string.buffer, tok->data.string.count);
            node->macroExpansionEnabled = true;
            tableSet(ctx->hashNodes, tok->data.string.buffer, tok->data.string.count, node);
        }
        tok->data.node = node;
        tok->data.attemptExpansion = true;

        return;
    }

    // pp-number
    if(isDigit(c) || c == '.') {
        tok->type = TOKEN_PP_NUMBER;
        LexerStringInit(&tok->data.string, ctx->settings, 10);
        LexerStringAddChar(&tok->data.string, ctx->settings, c);

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

            LexerStringAddChar(&tok->data.string, ctx->settings, c);
            c = Phase3Peek(ctx);
        }
        return;
    }

    // default
    Phase3Make(tok, TOKEN_UNKNOWN_L);
    tok->data.character = c;
}

static void PredefinedMacros(Phase3Context* ctx) {
    ctx->hashNodes = ArenaAlloc(sizeof(Table));
    TABLE_INIT(*ctx->hashNodes, HashNode*);

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
    TABLE_SET(*ctx->hashNodes, "__TIME__", 8, time);

    char* stringDate = ArenaAlloc(sizeof(char) * 128);
    strftime(stringDate, 128, "%b %d %Y", &timeStruct);
    HashNode* date = ArenaAlloc(sizeof(HashNode));
    date->type = NODE_MACRO_STRING;
    date->as.string = stringDate;
    date->macroExpansionEnabled = true;
    date->hash = stringHash("__DATE__", 8);
    date->name.data.string.buffer = "__DATE__";
    date->name.data.string.count = 8;
    TABLE_SET(*ctx->hashNodes, "__DATE__", 8, date);

    HashNode* file = ArenaAlloc(sizeof(HashNode));
    file->type = NODE_MACRO_FILE;
    file->macroExpansionEnabled = true;
    file->hash = stringHash("__FILE__", 8);
    file->name.data.string.buffer = "__FILE__";
    file->name.data.string.count = 8;
    TABLE_SET(*ctx->hashNodes, "__FILE__", 8, file);

    HashNode* line = ArenaAlloc(sizeof(HashNode));
    line->type = NODE_MACRO_LINE;
    line->macroExpansionEnabled = true;
    line->hash = stringHash("__LINE__", 8);
    line->name.data.string.buffer = "__LINE__";
    line->name.data.string.count = 8;
    TABLE_SET(*ctx->hashNodes, "__LINE__", 8, line);

#define INT_MACRO(stringname, value) do {\
        size_t len = strlen(stringname); \
        HashNode* m = ArenaAlloc(sizeof(HashNode)); \
        m->type = NODE_MACRO_INTEGER; \
        m->as.integer = (value); \
        m->macroExpansionEnabled = true; \
        m->hash = stringHash(stringname, len); \
        m->name.data.string.buffer = stringname; \
        m->name.data.string.count = len; \
        TABLE_SET(*ctx->hashNodes, stringname, len, m); \
    } while(0)

    INT_MACRO("__STDC__", 1);
    INT_MACRO("__STDC_HOSTED__", 1);
    INT_MACRO("__STDC_VERSION__", 201112L);
    INT_MACRO("__STDC_UTF_16__", 1);
    INT_MACRO("__STDC_UTF_32__", 1);
    INT_MACRO("__STDC_NO_ATOMICS__", 1);
    INT_MACRO("__STDC_NO_COMPLEX__", 1);
    INT_MACRO("__STDC_NO_THREADS__", 1);
    INT_MACRO("__STDC_NO_VLA__", 1);
    INT_MACRO("__STDC_LIB_EXT1__", 201112L);
    INT_MACRO("__x86_64__", 1);
    INT_MACRO("__x86_64", 1);
    INT_MACRO("WIN32", 1);
    INT_MACRO("_WIN32", 1);
    INT_MACRO("__WIN32__", 1);
    INT_MACRO("__WIN32__", 1);
    INT_MACRO("WIN64", 1);
    INT_MACRO("_WIN64", 1);
    INT_MACRO("__WIN64__", 1);
    INT_MACRO("__WIN64__", 1);

#undef INT_MACRO
}

// initialise the context for running phase 3
static void Phase3Initialise(Phase3Context* ctx, TranslationContext* settings, Phase3Context* parent, bool needPhase2) {
    if(needPhase2) Phase2Initialise(&ctx->phase2, settings);

    ctx->settings = settings;
    ctx->mode = LEX_MODE_NO_HEADER,
    ctx->peek = '\0',
    ctx->peekNext = '\0',
    ctx->currentLocation = memoryArrayPush(&settings->locations);
    ctx->peekLoc = *ctx->currentLocation,
    ctx->peekNextLoc = *ctx->currentLocation,
    ctx->AtStart = true;
    if(!ctx->getter) {
        ctx->getter = Phase3GetFromPhase2;
        ctx->getterCtx = ctx;
    }

    if(ctx->hashNodes == NULL) {
        if(parent) {
            ctx->hashNodes = parent->hashNodes;
        } else {
            PredefinedMacros(ctx);
        }
    }

    Phase3AdvanceOverwrite(ctx);
    Phase3AdvanceOverwrite(ctx);
}

// helper to run upto and including phase 3
void runPhase3(TranslationContext* settings) {
    Phase3Context ctx;
    Phase3Initialise(&ctx, settings, NULL, true);

    LexerToken tok;
    TokenPrintCtxFile printCtx;
    TokenPrintCtxInitFile(&printCtx, stdout, settings);

    while(Phase3Get(&tok, &ctx), tok.type != TOKEN_EOF_L) {
        TokenPrintFile(&printCtx, &tok);
    }
    fprintf(stdout, "\n");
}

// ------- //
// Phase 4 //
// ------- //
// macro expansion
// directive execution
// _Pragma expansion
// include resolution

static void Phase4Initialise(Phase4Context* ctx, TranslationContext* settings, Phase4Context* parent) {
    Phase3Initialise(&ctx->phase3, settings, NULL, true);

    ctx->settings = settings;

    if(parent != NULL) {
        ctx->phase3.hashNodes = parent->phase3.hashNodes;
    } else {
        ctx->phase3.hashNodes = NULL;
    }

    ctx->mode = LEX_MODE_NO_HEADER;
    ctx->searchState = (IncludeSearchState){0};
    ctx->parent = parent;
    ctx->macroCtx = (MacroContext){0};
    if(parent != NULL) {
        ctx->depth = parent->depth + 1;
    } else {
        ctx->depth = 0;
        ctx->previous.loc = &(SourceLocation) {
            .fileName = settings->fileName,
            .column = 0,
            .length = 0,
            .line = 1,
            .sourceText = (const unsigned char*)"",
        };
    }

    Phase3Initialise(&ctx->phase3, settings, NULL, true);
    Phase3Get(&ctx->peek, &ctx->phase3);
}

static bool Phase4AtEnd(Phase4Context* ctx) {
    return ctx->peek.type == TOKEN_EOF_L;
}

static LexerToken* Phase4Advance(LexerToken* tok, void* ctx) {
    Phase4Context* t = ctx;
    *tok = t->peek;
    Phase3Get(&t->peek, &t->phase3);
    return tok;
}

static LexerToken* Phase4Peek(LexerToken* tok, void* ctx) {
    Phase4Context* t = ctx;
    *tok = t->peek;
    return tok;
}

static void Phase4SkipLine(LexerToken* tok, Phase4Context* ctx) {
    while(!Phase4AtEnd(ctx) && !ctx->peek.isStartOfLine) {
        Phase4Advance(tok, ctx);
    }
}

static void Phase4Get(LexerToken* tok, Phase4Context* ctx);
static bool includeFile(LexerToken* tok, Phase4Context* ctx, bool isUser, bool isNext) {
    Phase4Advance(tok, ctx);

    IncludeSearchState* state;
    if(isNext) {
        if(ctx->parent) {
            state = &ctx->parent->searchState;
        } else {
            state = &(IncludeSearchState){0};
            fprintf(stderr, "Warning: #include_next at top level\n");
        }
    } else {
        state = &ctx->searchState;
        *state = (IncludeSearchState){0};
    }

    const char* fileName;
    if(isUser) {
        fileName = IncludeSearchPathFindUser(state, &ctx->settings->search, tok->data.string.buffer);
    } else {
        fileName = IncludeSearchPathFindSys(state, &ctx->settings->search, tok->data.string.buffer);
    }

    if(fileName == NULL) {
        fprintf(stderr, "Error: Cannot resolve include\n");
        Phase4SkipLine(tok, ctx);
        return false;
    }

    // See n1570.5.2.4.1
    if(ctx->depth > 15) {
        fprintf(stderr, "Error: include depth limit reached\n");
        Phase4SkipLine(tok, ctx);
        return false;
    }

    Phase4Context* ctx2 = ArenaAlloc(sizeof(*ctx2));
    memset(ctx2, 0, sizeof(*ctx2));

    ctx->mode = LEX_MODE_INCLUDE;
    ctx->includeContext = ctx2;

    ctx2->previous = ctx->previous;

    const unsigned char* oldFileName = ctx->settings->fileName;
    ctx->settings->fileName = (const unsigned char*)fileName;
    Phase4Initialise(ctx2, ctx->settings, ctx);
    ctx->settings->fileName = oldFileName;

    Phase4Get(tok, ctx2);
    return true;
}

static bool parseInclude(LexerToken* tok, Phase4Context* ctx, bool isNext) {
    ctx->phase3.mode = LEX_MODE_MAYBE_HEADER;
    Phase4Advance(tok, ctx);
    ctx->phase3.mode = LEX_MODE_NO_HEADER;

    LexerToken* peek = &ctx->peek;
    bool retVal = false;
    if(peek->type == TOKEN_HEADER_NAME) {
        retVal = includeFile(tok, ctx, true, isNext);
    } else if(peek->type == TOKEN_SYS_HEADER_NAME) {
        retVal = includeFile(tok, ctx, false, isNext);
    } else {
        fprintf(stderr, "Error: macro #include not implemented\n");
        Phase4SkipLine(tok, ctx);
    }

    if(!ctx->peek.isStartOfLine) {
        fprintf(stderr, "Error: Unexpected token after include location\n");
        Phase4SkipLine(tok, ctx);
    }

    return retVal;
}

static void parseDefine(Phase4Context* ctx) {
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

    LexerToken* tok = &ctx->peek;

    if(tok->type == TOKEN_PUNC_LEFT_PAREN && !tok->whitespaceBefore) {
        node->type = NODE_MACRO_FUNCTION;
        ARRAY_ALLOC(LexerToken, node->as.function, argument);
        ARRAY_ALLOC(LexerToken, node->as.function, replacement);
        node->as.function.variadacArgument = -1;

        LexerToken currentToken;
        Phase4Advance(&currentToken, ctx); // consume '('

        while(!tok->isStartOfLine) {
            Phase4Advance(&currentToken, ctx);
            if(currentToken.type == TOKEN_PUNC_ELIPSIS) {
                node->as.function.variadacArgument = node->as.function.argumentCount;
                Phase4Advance(&currentToken, ctx);
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
        if(addr->type == TOKEN_EOF_L) {
            break;
        }

        if(i == 0) {
            addr->indent = 0;
        } else {
            addr->indent = addr->indent ? 1 : 0;
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
        (node->type == NODE_MACRO_OBJECT || (node->type == NODE_MACRO_FUNCTION && node->as.function.variadacArgument == -1)) &&
        addr->data.node->name.data.string.count == 11 &&
        addr->data.node->hash == stringHash("__VA_ARGS__", 11)) {
            fprintf(stderr, "Error: __VA_ARGS__ is invalid unless in a variadac function macro\n");
        }

        i++;
    }
}

static void parseUndef(Phase4Context* ctx) {
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

typedef enum EnterContextResult {
    CONTEXT_MACRO_TOKEN,
    CONTEXT_MACRO_NULL,
    CONTEXT_NOT_MACRO,
    CONTEXT_DISABLED_MACRO,
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
    Phase4Context* ctx, \
    MacroContext* macro, \
    Phase4GetterFn advance, \
    Phase4GetterFn peek, \
    void* getCtx \
)

EXPAND_LINE_FN;

static void ExpandTokenList(
    Phase4Context* ctx,
    TokenList* result,
    Phase4GetterFn advance,
    Phase4GetterFn peek,
    Phase4BoolFn earlyExit,
    void* getCtx,
    bool disableNonExpandedIdentifiers,
    LexerToken* firstPaddingToken) {
    ARRAY_ALLOC(LexerToken, *result, item);
    LexerToken* t = ARRAY_PUSH_PTR(*result, item);
    int iteration = 0;
    LexerToken previous = {.type = TOKEN_EOF_L};
    while(true) {
        advance(t, getCtx);

        if(iteration == 0) {
            t->indent = firstPaddingToken->indent;
            t->renderStartOfLine = firstPaddingToken->renderStartOfLine;
            t->whitespaceBefore = firstPaddingToken->whitespaceBefore;
        }
        if(previous.type != TOKEN_EOF_L) {
            t->renderStartOfLine |= previous.renderStartOfLine;
            t->whitespaceBefore |= previous.whitespaceBefore;
            t->indent += previous.indent;
        }

        previous = *t;

        // expand the new macro
        MacroContext macro = {0};
        EnterContextResult res = ExpandSingleMacro(t, ctx, &macro, advance, peek, getCtx);
        if(res == CONTEXT_DISABLED_MACRO && disableNonExpandedIdentifiers) {
            t->data.attemptExpansion = false;
        }

        if(res == CONTEXT_MACRO_NULL) {
            result->itemCount--;
        } else {
            previous.type = TOKEN_EOF_L;
        }

        // append the new tokens to the current buffer
        for(unsigned int i = 0; i < macro.tokenCount; i++) {
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
        iteration++;
    }
}

typedef struct Phase4JoinCtx {
    LexerToken left;
    LexerToken right;
    size_t consumed;
} Phase4JoinCtx;

static unsigned char Phase4JoinGetter(void* voidCtx, SourceLocation* loc) {
    Phase4JoinCtx* ctx = voidCtx;
    if(!ctx->left.loc || !ctx->right.loc) {
        fprintf(stderr, "Joining undefined tokens\n");
        exit(1);
    }

    loc->length = 1;

    if(ctx->consumed < ctx->left.loc->length) {
        unsigned char ret = ctx->left.loc->sourceText[ctx->consumed];

        loc->column = ctx->left.loc->column + ctx->consumed;
        loc->line = ctx->left.loc->line;
        loc->fileName = ctx->left.loc->fileName;
        loc->sourceText = ctx->left.loc->sourceText + ctx->consumed;

        ctx->consumed++;
        return ret;
    } else if(ctx->consumed - ctx->left.loc->length < ctx->right.loc->length) {
        size_t rightConsumed = ctx->consumed - ctx->left.loc->length;
        unsigned char ret = ctx->right.loc->sourceText[rightConsumed];

        loc->column = ctx->right.loc->column + rightConsumed;
        loc->line = ctx->right.loc->line;
        loc->fileName = ctx->right.loc->fileName;
        loc->sourceText = ctx->right.loc->sourceText + rightConsumed;

        ctx->consumed++;
        return ret;
    }

    return END_OF_FILE;
}

// join two tokens into one token or return false
static bool JoinTokens(Phase4Context* ctx, LexerToken* result, LexerToken left, LexerToken right) {
    // placeholder + placeholder => placeholder
    // placeholder + * => placeholder
    // * + placeholder => placeholder
    // * + * => run phase 3, error if second token produced, etc

    if(left.type == TOKEN_PLACEHOLDER_L) {
        *result = right;
        return true;
    }
    if(right.type == TOKEN_PLACEHOLDER_L) {
        *result = left;
        return true;
    }

    Phase4JoinCtx joinCtx;
    joinCtx.left = left;
    joinCtx.right = right;
    joinCtx.consumed = 0;

    // create new translation context
    // initialise for phase 3, with no phase 1/2 initialisation
    // getter = Phase4GetterForPhase3

    Phase3Context joinTranslate = {0};
    joinTranslate.getter = Phase4JoinGetter;
    joinTranslate.getterCtx = &joinCtx;
    Phase3Initialise(&joinTranslate, ctx->settings, &ctx->phase3, false);

    LexerToken newTok;
    Phase3Get(&newTok, &joinTranslate);

    LexerToken next;
    Phase3Get(&next, &joinTranslate);
    if(next.type != TOKEN_EOF_L) {
        return false;
    }

    *result = newTok;
    return true;
}

static bool TokenConcatList(Phase4Context* ctx, TokenList* in, TokenList* out) {
    // token concatanation operator application

    // algorithm =
    // maintain two lists - left + right
    // right = all tokens, initially
    // if pop front right == '##'
    //    check left length > 0 && right length > 0
    //    join pop front right to pop end left
    //    push result to left
    // else
    //    push back left
    // repeat until right empty
    // result = left

    TokenList left;
    ARRAY_ALLOC(LexerToken, left, item);
    TokenList right = *in;

    while(right.itemCount > 0) {
        LexerToken current = ARRAY_POP_FRONT(right, item);
        if(current.type == TOKEN_PUNC_HASH_HASH || current.type == TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON) {
            if(left.itemCount <= 0) {
                fprintf(stderr, "Error: No token before ## operator\n");
                return false;
            }
            if(right.itemCount <= 0) {
                fprintf(stderr, "Error: No token after ## operator\n");
                return false;
            }
            LexerToken leftT = ARRAY_POP(left, item);
            LexerToken rightT = ARRAY_POP_FRONT(right, item);
            LexerToken* new = ARRAY_PUSH_PTR(left, item);

            if(!JoinTokens(ctx, new, leftT, rightT)) {
                fprintf(stderr, "Error unable to join tokens\n");
                return false;
            }

            new->indent = leftT.indent;
            new->renderStartOfLine = leftT.renderStartOfLine;
            new->whitespaceBefore = leftT.whitespaceBefore;
        } else {
            ARRAY_PUSH(left, item, current);
        }
    }

    // now remove all placemarker tokens
    ARRAY_ALLOC(LexerToken, *out, item);
    for(unsigned int i = 0; i < left.itemCount; i++) {
        if(left.items[i].type != TOKEN_PLACEHOLDER_L) {
            ARRAY_PUSH(*out, item, left.items[i]);
        }
    }

    return true;
}

// expand an object macro
static EnterContextResult ParseObjectMacro(
    MacroContext* macro,
    LexerToken* tok,
    Phase4Context* ctx,
    Phase4GetterFn advance,
    Phase4GetterFn peek,
    void* getCtx
) {
    if(tok->data.node->as.object.itemCount <= 0) {
        return CONTEXT_MACRO_NULL;
    }

    TokenList tokens = tok->data.node->as.object;
    bool hasTokenCat = false;
    for(unsigned int i = 0; i < tokens.itemCount; i++) {
        if(tokens.items[i].type == TOKEN_PUNC_HASH_HASH || tokens.items[i].type == TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON) {
            hasTokenCat = true;
        }
    }

    TokenList concatenated;
    if(hasTokenCat) {
        if(!TokenConcatList(ctx, &tokens, &concatenated)) {
            return CONTEXT_MACRO_NULL;
        }
    } else {
        concatenated = tokens;
    }

    // copy token list otherwise the tokens are globably removed
    // from the macro's hash node.
    JointTokenStream stream = {
        .list = &concatenated,
        .macroContext = tok->data.node,
        .second = getCtx,
        .secondAdvance = advance,
        .secondPeek = peek,
    };
    TokenList result;
    tok->data.node->macroExpansionEnabled = false;
    ExpandTokenList(ctx, &result, JointTokenAdvance, JointTokenPeek, JointTokenEarlyExit, &stream, true, tok);
    tok->data.node->macroExpansionEnabled = true;

    if(result.itemCount <= 0) {
        return CONTEXT_MACRO_NULL;
    }

    *tok = result.items[0];
    macro->tokens = result.items + 1;
    macro->tokenCount = result.itemCount - 1;

    return CONTEXT_MACRO_TOKEN;
}

// Container for the argument tokens passed into a function macro.
// The tokens are lazily stringified or macro expanded as required.  This avoids
// calculating stringification/expansion if not required.  Also it avoids
// emitting errors relating to macro expansion, if the argument never undergoes
// macro expansion.
typedef struct ArgumentItem {
    TokenList tokens;
    TokenList expanded;
    LexerToken string;
    bool hasExpanded;
    bool hasString;
} ArgumentItem;

typedef struct ArgumentItemList {
    ARRAY_DEFINE(ArgumentItem, item);
} ArgumentItemList;

// macro expand a function macro argument
// if the argument was already expanded, return the previous expansion
// to avoid expanding the argument multiple times
static TokenList* expandArgument(Phase4Context* ctx, ArgumentItem* arg, LexerToken* padding) {
    if(arg->hasExpanded) {
        return &arg->expanded;
    }

    arg->expanded = (TokenList){0};
    ExpandTokenList(ctx, &arg->expanded, TokenListAdvance, TokenListPeek, ReturnFalse, &arg->tokens, false, padding);
    arg->hasExpanded = true;

    return &arg->expanded;
}

// returns the stringified version of a token using the # operator
// caches the resultant token to avoid recalculation
static LexerToken* stringifyArgument(Phase4Context* ctx, ArgumentItem* arg) {
    if(arg->hasString) {
        return &arg->string;
    }

    if(arg->tokens.itemCount == 0) {
        arg->string = (LexerToken){
            .isStartOfLine = false,
            .indent = 0,
            .renderStartOfLine = false,
            .type = TOKEN_STRING_L,
            .whitespaceBefore = false,
            .data.string = {
                .type = STRING_NONE,
                .count = 0,
                .capacity = 0,
                .buffer = "",
            }
        };
        arg->hasString = true;
        return &arg->string;
    }

    LexerString str;
    LexerStringInit(&str, ctx->settings, 50);
    TokenPrintCtxString printCtx;
    TokenPrintCtxInitString(&printCtx, &str, ctx->settings);

    for(unsigned int i = 0; i < arg->tokens.itemCount; i++) {
        TokenPrintString(&printCtx, &arg->tokens.items[i]);
    }

    arg->string = (LexerToken) {
        .isStartOfLine = false,
        .indent = 0,
        .renderStartOfLine = false,
        .type = TOKEN_STRING_L,
        .whitespaceBefore = false,
        .data.string = str,
    };
    arg->hasString = true;

    return &arg->string;
}

static EnterContextResult ParseFunctionMacro(
    MacroContext* macro,
    LexerToken* tok,
    Phase4Context* ctx,
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

    FnMacro* fn = &tok->data.node->as.function;

    ArgumentItemList args;
    ARRAY_ALLOC(ArgumentItem, args, item);

    // gather arguments and macro expand them
    // this guarantees that there will be at least one argument parsed
    LexerToken* next;
    while(true) {
        ArgumentItem* arg = ARRAY_PUSH_PTR(args, item);
        ARRAY_ALLOC(LexerToken, arg->tokens, item);
        arg->hasExpanded = false;
        arg->hasString = false;

        int bracketDepth = 0;
        while(true) {
            next = ARRAY_PUSH_PTR(arg->tokens, item);
            advance(next, getCtx);

            if(next->type == TOKEN_PUNC_COMMA && bracketDepth == 0) {
                if(fn->variadacArgument < 0  || args.itemCount <= (unsigned int)fn->variadacArgument) {
                    arg->tokens.itemCount--;
                    break;
                }
            } else if(next->type == TOKEN_PUNC_LEFT_PAREN) {
                bracketDepth++;
            } else if(next->type == TOKEN_PUNC_RIGHT_PAREN) {
                if(bracketDepth == 0) {
                    arg->tokens.itemCount--;
                    break;
                } else {
                    bracketDepth--;
                }
            } else if(next->type == TOKEN_EOF_L) {
                break;
            }
        }

        if(arg->tokens.itemCount > 0) {
            arg->tokens.items[0].indent = 0;
        }

        if(next->type == TOKEN_PUNC_RIGHT_PAREN || next->type == TOKEN_EOF_L) {
            break;
        }
    }

    if(next->type != TOKEN_PUNC_RIGHT_PAREN) {
        fprintf(stderr, "Error: Unterminated function macro call [%lld:%lld]\n", tok->loc->line, tok->loc->column);
        return CONTEXT_MACRO_NULL;
    }

    size_t minArgs = fn->argumentCount +
        (fn->variadacArgument >= 0 && !ctx->settings->optionalVariadacArgs);

    if(minArgs == 0 && fn->variadacArgument == -1) {
        // empty parens e.g. macrocall() counts as one empty argument, or none
        // depending on what is required
        if(args.itemCount != 1 || args.items[0].tokens.itemCount != 0) {
            fprintf(stderr, "Error: Arguments provided to macro call %s\n", tok->data.node->name.data.string.buffer);
            return CONTEXT_MACRO_NULL;
        }
    } else {
        if(args.itemCount < minArgs) {
            fprintf(stderr, "Error: Not enough arguments provided to macro call %s - %lld of %lld\n", tok->data.node->name.data.string.buffer, args.itemCount, minArgs);
            return CONTEXT_MACRO_NULL;
        }
        if(args.itemCount > minArgs && fn->variadacArgument == -1) {
            fprintf(stderr, "Error: Too many arguments provided to macro call\n");
            return CONTEXT_MACRO_NULL;
        }
    }

    TokenList substituted;
    ARRAY_ALLOC(LexerToken, substituted, item);

    bool containsTokenConcatanation = false;

    // substitute arguments into replacement list
    for(unsigned int i = 0; i < fn->replacementCount; i++) {
        LexerToken* tok = &fn->replacements[i];

        bool isVaArgs = fn->variadacArgument != -1 &&
            tok->type == TOKEN_IDENTIFIER_L &&
            tok->data.node->name.data.string.count == 11 &&
            tok->data.node->hash == stringHash("__VA_ARGS__", 11);

        if(tok->type == TOKEN_MACRO_ARG || isVaArgs) {

            // should macro expansion be applied to the argument's tokens?
            // disabled for the ## operator
            bool isExpanded = true;

            // argument before ##
            if(i + 1 < fn->replacementCount) {
                LexerTokenType next = fn->replacements[i+1].type;
                if(next == TOKEN_PUNC_HASH_HASH || next == TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON) {
                    isExpanded = false;
                    containsTokenConcatanation = true;
                }
            }

            // argument after ##
            if(i > 0) {
                LexerTokenType prev = fn->replacements[i-1].type;
                if(prev == TOKEN_PUNC_HASH_HASH || prev == TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON) {
                    isExpanded = false;
                    containsTokenConcatanation = true;
                }
            }

            size_t argumentNumber = tok->data.integer;
            if(isVaArgs) {
                argumentNumber = fn->variadacArgument;
            }
            if(argumentNumber >= args.itemCount) {
                continue;
            }
            ArgumentItem* argument = &args.items[argumentNumber];

            if(isExpanded) {
                // no relation to token concatanation operator
                TokenList* arg = expandArgument(ctx, argument, tok);
                for(unsigned int j = 0; j < arg->itemCount; j++) {
                    ARRAY_PUSH(substituted, item, arg->items[j]);

                    if(j == 0) {
                        LexerToken* t = &substituted.items[substituted.itemCount-1];
                        t->indent = tok->indent;
                        t->whitespaceBefore = tok->whitespaceBefore;
                        t->renderStartOfLine = tok->renderStartOfLine;
                    }
                }
            } else {
                TokenList* arg = &argument->tokens;
                // empty argument -> standard says add placeholder token
                if(arg->itemCount == 0) {
                    LexerToken* placeholder = ARRAY_PUSH_PTR(substituted, item);
                    placeholder->type = TOKEN_PLACEHOLDER_L;

                } else {
                    for(unsigned int j = 0; j < arg->itemCount; j++) {
                        ARRAY_PUSH(substituted, item, arg->items[j]);
                        if(j == 0) {
                            LexerToken* t = &substituted.items[substituted.itemCount-1];
                            t->indent = tok->indent;
                            t->whitespaceBefore = tok->whitespaceBefore;
                            t->renderStartOfLine = tok->renderStartOfLine;
                        }
                    }
                }
            }
        } else if(tok->type == TOKEN_PUNC_HASH || tok->type == TOKEN_PUNC_PERCENT_COLON) {
            i++;
            LexerToken* argToken = &fn->replacements[i];
            if(argToken->type != TOKEN_MACRO_ARG) {
                fprintf(stderr, "Error: Stringification operator applied to non-argument token\n");
                return CONTEXT_MACRO_NULL;
            }
            LexerToken* str = stringifyArgument(ctx, &args.items[argToken->data.integer]);
            str->indent = tok->indent;
            str->renderStartOfLine = tok->renderStartOfLine;
            str->whitespaceBefore = tok->whitespaceBefore;
            ARRAY_PUSH(substituted, item, *str);
        } else {
            ARRAY_PUSH(substituted, item, *tok);
        }
    }

    if(substituted.itemCount <= 0) {
        return CONTEXT_MACRO_NULL;
    }

    TokenList concatenated;

    if(containsTokenConcatanation) {
        if(!TokenConcatList(ctx, &substituted, &concatenated)) {
            return CONTEXT_MACRO_NULL;
        }
    } else {
        concatenated = substituted;
    }

    // macro expand the substituted list

    JointTokenStream stream = {
        .list = &concatenated,
        .macroContext = tok->data.node,
        .second = getCtx,
        .secondAdvance = advance,
        .secondPeek = peek,
    };
    TokenList result;
    tok->data.node->macroExpansionEnabled = false;
    ExpandTokenList(ctx, &result, JointTokenAdvance, JointTokenPeek, JointTokenEarlyExit, &stream, true, tok);
    tok->data.node->macroExpansionEnabled = true;

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
    if(tok->type != TOKEN_IDENTIFIER_L || tok->data.node->type == NODE_VOID) {
        return CONTEXT_NOT_MACRO;
    }
    if(!tok->data.node->macroExpansionEnabled || !tok->data.attemptExpansion) {
        return CONTEXT_DISABLED_MACRO;
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
            tok->data.integer = ctx->previous.loc->line;
            macro->tokenCount = 0;
            return CONTEXT_MACRO_TOKEN;
        }
        case NODE_MACRO_FILE: {
            tok->type = TOKEN_STRING_L;
            LexerString str;
            str.buffer = (char*)ctx->previous.loc->fileName;
            str.count = strlen((char*)ctx->previous.loc->fileName);
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
static EnterContextResult __attribute__((warn_unused_result)) EnterMacroContext(LexerToken* tok, Phase4Context* ctx) {
    ctx->previous = *tok;
    return ExpandSingleMacro(tok, ctx, &ctx->macroCtx, Phase4Advance, Phase4Peek, ctx);
}

// gets next token from existing buffer,
// does not do any macro expansion
static void AdvanceMacroContext(LexerToken* tok, Phase4Context* ctx) {
    *tok = ctx->macroCtx.tokens[0];
    ctx->macroCtx.tokens++;
    ctx->macroCtx.tokenCount--;
}

// stops using a buffer to store the expanded macro, if at the end of the macro
static void TryExitMacroContext(Phase4Context* ctx) {
    if(ctx->macroCtx.tokens != NULL && ctx->macroCtx.tokenCount == 0) {
        ctx->macroCtx.tokens= NULL;
    }
}

static void Phase4Get(LexerToken* tok, Phase4Context* ctx) {
    if(ctx->mode == LEX_MODE_INCLUDE) {
        Phase4Get(tok, ctx->includeContext);
        if(tok->type == TOKEN_EOF_L) {
            ctx->mode = LEX_MODE_NO_HEADER;
        } else {
            ctx->previous = *tok;
            return;
        }
    }

    TryExitMacroContext(ctx);
    if(ctx->macroCtx.tokens != NULL) {
        AdvanceMacroContext(tok, ctx);
        return;
    }

    tok->type = TOKEN_ERROR_L;

    // loop over multiple directives (instead of reccursion)
    LexerToken previous = {.type = TOKEN_EOF_L};
    while(tok->type != TOKEN_EOF_L) {
        Phase4Advance(tok, ctx);

        if(previous.type != TOKEN_EOF_L) {
            tok->renderStartOfLine |= previous.renderStartOfLine;
            tok->whitespaceBefore |= previous.whitespaceBefore;
            tok->indent += previous.indent;
        }
        previous = *tok;

        if((tok->type == TOKEN_PUNC_HASH || tok->type == TOKEN_PUNC_PERCENT_COLON) && tok->isStartOfLine) {
            LexerToken* peek = &ctx->peek;

            if(peek->isStartOfLine) {
                // NULL directive
                previous.type = TOKEN_EOF_L;
                continue;
            }

            if(peek->type != TOKEN_IDENTIFIER_L) {
                fprintf(stderr, "Error: Unexpected token at start of directive\n");
                Phase4SkipLine(tok, ctx);
                previous.type = TOKEN_EOF_L;
                continue;
            }

            uint32_t hash = peek->data.node->hash;
            size_t len = peek->data.node->name.data.string.count;

            if(len == 7 && hash == stringHash("include", 7)) {
                bool success = parseInclude(tok, ctx, false);

                // not all header files have source code, they could be all
                // preprocessor directives, so it could succeed, but produce
                // an eof token when it is not end of file, causing processing
                // to halt too early.
                if(success && tok->type != TOKEN_EOF_L) {
                    ctx->previous = *tok;
                    return;
                }
                tok->type = TOKEN_ERROR_L;
                previous.type = TOKEN_EOF_L;
                continue;
            } else if(len == 12 && hash == stringHash("include_next", 12)) {
                // See https://gcc.gnu.org/onlinedocs/cpp/Wrapper-Headers.html
                bool success = parseInclude(tok, ctx, true);
                if(success) {
                    ctx->previous = *tok;
                    return;
                }
                tok->type = TOKEN_ERROR_L;
                previous.type = TOKEN_EOF_L;
                continue;
            } else if(len == 6 && hash == stringHash("define", 6)) {
                parseDefine(ctx);
                previous.type = TOKEN_EOF_L;
                continue;
            } else if(len == 5 && hash == stringHash("undef", 5)) {
                parseUndef(ctx);
                previous.type = TOKEN_EOF_L;
                continue;
            }

            //fprintf(stderr, "Error: Unknown preprocessing directive\n");
            //Phase4SkipLine(tok, ctx);
            break;
        }

        if(EnterMacroContext(tok, ctx) == CONTEXT_MACRO_NULL) {
            // dont set loop previous token
            continue;
        }

        if(tok->type == TOKEN_IDENTIFIER_L &&
        tok->data.node->name.data.string.count == 11 &&
        tok->data.node->hash == stringHash("__VA_ARGS__", 11)) {
            fprintf(stderr, "Warning: Unexpected identifier __VA_ARGS__ outisde of variadac function macro\n");
        }
        break;
    }

    ctx->previous = *tok;
}

// helper to run upto and including phase 4
void runPhase4(TranslationContext* settings) {
    Phase4Context ctx;
    Phase4Initialise(&ctx, settings, NULL);

    LexerToken tok;
    TokenPrintCtxFile printCtx;
    TokenPrintCtxInitFile(&printCtx, stdout, settings);

    while(Phase4Get(&tok, &ctx), tok.type != TOKEN_EOF_L) {
        TokenPrintFile(&printCtx, &tok);
    }
    fprintf(stdout, "\n");
}
