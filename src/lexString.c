#include "lexString.h"

#include <string.h>
#include "lex.h"

void LexerStringInit(LexerString* str, TranslationContext* ctx, size_t size) {
    str->buffer = memoryArrayPushN(&ctx->stringArr, size + 1);
    str->buffer[0] = '\0';
    str->capacity = size;
    str->count = 0;
    str->type = STRING_NONE;
}

#define max(a,b) ((a)>(b)?(a):(b))

static void expandString(TranslationContext* ctx, LexerString* str, size_t len) {
    if(str->count + len > str->capacity) {
        size_t newLen = max(str->capacity * 2, str->count + len) + 1;
        char* buffer = memoryArrayPushN(&ctx->stringArr, newLen);
        strncpy(buffer, str->buffer, str->capacity);
        str->buffer = buffer;
        str->capacity = newLen;
    }
}

void LexerStringAddChar(LexerString* str, TranslationContext* ctx, char c) {
    expandString(ctx, str, 1);

    str->buffer[str->count] = c;
    str->buffer[str->count + 1] = '\0';
    str->count++;
}


void LexerStringAddString(LexerString* str, TranslationContext* ctx, const char* c) {
    size_t len = strlen(c);
    expandString(ctx, str, len);

    strcat(str->buffer, c);
    str->count += len;
}

void LexerStringAddInt(LexerString* str, struct TranslationContext* ctx, int val) {
    size_t len = snprintf(NULL, 0, "%d", val);
    expandString(ctx, str, len);
    snprintf(&str->buffer[str->count], len, "%d", val);
    str->count += len;
}

void LexerStringAddSizeT(LexerString* str, struct TranslationContext* ctx, size_t val) {
    size_t len = snprintf(NULL, 0, "%llu", val);
    expandString(ctx, str, len);
    snprintf(&str->buffer[str->count], len, "%llu", val);
    str->count += len;
}

void LexerStringAddIntMaxT(LexerString* str, struct TranslationContext* ctx, intmax_t val) {
    size_t len = snprintf(NULL, 0, "%lld", val);
    expandString(ctx, str, len);
    snprintf(&str->buffer[str->count], len, "%lld", val);
    str->count += len;
}

void LexerStringAddDouble(LexerString* str, struct TranslationContext* ctx, double val) {
    size_t len = snprintf(NULL, 0, "%f", val);
    expandString(ctx, str, len);
    snprintf(&str->buffer[str->count], len, "%f", val);
    str->count += len;
}
