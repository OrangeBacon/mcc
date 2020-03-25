#include "scanner.h"

#include <stdbool.h>
#include <string.h>
#include "file.h"
#include "token.h"

void ScannerInit(Scanner* scanner, char* fileName) {
    scanner->fileName = fileName;
    scanner->text = readFile(fileName);
    scanner->start = scanner->text;
    scanner->current = scanner->text;
    scanner->line = 1;
    scanner->column = 1;
}

static bool isAtEnd(Scanner* scanner) {
    return *scanner->current == '\0';
}

static void makeToken(Scanner* scanner, Token* token, TokenType type) {
    token->type = type;
    token->start = scanner->start;
    token->length = (int)(scanner->current - scanner->start);
    token->line = scanner->line;
    token->column = scanner->column;
}

static void errorToken(Scanner* scanner, Token* token, const char* message) {
    token->type = TOKEN_ERROR;
    token->start = message;
    token->length = (int)strlen(message);
    token->line = scanner->line;
    token->column = scanner->column;
}

static char advance(Scanner* scanner) {
    scanner->current++;
    scanner->column++;
    return scanner->current[-1];
}

static char peek(Scanner* scanner) {
    return *scanner->current;
}

static char peekNext(Scanner* scanner) {
    if(isAtEnd(scanner)) return '\0';
    return scanner->current[1];
}

static bool match(Scanner* scanner, char expected) {
    if(isAtEnd(scanner)) return false;
    if(*scanner->current != expected) return false;

    scanner->current++;
    scanner->column++;
    return true;
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_';
}

static void skipWhitespace(Scanner* scanner) {
    while(true) {
        char c = peek(scanner);
        switch(c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;

            case '\n':
                scanner->line++;
                scanner->column = 0;
                advance(scanner);
                break;

            case '/': {
                char next = peekNext(scanner);
                if(next == '/') {
                    while(peek(scanner) != '\n' && !isAtEnd(scanner)) {
                        advance(scanner);
                    }
                } else if(next == '*') {
                    advance(scanner);
                    advance(scanner);
                    while(!isAtEnd(scanner)) {
                        if(peek(scanner) == '*' && peekNext(scanner) == '/') {
                            break;
                        }
                        advance(scanner);
                    }
                    advance(scanner);
                    advance(scanner);
                } else {
                    return;
                }
            }; break;

            default:
                return;
        }
    }
}

static void number(Scanner* scanner, Token* token) {
    while(isDigit(peek(scanner))) {
        advance(scanner);
    }

    makeToken(scanner, token, TOKEN_INTEGER);
}

static TokenType checkKeyword(Scanner* scanner, int start, int length,
    const char* rest, TokenType type)
{
    if(scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identiferType(Scanner* scanner) {
    switch(scanner->start[0]) {
        case 'i': return checkKeyword(scanner, 1, 2, "nt", TOKEN_INT);
        case 'r': return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
    }

    return TOKEN_IDENTIFIER;
}

static void identifier(Scanner* scanner, Token* token) {
    while(isAlpha(peek(scanner)) || isDigit(peek(scanner))) {
        advance(scanner);
    }

    makeToken(scanner, token, identiferType(scanner));
}

void ScannerNext(Scanner* scanner, Token* token) {
    skipWhitespace(scanner);
    scanner->start = scanner->current;

    if(isAtEnd(scanner)) {
        makeToken(scanner, token, TOKEN_EOF);
        return;
    }

    char c = advance(scanner);

    switch(c) {
        case '{': makeToken(scanner, token, TOKEN_LEFT_BRACE); return;
        case '}': makeToken(scanner, token, TOKEN_RIGHT_BRACE); return;
        case '(': makeToken(scanner, token, TOKEN_LEFT_PAREN); return;
        case ')': makeToken(scanner, token, TOKEN_RIGHT_PAREN); return;
        case ';': makeToken(scanner, token, TOKEN_SEMICOLON); return;
        case '-': makeToken(scanner, token, TOKEN_NEGATE); return;
        case '~': makeToken(scanner, token, TOKEN_COMPLIMENT); return;
        case '+': makeToken(scanner, token, TOKEN_PLUS); return;
        case '*': makeToken(scanner, token, TOKEN_STAR); return;
        case '/': makeToken(scanner, token, TOKEN_SLASH); return;
        case '^': makeToken(scanner, token, TOKEN_XOR); return;
        case '%': makeToken(scanner, token, TOKEN_PERCENT); return;

        case '!': makeToken(scanner, token, match(scanner, '=')?
            TOKEN_NOT_EQUAL:TOKEN_NOT); return;
        case '&': makeToken(scanner, token, match(scanner, '&')?
            TOKEN_AND_AND:TOKEN_AND); return;
        case '|': makeToken(scanner, token, match(scanner, '|')?
            TOKEN_OR_OR:TOKEN_OR); return;
        case '=': makeToken(scanner, token, match(scanner, '=')?
            TOKEN_EQUAL_EQUAL:TOKEN_EQUAL); return;
        case '<': makeToken(scanner, token,
            match(scanner, '=') ? TOKEN_LESS_EQUAL :
            match(scanner, '<') ? TOKEN_SHIFT_LEFT :
            TOKEN_LESS); return;
        case '>': makeToken(scanner, token,
            match(scanner, '=')? TOKEN_GREATER_EQUAL :
            match(scanner, '>')? TOKEN_SHIFT_RIGHT :
            TOKEN_GREATER); return;
    }

    if(isDigit(c)) {
        number(scanner, token);
        return;
    }

    if(isAlpha(c)) {
        identifier(scanner, token);
        return;
    }

    errorToken(scanner, token, "Unexpected character.");
}