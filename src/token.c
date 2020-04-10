#include "token.h"

#include <stdio.h>

#define STRING_TOKEN(x) #x,
static const char* TokenNames[] = {
    FOREACH_TOKEN(STRING_TOKEN)
};
#undef STRING_TOKEN

void TokenPrint(Token* token) {
    printf("%s: %u:%u '%.*s'", TokenNames[token->type], token->line, token->column, token->length, token->start);
}