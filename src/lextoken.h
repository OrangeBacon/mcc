#include "lex.h"
#include <stdio.h>

// This file implements a token printer.  The printer needs to work with
// both FILE* pointers and LexerString buffers, so macros are used to define
// the string handling, so this header can be included twice with different
// macro definitions, to avoid duplicating code.
// It also includes options to have the printed output escape special characters
// e.g. if printing " or \ in a string literal.

static void StringTypePrint(LexerStringType t, FILE* file) {
    switch(t) {
        case STRING_NONE: return;
        case STRING_U8: fprintf(file, "u8"); return;
        case STRING_WCHAR: fprintf(file, "L"); return;
        case STRING_16: fprintf(file, "u"); return;
        case STRING_32: fprintf(file, "U"); return;
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

typedef struct TokenPrintCtx {
    bool debugPrint;
    bool tokenPrinterAtStart;
    LexerToken previousPrinted;
    FILE* file;
} TokenPrintCtx;

static void TokenPrintCtxInit(TokenPrintCtx* ctx, FILE* file) {
    ctx->tokenPrinterAtStart = true;
    ctx->debugPrint = false;
    ctx->previousPrinted = (LexerToken){.type = TOKEN_EOF_L};
    ctx->file = file;
}

static void TokenPrint(TokenPrintCtx* ctx, LexerToken* tok) {
    if(ctx->debugPrint) {
        fprintf(ctx->file, "%llu:%llu", tok->loc->line, tok->loc->column);
        if(tok->renderStartOfLine) fprintf(ctx->file, " bol");
        if(tok->whitespaceBefore) fprintf(ctx->file, " white=%llu", tok->indent);
        fprintf(ctx->file, " token=%d", tok->type);
        fprintf(ctx->file, " data(%llu) ", tok->loc->length);
    } else {
        bool printedWhitespace = false;
        if(tok->renderStartOfLine && !ctx->tokenPrinterAtStart) {
            fprintf(ctx->file, "\n");
            printedWhitespace = true;
        }
        ctx->tokenPrinterAtStart = false;
        if(tok->whitespaceBefore) {
            fprintf(ctx->file, "%*s", (int)tok->indent, "");
            printedWhitespace |= tok->indent > 0;
        }
        if(!printedWhitespace && TokenPasteAvoidance(&ctx->previousPrinted, tok)) {
            fprintf(ctx->file, " ");
        }
    }
    switch(tok->type) {
        case TOKEN_KW_AUTO: fprintf(ctx->file, "auto"); break;
        case TOKEN_KW_BREAK: fprintf(ctx->file, "break"); break;
        case TOKEN_KW_CASE: fprintf(ctx->file, "case"); break;
        case TOKEN_KW_CHAR: fprintf(ctx->file, "char"); break;
        case TOKEN_KW_CONST: fprintf(ctx->file, "const"); break;
        case TOKEN_KW_CONTINUE: fprintf(ctx->file, "continue"); break;
        case TOKEN_KW_DEFAULT: fprintf(ctx->file, "default"); break;
        case TOKEN_KW_DO: fprintf(ctx->file, "do"); break;
        case TOKEN_KW_DOUBLE: fprintf(ctx->file, "double"); break;
        case TOKEN_KW_ELSE: fprintf(ctx->file, "else"); break;
        case TOKEN_KW_ENUM: fprintf(ctx->file, "enum"); break;
        case TOKEN_KW_EXTERN: fprintf(ctx->file, "extern"); break;
        case TOKEN_KW_FLOAT: fprintf(ctx->file, "float"); break;
        case TOKEN_KW_FOR: fprintf(ctx->file, "for"); break;
        case TOKEN_KW_GOTO: fprintf(ctx->file, "goto"); break;
        case TOKEN_KW_IF: fprintf(ctx->file, "if"); break;
        case TOKEN_KW_INLINE: fprintf(ctx->file, "inline"); break;
        case TOKEN_KW_INT: fprintf(ctx->file, "int"); break;
        case TOKEN_KW_LONG: fprintf(ctx->file, "long"); break;
        case TOKEN_KW_REGISTER: fprintf(ctx->file, "register"); break;
        case TOKEN_KW_RESTRICT: fprintf(ctx->file, "restrict"); break;
        case TOKEN_KW_RETURN: fprintf(ctx->file, "return"); break;
        case TOKEN_KW_SHORT: fprintf(ctx->file, "short"); break;
        case TOKEN_KW_SIGNED: fprintf(ctx->file, "signed"); break;
        case TOKEN_KW_SIZEOF: fprintf(ctx->file, "sizeof"); break;
        case TOKEN_KW_STATIC: fprintf(ctx->file, "static"); break;
        case TOKEN_KW_STRUCT: fprintf(ctx->file, "struct"); break;
        case TOKEN_KW_SWITCH: fprintf(ctx->file, "switch"); break;
        case TOKEN_KW_TYPEDEF: fprintf(ctx->file, "typedef"); break;
        case TOKEN_KW_UNION: fprintf(ctx->file, "union"); break;
        case TOKEN_KW_UNSIGNED: fprintf(ctx->file, "unsigned"); break;
        case TOKEN_KW_VOID: fprintf(ctx->file, "void"); break;
        case TOKEN_KW_VOLATILE: fprintf(ctx->file, "volatile"); break;
        case TOKEN_KW_WHILE: fprintf(ctx->file, "while"); break;
        case TOKEN_KW_ALIGNAS: fprintf(ctx->file, "_Alignas"); break;
        case TOKEN_KW_ALIGNOF: fprintf(ctx->file, "_Alignof"); break;
        case TOKEN_KW_ATOMIC: fprintf(ctx->file, "_Atomic"); break;
        case TOKEN_KW_BOOL: fprintf(ctx->file, "_Bool"); break;
        case TOKEN_KW_COMPLEX: fprintf(ctx->file, "_Complex"); break;
        case TOKEN_KW_GENERIC: fprintf(ctx->file, "_Generic"); break;
        case TOKEN_KW_IMAGINARY: fprintf(ctx->file, "_Imaginary"); break;
        case TOKEN_KW_NORETURN: fprintf(ctx->file, "_Noreturn"); break;
        case TOKEN_KW_STATICASSERT: fprintf(ctx->file, "_Static_assert"); break;
        case TOKEN_KW_THREADLOCAL: fprintf(ctx->file, "_Thread_local"); break;
        case TOKEN_PUNC_LEFT_SQUARE: fprintf(ctx->file, "["); break;
        case TOKEN_PUNC_RIGHT_SQUARE: fprintf(ctx->file, "]"); break;
        case TOKEN_PUNC_LEFT_PAREN: fprintf(ctx->file, "("); break;
        case TOKEN_PUNC_RIGHT_PAREN: fprintf(ctx->file, ")"); break;
        case TOKEN_PUNC_LEFT_BRACE: fprintf(ctx->file, "{"); break;
        case TOKEN_PUNC_RIGHT_BRACE: fprintf(ctx->file, "}"); break;
        case TOKEN_PUNC_DOT: fprintf(ctx->file, "."); break;
        case TOKEN_PUNC_ARROW: fprintf(ctx->file, "->"); break;
        case TOKEN_PUNC_PLUS_PLUS: fprintf(ctx->file, "++"); break;
        case TOKEN_PUNC_MINUS_MINUS: fprintf(ctx->file, "--"); break;
        case TOKEN_PUNC_AND: fprintf(ctx->file, "&"); break;
        case TOKEN_PUNC_STAR: fprintf(ctx->file, "*"); break;
        case TOKEN_PUNC_PLUS: fprintf(ctx->file, "+"); break;
        case TOKEN_PUNC_MINUS: fprintf(ctx->file, "-"); break;
        case TOKEN_PUNC_TILDE: fprintf(ctx->file, "~"); break;
        case TOKEN_PUNC_BANG: fprintf(ctx->file, "!"); break;
        case TOKEN_PUNC_SLASH: fprintf(ctx->file, "/"); break;
        case TOKEN_PUNC_PERCENT: fprintf(ctx->file, "%%"); break;
        case TOKEN_PUNC_LESS_LESS: fprintf(ctx->file, "<<"); break;
        case TOKEN_PUNC_GREATER_GREATER: fprintf(ctx->file, ">>"); break;
        case TOKEN_PUNC_LESS: fprintf(ctx->file, "<"); break;
        case TOKEN_PUNC_GREATER: fprintf(ctx->file, ">"); break;
        case TOKEN_PUNC_LESS_EQUAL: fprintf(ctx->file, "<="); break;
        case TOKEN_PUNC_GREATER_EQUAL: fprintf(ctx->file, ">="); break;
        case TOKEN_PUNC_EQUAL_EQUAL: fprintf(ctx->file, "=="); break;
        case TOKEN_PUNC_BANG_EQUAL: fprintf(ctx->file, "!="); break;
        case TOKEN_PUNC_CARET: fprintf(ctx->file, "^"); break;
        case TOKEN_PUNC_OR: fprintf(ctx->file, "|"); break;
        case TOKEN_PUNC_AND_AND: fprintf(ctx->file, "&&"); break;
        case TOKEN_PUNC_OR_OR: fprintf(ctx->file, "||"); break;
        case TOKEN_PUNC_QUESTION: fprintf(ctx->file, "?"); break;
        case TOKEN_PUNC_COLON: fprintf(ctx->file, ":"); break;
        case TOKEN_PUNC_SEMICOLON: fprintf(ctx->file, ";"); break;
        case TOKEN_PUNC_ELIPSIS: fprintf(ctx->file, "..."); break;
        case TOKEN_PUNC_EQUAL: fprintf(ctx->file, "="); break;
        case TOKEN_PUNC_STAR_EQUAL: fprintf(ctx->file, "*="); break;
        case TOKEN_PUNC_SLASH_EQUAL: fprintf(ctx->file, "/="); break;
        case TOKEN_PUNC_PERCENT_EQUAL: fprintf(ctx->file, "%%="); break;
        case TOKEN_PUNC_PLUS_EQUAL: fprintf(ctx->file, "+="); break;
        case TOKEN_PUNC_MINUS_EQUAL: fprintf(ctx->file, "-="); break;
        case TOKEN_PUNC_LESS_LESS_EQUAL: fprintf(ctx->file, "<<="); break;
        case TOKEN_PUNC_GREATER_GREATER_EQUAL: fprintf(ctx->file, ">>="); break;
        case TOKEN_PUNC_AND_EQUAL: fprintf(ctx->file, "&="); break;
        case TOKEN_PUNC_CARET_EQUAL: fprintf(ctx->file, "^="); break;
        case TOKEN_PUNC_PIPE_EQUAL: fprintf(ctx->file, "|="); break;
        case TOKEN_PUNC_COMMA: fprintf(ctx->file, ","); break;
        case TOKEN_PUNC_HASH: fprintf(ctx->file, "#"); break;
        case TOKEN_PUNC_HASH_HASH: fprintf(ctx->file, "##"); break;
        case TOKEN_PUNC_LESS_COLON: fprintf(ctx->file, "<:"); break;
        case TOKEN_PUNC_COLON_GREATER: fprintf(ctx->file, ":>"); break;
        case TOKEN_PUNC_LESS_PERCENT: fprintf(ctx->file, "<%%"); break;
        case TOKEN_PUNC_PERCENT_GREATER: fprintf(ctx->file, "%%>"); break;
        case TOKEN_PUNC_PERCENT_COLON: fprintf(ctx->file, "%%:"); break;
        case TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON: fprintf(ctx->file, "%%:%%:"); break;
        case TOKEN_HEADER_NAME: fprintf(ctx->file, "\"%s\"", tok->data.string.buffer); break;
        case TOKEN_SYS_HEADER_NAME: fprintf(ctx->file, "<%s>", tok->data.string.buffer); break;
        case TOKEN_PP_NUMBER: fprintf(ctx->file, "%s", tok->data.string.buffer); break;
        case TOKEN_IDENTIFIER_L: fprintf(ctx->file, "%s", tok->data.node->name.data.string.buffer); break;
        case TOKEN_INTEGER_L: fprintf(ctx->file, "%llu", tok->data.integer); break;
        case TOKEN_FLOATING_L: fprintf(ctx->file, "%f", tok->data.floating); break;
        case TOKEN_CHARACTER_L: StringTypePrint(tok->data.string.type, ctx->file); fprintf(ctx->file, "'%s'", tok->data.string.buffer); break;
        case TOKEN_STRING_L: StringTypePrint(tok->data.string.type, ctx->file); fprintf(ctx->file, "\"%s\"", tok->data.string.buffer); break;
        case TOKEN_MACRO_ARG: fprintf(ctx->file, "argument(%lld)", tok->data.integer); break;
        case TOKEN_UNKNOWN_L: fprintf(ctx->file, "%c", tok->data.character); break;
        case TOKEN_ERROR_L: fprintf(ctx->file, "error token"); break;
        case TOKEN_EOF_L: break;
    }

    if(ctx->debugPrint) {
        fprintf(ctx->file, "\n");
    }
    ctx->previousPrinted = *tok;
}
