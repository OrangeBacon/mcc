#include "lex.h"
#include "lexString.h"
#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>

// This file implements a token printer.  The printer needs to work with
// both FILE* pointers and LexerString buffers, so macros are used to define
// the string handling, so this header can be included twice with different
// macro definitions, to avoid duplicating code.
// It also includes escapes special characters if outputing to a LexerString
// e.g. if printing " or \ in a string literal.

// Printing: use the PRINT() macro, which prints based on the type of the value
// provided to it. The first argument is the current token print ctx.
// Character literals cannot be printed as they have int type by default,
// use a string literal or a variable instead.

#ifndef LEX_TOKEN_H
#   define PRINT_TYPE FILE*
#   define PRINT_TYPE_NAME File
#   define PRINT_NUMERIC_ID 0
#   define PRINT(c, value) fprintf((c)->file, _Generic((value), \
        char*: "%s", \
        char: "%c", \
        LexerTokenType: "%d", \
        size_t: "%llu", \
        intmax_t: "%lld", \
        double: "%f" \
        ), (value))
#   define PRINT_ESCAPE(c, value, len) \
        fprintfEscape((c)->file, (value), (len))
#else
#   define PRINT_TYPE LexerString*
#   define PRINT_TYPE_NAME String
#   define PRINT_NUMERIC_ID 1
#   define PRINT(c, value) _Generic((value), \
        char*: LexerStringAddString, \
        char: LexerStringAddChar, \
        LexerTokenType: LexerStringAddInt, \
        size_t: LexerStringAddSizeT, \
        intmax_t: LexerStringAddIntMaxT, \
        double: LexerStringAddDouble \
        )((c)->file, (c)->ctx, (value))
#   define PRINT_ESCAPE(c, value, len) \
        LexerStringAddEscapedString((c)->file, (c)->ctx, (value), (len))
#endif

#define JOIN(a,b) JOIN_(a,b)
#define JOIN_(a,b) a##b

#define TokenPrintCtx JOIN(TokenPrintCtx, PRINT_TYPE_NAME)
typedef struct TokenPrintCtx {
    bool debugPrint;
    bool tokenPrinterAtStart;
    LexerToken previousPrinted;
    TranslationContext* ctx;
    PRINT_TYPE file;
} TokenPrintCtx;

#define StringTypePrint JOIN(StringTypePrint, PRINT_TYPE_NAME)
static void StringTypePrint(LexerStringType t, TokenPrintCtx* ctx) {
    (void)ctx;
    switch(t) {
        case STRING_NONE: return;
        case STRING_U8: PRINT(ctx, "u8"); return;
        case STRING_WCHAR: PRINT(ctx, "L"); return;
        case STRING_16: PRINT(ctx, "u"); return;
        case STRING_32: PRINT(ctx, "U"); return;
    }
}

#ifndef LEX_TOKEN_H
#define LEX_TOKEN_H

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
    TOKEN_MACRO_ARG,
    TOKEN_ERROR_L,
    TOKEN_PLACEHOLDER_L,
    TOKEN_UNKNOWN_L,
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
    TOKEN_PUNC_ARROW,
    TOKEN_PUNC_PLUS_PLUS,
    TOKEN_PUNC_MINUS_MINUS,
    TOKEN_PUNC_PLUS,
    TOKEN_PUNC_MINUS,
    TOKEN_PUNC_PLUS_EQUAL,
    TOKEN_PUNC_MINUS_EQUAL,
};

// Is a space required between the left and right tokens?
// A space is required when re-lexing the source file with the two tokens
// adjacent would be different compared with having a space inbetween.
// This function is not perfectly accurate, todo in the future
// Does not deal with TOKEN_INTEGER_L or TOKEN_FLOATING_L, assumes no spacing
// - todo when integer/floating parsing is implemented
bool TokenPasteAvoidance(LexerToken* left, LexerToken* right) {
    if(left->type == TOKEN_PP_NUMBER && right->type == TOKEN_PUNC_DOT) {
        return true;
    }

    if(left->type == TOKEN_PP_NUMBER && left->data.string.count > 0) {
        char c = left->data.string.buffer[left->data.string.count-1];
        if(c == 'e' || c == 'E' || c == 'p' || c == 'P') {
            for(unsigned int i = 0; i < sizeof(couldBeInNumber)/sizeof(LexerTokenType); i++) {
                if(couldBeInNumber[i] == right->type) return true;
            }
        }
    }

    // string and charater literal u/U/L/u8 prefix avoidance
    if(right->type == TOKEN_STRING_L || right->type == TOKEN_CHARACTER_L) {
        if(left->type != TOKEN_PP_NUMBER && left->type != TOKEN_IDENTIFIER_L) {
            return false;
        }

        return right->data.string.type != STRING_NONE;
    }

    bool leftIncluded = false;
    bool rightIncluded = false;
    for(unsigned int i = 0; i < sizeof(stringLikeTokens)/sizeof(LexerTokenType); i++) {
        if(stringLikeTokens[i] == left->type) leftIncluded = true;
        if(stringLikeTokens[i] == right->type) rightIncluded = true;
        if(leftIncluded && rightIncluded) break;
    }

    if(leftIncluded && rightIncluded) {
        return true;
    }

    leftIncluded = false;
    rightIncluded = false;

    for(unsigned int i = 0; i < sizeof(puncLikeTokens)/sizeof(LexerTokenType); i++) {
        if(puncLikeTokens[i] == left->type) leftIncluded = true;
        if(puncLikeTokens[i] == right->type) rightIncluded = true;
        if(leftIncluded && rightIncluded) break;
    }

    return leftIncluded && rightIncluded;
}

#endif

#define TokenPrintCtxInit JOIN(TokenPrintCtxInit, PRINT_TYPE_NAME)
static void TokenPrintCtxInit(TokenPrintCtx* ctx, PRINT_TYPE file, TranslationContext* alloc) {
    ctx->tokenPrinterAtStart = true;
    ctx->debugPrint = false;
    ctx->previousPrinted = (LexerToken){.type = TOKEN_EOF_L};
    ctx->ctx = alloc;
    ctx->file = file;
}

#define TokenPrint JOIN(TokenPrint, PRINT_TYPE_NAME)
static void TokenPrint(TokenPrintCtx* ctx, LexerToken* tok) {
#if PRINT_NUMERIC_ID == 0
    if(ctx->debugPrint) {
        if(tok->loc) {
            PRINT(ctx, tok->loc->line);
            PRINT(ctx, ":");
            PRINT(ctx, tok->loc->column);
        } else {
            PRINT(ctx, "<no location>");
        }
        if(tok->renderStartOfLine){
            PRINT(ctx, " bol");
        }
        if(tok->whitespaceBefore) {
            PRINT(ctx, " white=");
            PRINT(ctx, tok->indent);
        }
        PRINT(ctx, " token=");
        PRINT(ctx, tok->type);
        PRINT(ctx, " data(");
        if(tok->loc) {
            PRINT(ctx, tok->loc->length);
        } else {
            PRINT(ctx, "<no length>");
        }
        PRINT(ctx, ") ");
    } else {
        bool printedWhitespace = false;
        if(tok->renderStartOfLine && !ctx->tokenPrinterAtStart) {
            PRINT(ctx, "\n");
            printedWhitespace = true;
        }
        ctx->tokenPrinterAtStart = false;
        if(tok->whitespaceBefore) {
            for(size_t i = 0; i < tok->indent; i++){
                PRINT(ctx, " ");
            }
            printedWhitespace |= tok->indent > 0;
        }
        if(!printedWhitespace && TokenPasteAvoidance(&ctx->previousPrinted, tok)) {
            PRINT(ctx, " ");
        }
    }
#elif PRINT_NUMERIC_ID == 1
    // print one or minimal space to the string buffer for # operator
    if(ctx->file->count != 0 &&
        (tok->indent > 0 || tok->renderStartOfLine)) {
             PRINT(ctx, " ");
    }
#endif

    switch(tok->type) {
        case TOKEN_KW_AUTO: PRINT(ctx, "auto"); break;
        case TOKEN_KW_BREAK: PRINT(ctx, "break"); break;
        case TOKEN_KW_CASE: PRINT(ctx, "case"); break;
        case TOKEN_KW_CHAR: PRINT(ctx, "char"); break;
        case TOKEN_KW_CONST: PRINT(ctx, "const"); break;
        case TOKEN_KW_CONTINUE: PRINT(ctx, "continue"); break;
        case TOKEN_KW_DEFAULT: PRINT(ctx, "default"); break;
        case TOKEN_KW_DO: PRINT(ctx, "do"); break;
        case TOKEN_KW_DOUBLE: PRINT(ctx, "double"); break;
        case TOKEN_KW_ELSE: PRINT(ctx, "else"); break;
        case TOKEN_KW_ENUM: PRINT(ctx, "enum"); break;
        case TOKEN_KW_EXTERN: PRINT(ctx, "extern"); break;
        case TOKEN_KW_FLOAT: PRINT(ctx, "float"); break;
        case TOKEN_KW_FOR: PRINT(ctx, "for"); break;
        case TOKEN_KW_GOTO: PRINT(ctx, "goto"); break;
        case TOKEN_KW_IF: PRINT(ctx, "if"); break;
        case TOKEN_KW_INLINE: PRINT(ctx, "inline"); break;
        case TOKEN_KW_INT: PRINT(ctx, "int"); break;
        case TOKEN_KW_LONG: PRINT(ctx, "long"); break;
        case TOKEN_KW_REGISTER: PRINT(ctx, "register"); break;
        case TOKEN_KW_RESTRICT: PRINT(ctx, "restrict"); break;
        case TOKEN_KW_RETURN: PRINT(ctx, "return"); break;
        case TOKEN_KW_SHORT: PRINT(ctx, "short"); break;
        case TOKEN_KW_SIGNED: PRINT(ctx, "signed"); break;
        case TOKEN_KW_SIZEOF: PRINT(ctx, "sizeof"); break;
        case TOKEN_KW_STATIC: PRINT(ctx, "static"); break;
        case TOKEN_KW_STRUCT: PRINT(ctx, "struct"); break;
        case TOKEN_KW_SWITCH: PRINT(ctx, "switch"); break;
        case TOKEN_KW_TYPEDEF: PRINT(ctx, "typedef"); break;
        case TOKEN_KW_UNION: PRINT(ctx, "union"); break;
        case TOKEN_KW_UNSIGNED: PRINT(ctx, "unsigned"); break;
        case TOKEN_KW_VOID: PRINT(ctx, "void"); break;
        case TOKEN_KW_VOLATILE: PRINT(ctx, "volatile"); break;
        case TOKEN_KW_WHILE: PRINT(ctx, "while"); break;
        case TOKEN_KW_ALIGNAS: PRINT(ctx, "_Alignas"); break;
        case TOKEN_KW_ALIGNOF: PRINT(ctx, "_Alignof"); break;
        case TOKEN_KW_ATOMIC: PRINT(ctx, "_Atomic"); break;
        case TOKEN_KW_BOOL: PRINT(ctx, "_Bool"); break;
        case TOKEN_KW_COMPLEX: PRINT(ctx, "_Complex"); break;
        case TOKEN_KW_GENERIC: PRINT(ctx, "_Generic"); break;
        case TOKEN_KW_IMAGINARY: PRINT(ctx, "_Imaginary"); break;
        case TOKEN_KW_NORETURN: PRINT(ctx, "_Noreturn"); break;
        case TOKEN_KW_STATICASSERT: PRINT(ctx, "_Static_assert"); break;
        case TOKEN_KW_THREADLOCAL: PRINT(ctx, "_Thread_local"); break;
        case TOKEN_PUNC_LEFT_SQUARE: PRINT(ctx, "["); break;
        case TOKEN_PUNC_RIGHT_SQUARE: PRINT(ctx, "]"); break;
        case TOKEN_PUNC_LEFT_PAREN: PRINT(ctx, "("); break;
        case TOKEN_PUNC_RIGHT_PAREN: PRINT(ctx, ")"); break;
        case TOKEN_PUNC_LEFT_BRACE: PRINT(ctx, "{"); break;
        case TOKEN_PUNC_RIGHT_BRACE: PRINT(ctx, "}"); break;
        case TOKEN_PUNC_DOT: PRINT(ctx, "."); break;
        case TOKEN_PUNC_ARROW: PRINT(ctx, "->"); break;
        case TOKEN_PUNC_PLUS_PLUS: PRINT(ctx, "++"); break;
        case TOKEN_PUNC_MINUS_MINUS: PRINT(ctx, "--"); break;
        case TOKEN_PUNC_AND: PRINT(ctx, "&"); break;
        case TOKEN_PUNC_STAR: PRINT(ctx, "*"); break;
        case TOKEN_PUNC_PLUS: PRINT(ctx, "+"); break;
        case TOKEN_PUNC_MINUS: PRINT(ctx, "-"); break;
        case TOKEN_PUNC_TILDE: PRINT(ctx, "~"); break;
        case TOKEN_PUNC_BANG: PRINT(ctx, "!"); break;
        case TOKEN_PUNC_SLASH: PRINT(ctx, "/"); break;
        case TOKEN_PUNC_PERCENT: PRINT(ctx, "%"); break;
        case TOKEN_PUNC_LESS_LESS: PRINT(ctx, "<<"); break;
        case TOKEN_PUNC_GREATER_GREATER: PRINT(ctx, ">>"); break;
        case TOKEN_PUNC_LESS: PRINT(ctx, "<"); break;
        case TOKEN_PUNC_GREATER: PRINT(ctx, ">"); break;
        case TOKEN_PUNC_LESS_EQUAL: PRINT(ctx, "<="); break;
        case TOKEN_PUNC_GREATER_EQUAL: PRINT(ctx, ">="); break;
        case TOKEN_PUNC_EQUAL_EQUAL: PRINT(ctx, "=="); break;
        case TOKEN_PUNC_BANG_EQUAL: PRINT(ctx, "!="); break;
        case TOKEN_PUNC_CARET: PRINT(ctx, "^"); break;
        case TOKEN_PUNC_OR: PRINT(ctx, "|"); break;
        case TOKEN_PUNC_AND_AND: PRINT(ctx, "&&"); break;
        case TOKEN_PUNC_OR_OR: PRINT(ctx, "||"); break;
        case TOKEN_PUNC_QUESTION: PRINT(ctx, "?"); break;
        case TOKEN_PUNC_COLON: PRINT(ctx, ":"); break;
        case TOKEN_PUNC_SEMICOLON: PRINT(ctx, ";"); break;
        case TOKEN_PUNC_ELIPSIS: PRINT(ctx, "..."); break;
        case TOKEN_PUNC_EQUAL: PRINT(ctx, "="); break;
        case TOKEN_PUNC_STAR_EQUAL: PRINT(ctx, "*="); break;
        case TOKEN_PUNC_SLASH_EQUAL: PRINT(ctx, "/="); break;
        case TOKEN_PUNC_PERCENT_EQUAL: PRINT(ctx, "%="); break;
        case TOKEN_PUNC_PLUS_EQUAL: PRINT(ctx, "+="); break;
        case TOKEN_PUNC_MINUS_EQUAL: PRINT(ctx, "-="); break;
        case TOKEN_PUNC_LESS_LESS_EQUAL: PRINT(ctx, "<<="); break;
        case TOKEN_PUNC_GREATER_GREATER_EQUAL: PRINT(ctx, ">>="); break;
        case TOKEN_PUNC_AND_EQUAL: PRINT(ctx, "&="); break;
        case TOKEN_PUNC_CARET_EQUAL: PRINT(ctx, "^="); break;
        case TOKEN_PUNC_PIPE_EQUAL: PRINT(ctx, "|="); break;
        case TOKEN_PUNC_COMMA: PRINT(ctx, ","); break;
        case TOKEN_PUNC_HASH: PRINT(ctx, "#"); break;
        case TOKEN_PUNC_HASH_HASH: PRINT(ctx, "##"); break;
        case TOKEN_PUNC_LESS_COLON: PRINT(ctx, "<:"); break;
        case TOKEN_PUNC_COLON_GREATER: PRINT(ctx, ":>"); break;
        case TOKEN_PUNC_LESS_PERCENT: PRINT(ctx, "<%"); break;
        case TOKEN_PUNC_PERCENT_GREATER: PRINT(ctx, "%>"); break;
        case TOKEN_PUNC_PERCENT_COLON: PRINT(ctx, "%:"); break;
        case TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON: PRINT(ctx, "%:%:"); break;
        case TOKEN_HEADER_NAME:
            PRINT(ctx, "\"");
            PRINT(ctx, tok->data.string.buffer);
            PRINT(ctx, "\""); break;
        case TOKEN_SYS_HEADER_NAME:
            PRINT(ctx, "<");
            PRINT(ctx, tok->data.string.buffer);
            PRINT(ctx, ">"); break;
        case TOKEN_PP_NUMBER: PRINT(ctx, tok->data.string.buffer); break;
        case TOKEN_IDENTIFIER_L: PRINT(ctx, tok->data.node->name.data.string.buffer); break;
        case TOKEN_INTEGER_L: PRINT(ctx, tok->data.integer); break;
        case TOKEN_FLOATING_L: PRINT(ctx, tok->data.floating); break;
        case TOKEN_CHARACTER_L:
            StringTypePrint(tok->data.string.type, ctx);
            PRINT(ctx, "\'");
            PRINT_ESCAPE(ctx, tok->data.string.buffer, tok->data.string.count);
            PRINT(ctx, "\'"); break;
        case TOKEN_STRING_L:
            StringTypePrint(tok->data.string.type, ctx);
#if PRINT_NUMERIC_ID == 0
            PRINT(ctx, "\"");
            PRINT_ESCAPE(ctx, tok->data.string.buffer, tok->data.string.count);
            PRINT(ctx, "\"");
#elif PRINT_NUMERIC_ID == 1
            PRINT(ctx, "\\\"");
            PRINT_ESCAPE(ctx, tok->data.string.buffer, tok->data.string.count);
            PRINT(ctx, "\\\"");
#endif
            break;
        case TOKEN_MACRO_ARG:
            PRINT(ctx, "argument(");
            PRINT(ctx, tok->data.integer);
            PRINT(ctx, ")");
            break;
        case TOKEN_UNKNOWN_L: PRINT(ctx, tok->data.character); break;
        case TOKEN_PLACEHOLDER_L: PRINT(ctx, "placeholder"); break;
        case TOKEN_ERROR_L: PRINT(ctx, "error token"); break;
        case TOKEN_EOF_L: break;
    }

    if(ctx->debugPrint) {
        PRINT(ctx, "\n");
    }
    ctx->previousPrinted = *tok;
}

#undef PRINT_TYPE
#undef PRINT_TYPE_NAME
#undef PRINT_NUMERIC_ID
#undef PRINT
#undef PRINT_ESCAPE
#undef JOIN
#undef JOIN_
#undef StringTypePrint
#undef TokenPrintCtx
#undef TokenPrintCtxInit
#undef TokenPrint
