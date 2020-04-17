#include "token.h"

#include <stdio.h>

#define STRING_TOKEN(x) #x,
static const char* TokenNames[] = {
    FOREACH_TOKEN(STRING_TOKEN)
};
#undef STRING_TOKEN

void TokenPrint(Token* token) {
    printf("%s: %d:%d '%.*s'", TokenNames[token->type], token->line, token->column, token->length, token->start);
}

Token TokenMake(TokenType type) {
    Token t;
    t.type = type;
    t.start = "internal";
    t.length = 0;
    t.column = -1;
    t.line = -1;

    return t;
}