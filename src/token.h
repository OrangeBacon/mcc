#ifndef TOKEN_H
#define TOKEN_H

#define FOREACH_TOKEN(x) \
    x(IDENTIFIER) x(LEFT_PAREN) x(RIGHT_PAREN) \
    x(LEFT_BRACE) x(RIGHT_BRACE) x(RETURN) \
    x(INTEGER) x(SEMICOLON) x(INT) \
    x(ERROR) x(EOF)

#define ENUM_TOKEN(x) TOKEN_##x,
typedef enum TokenType {
    FOREACH_TOKEN(ENUM_TOKEN)
} TokenType;
#undef ENUM_TOKEN

const char* TokenTypeToString(TokenType type);

typedef struct Token {
    TokenType type;
    const char* start;
    int length;
    int line;
    int column;
} Token;

#endif