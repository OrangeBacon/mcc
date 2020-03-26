#ifndef TOKEN_H
#define TOKEN_H

#define FOREACH_TOKEN(x) \
    x(IDENTIFIER) x(LEFT_PAREN) x(RIGHT_PAREN) \
    x(LEFT_BRACE) x(RIGHT_BRACE) x(RETURN) \
    x(INTEGER) x(SEMICOLON) x(INT) \
    x(NEGATE) x(COMPLIMENT) x(NOT) \
    x(PLUS) x(STAR) x(SLASH) \
    x(AND_AND) x(OR_OR) x(EQUAL_EQUAL) \
    x(NOT_EQUAL) x(LESS) x(LESS_EQUAL) \
    x(GREATER) x(GREATER_EQUAL) x(AND) \
    x(OR) x(EQUAL) x(PERCENT) \
    x(SHIFT_LEFT) x(SHIFT_RIGHT) x(XOR) \
    x(COMMA) x(PLUS_PLUS) x(MINUS_MINUS) \
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
    int numberValue;
} Token;

void TokenPrint(Token* token);

#endif