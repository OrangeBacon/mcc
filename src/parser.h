#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "scanner.h"
#include "ast.h"
#include "symbolTable.h"

typedef struct Parser {
    Scanner* scanner;
    ASTTranslationUnit* ast;

    Token previous;
    Token current;

    bool hadError;
    bool panicMode;

    SymbolTable locals;
    int stackIndex;
} Parser;

void ParserInit(Parser* parser, char* fileName);

void errorAt(Parser* parser, Token* loc, const char* message);

bool ParserRun(Parser* parser);

#endif