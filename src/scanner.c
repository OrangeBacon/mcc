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
        case 'r': return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 'e': return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'i': if(scanner->current - scanner->start > 1) {
            switch(scanner->start[1]) {
                case 'n': return checkKeyword(scanner, 2, 1, "t", TOKEN_INT);
                case 'f': return checkKeyword(scanner, 2, 0, "", TOKEN_IF);
            }
        }; break;
        case 'f': return checkKeyword(scanner, 1, 2, "or", TOKEN_FOR);
        case 'w': return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);
        case 'd': return checkKeyword(scanner, 1, 1, "o", TOKEN_DO);
        case 'c': return checkKeyword(scanner, 1, 7, "ontinue", TOKEN_CONTINUE);
        case 'b': return checkKeyword(scanner, 1, 4, "reak", TOKEN_BREAK);
        case 's': return checkKeyword(scanner, 1, 5, "izeof", TOKEN_SIZEOF);
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
        case '~': makeToken(scanner, token, TOKEN_COMPLIMENT); return;
        case ',': makeToken(scanner, token, TOKEN_COMMA); return;
        case '?': makeToken(scanner, token, TOKEN_QUESTION); return;
        case ':': makeToken(scanner, token, TOKEN_COLON); return;

        case '*': makeToken(scanner, token, match(scanner, '=')?
            TOKEN_STAR_EQUAL:TOKEN_STAR); return;
        case '/': makeToken(scanner, token, match(scanner, '=')?
            TOKEN_SLASH_EQUAL:TOKEN_SLASH); return;
        case '%': makeToken(scanner, token, match(scanner, '=')?
            TOKEN_PERCENT_EQUAL:TOKEN_PERCENT); return;
        case '!': makeToken(scanner, token, match(scanner, '=')?
            TOKEN_NOT_EQUAL:TOKEN_NOT); return;
        case '=': makeToken(scanner, token, match(scanner, '=')?
            TOKEN_EQUAL_EQUAL:TOKEN_EQUAL); return;
        case '^': makeToken(scanner, token, match(scanner, '=')?
            TOKEN_XOR_EQUAL:TOKEN_XOR); return;

        case '+': makeToken(scanner, token,
            match(scanner, '+') ? TOKEN_PLUS_PLUS :
            match(scanner, '=') ? TOKEN_PLUS_EQUAL :
            TOKEN_PLUS); return;
        case '-': makeToken(scanner, token,
            match(scanner, '-') ? TOKEN_MINUS_MINUS :
            match(scanner, '=') ? TOKEN_MINUS_EQUAL :
            TOKEN_NEGATE); return;
        case '&': makeToken(scanner, token,
            match(scanner, '&') ? TOKEN_AND_AND :
            match(scanner, '=') ? TOKEN_AND_EQUAL :
            TOKEN_AND); return;
        case '|': makeToken(scanner, token,
            match(scanner, '|') ? TOKEN_OR_OR :
            match(scanner, '=') ? TOKEN_OR_EQUAL :
            TOKEN_OR); return;

        case '<': makeToken(scanner, token,
            match(scanner, '=') ? TOKEN_LESS_EQUAL :
            match(scanner, '<') ? (match(scanner, '=') ?
                TOKEN_LEFT_SHIFT_EQUAL:TOKEN_SHIFT_LEFT) :
            TOKEN_LESS); return;
        case '>': makeToken(scanner, token,
            match(scanner, '=') ? TOKEN_GREATER_EQUAL :
            match(scanner, '>') ? (match(scanner, '=') ?
                TOKEN_RIGHT_SHIFT_EQUAL:TOKEN_SHIFT_RIGHT) :
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