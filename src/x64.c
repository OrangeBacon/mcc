#include "x64.h"

#include <stdio.h>

static void x64ASTGenExpression(ASTExpression* ast, FILE* f) {
    switch(ast->type) {
        case AST_EXPRESSION_INTEGER:
            fprintf(f, "\tpush $%i\n", ast->as.integer.numberValue);
    }
}

static void x64ASTGenStatement(ASTStatement* ast, FILE* f) {
    switch(ast->type) {
        case AST_STATEMENT_RETURN:
            x64ASTGenExpression(ast->as.return_, f);
            fprintf(f, "\tpop %%rax\n\tret\n\n");
    }
}

static void x64ASTGenBlockItem(ASTBlockItem* ast, FILE* f) {
    switch(ast->type) {
        case AST_BLOCK_ITEM_STATEMENT:
            x64ASTGenStatement(ast->as.statement, f);
    }
}

static void x64ASTGenCompoundStatement(ASTCompoundStatement* ast, FILE* f) {
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        x64ASTGenBlockItem(ast->items[i], f);
    }
}

static void x64ASTGenFunctionDefinition(ASTFunctionDefinition* ast, FILE* f) {
    fprintf(f, ".globl %.*s\n", ast->name.length, ast->name.start);
    fprintf(f, "%.*s:\n", ast->name.length, ast->name.start);
    x64ASTGenCompoundStatement(ast->statement, f);
}

static void x64ASTGenExternalDeclaration(ASTExternalDeclaration* ast, FILE* f) {
    switch(ast->type) {
        case AST_EXTERNAL_DECLARATION_FUNCTION_DEFINITION:
            x64ASTGenFunctionDefinition(ast->as.functionDefinition, f);
    }
}

static void x64ASTGenTranslationUnit(ASTTranslationUnit* ast, FILE* f) {
    for(unsigned int i = 0; i < ast->declarationCount; i++) {
        x64ASTGenExternalDeclaration(ast->declarations[i], f);
    }
}

void x64ASTGen(ASTTranslationUnit* ast) {
    x64ASTGenTranslationUnit(ast, fopen("a.s", "w"));
}