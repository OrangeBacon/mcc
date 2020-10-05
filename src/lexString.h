#ifndef LEX_STRING_H
#define LEX_STRING_H

#include <stddef.h>
#include <stdint.h>

typedef enum LexerStringType {
    STRING_NONE,
    STRING_U8,
    STRING_WCHAR,
    STRING_16,
    STRING_32,
} LexerStringType;

typedef struct LexerString {
    char* buffer;
    size_t capacity;
    size_t count;
    LexerStringType type;
} LexerString;

struct TranslationContext;

void LexerStringInit(LexerString* str, struct TranslationContext* ctx, size_t size);
void LexerStringAddString(LexerString* str, struct TranslationContext* ctx, const char* c);
void LexerStringAddChar(LexerString* str, struct TranslationContext* ctx, char c);
void LexerStringAddInt(LexerString* str, struct TranslationContext* ctx, int val);
void LexerStringAddSizeT(LexerString* str, struct TranslationContext* ctx, size_t val);
void LexerStringAddIntMaxT(LexerString* str, struct TranslationContext* ctx, intmax_t val);
void LexerStringAddDouble(LexerString* str, struct TranslationContext* ctx, double val);
void LexerStringAddMax2HexDigit(LexerString* str, struct TranslationContext* ctx, char c);
void LexerStringAddEscapedChar(LexerString* str, struct TranslationContext* ctx, char c);
void LexerStringAddEscapedString(LexerString* str, struct TranslationContext* ctx, const char* val);

#endif