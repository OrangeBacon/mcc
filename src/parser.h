#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "scanner.h"
#include "ast.h"

typedef struct Parser {
    Scanner* scanner;
    ASTTranslationUnit* ast;

    Token previous;
    Token current;

    bool hadError;
    bool panicMode;
} Parser;

void ParserInit(Parser* parser, Scanner* scanner);

bool ParserRun(Parser* parser);

#endif