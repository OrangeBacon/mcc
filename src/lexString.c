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

static void expandString(struct TranslationContext* ctx, LexerString* str, size_t len) {
    if(str->count + len > str->capacity) {
        size_t newLen = max(str->capacity * 2, str->count + len) + 1;
        char* buffer = memoryArrayPushN(&ctx->stringArr, newLen);
        strncpy(buffer, str->buffer, str->capacity);
        str->buffer = buffer;
        str->capacity = newLen;
    }
}

void LexerStringAddChar(LexerString* str, struct TranslationContext* ctx, char c) {
    expandString(ctx, str, 1);

    str->buffer[str->count] = c;
    str->buffer[str->count + 1] = '\0';
    str->count++;
}


void LexerStringAddString(LexerString* str, struct TranslationContext* ctx, const char* c) {
    size_t len = strlen(c);
    expandString(ctx, str, len);

    strcat(str->buffer, c);
    str->count += len;
}

void LexerStringAddInt(LexerString* str, struct TranslationContext* ctx, int val) {
    size_t len = snprintf(NULL, 0, "%d", val)+1;
    expandString(ctx, str, len);
    snprintf(&str->buffer[str->count], len, "%d", val);
    str->count += len;
}

void LexerStringAddSizeT(LexerString* str, struct TranslationContext* ctx, size_t val) {
    size_t len = snprintf(NULL, 0, "%llu", val)+1;
    expandString(ctx, str, len);
    snprintf(&str->buffer[str->count], len, "%llu", val);
    str->count += len;
}

void LexerStringAddIntMaxT(LexerString* str, struct TranslationContext* ctx, intmax_t val) {
    size_t len = snprintf(NULL, 0, "%lld", val)+1;
    expandString(ctx, str, len);
    snprintf(&str->buffer[str->count], len, "%lld", val);
    str->count += len;
}

void LexerStringAddDouble(LexerString* str, struct TranslationContext* ctx, double val) {
    size_t len = snprintf(NULL, 0, "%f", val)+1;
    expandString(ctx, str, len);
    snprintf(&str->buffer[str->count], len, "%f", val);
    str->count += len;
}

// print lowest byte of val as hexadecimal, leading zero padded
void LexerStringAdd2HexDigit(LexerString* str, struct TranslationContext* ctx, char val) {
    size_t len = snprintf(NULL, 0, "%02x", val & 0xff)+1;
    expandString(ctx, str, len);
    snprintf(&str->buffer[str->count], len, "%02x", val & 0xff);
    str->count += len;
}

// escape single character escape sequences from c,
// print printable characters
// for everything else use the '\xhh' format
// note: this will not work outside of the ## operator
void LexerStringAddEscapedChar(LexerString* str, struct TranslationContext* ctx, char val) {
    switch (val) {
        case '\"': LexerStringAddString(str, ctx, "\\\""); return;
        case '\\': LexerStringAddString(str, ctx, "\\\\"); return;
        default:
            if (val >= ' ' && val <= '~') {
                LexerStringAddChar(str, ctx, val);
            } else {
                LexerStringAddString(str, ctx, "\\x");
                LexerStringAdd2HexDigit(str, ctx, val);
            }
    }
}

// add a string to the LexerString, escaping all the characters
void LexerStringAddEscapedString(LexerString* str, struct TranslationContext* ctx, const char* val) {
    char c;
    while((c = *val++)) {
        LexerStringAddEscapedChar(str, ctx, c);
    }
}
