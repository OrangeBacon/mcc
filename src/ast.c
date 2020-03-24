#include "ast.h"

#include <stdio.h>

#define ASTARRAY_PRINT(name, type, arr) \
    static void AST##name##Print(AST##name* ast, int depth) { \
        PrintTabs(depth); \
        printf("AST"#name": size = %u\n", ast->arr##Count); \
        for(unsigned int i = 0; i < ast->arr##Count; i++) { \
            AST##type##Print(ast->arr##s[i], depth + 1); \
        } \
    }

#define ASTSTRARRAY(ns, name) #name,

static void PrintTabs(int depth) {
    for(int i = 0; i < depth; i++) putchar('\t');
}

static char* ASTExpressionTypeNames[] = {
    FOREACH_EXPRESSION(ASTSTRARRAY, 0)
};

static void ASTExpressionPrint(ASTExpression* ast, int depth) {
    PrintTabs(depth);
    printf("ASTExpression %s:\n", ASTExpressionTypeNames[ast->type]);

    switch(ast->type) {
        case AST_EXPRESSION_INTEGER:
            PrintTabs(depth + 1);
            TokenPrint(&ast->as.integer);
    }
}

static char* ASTStatementTypeNames[] = {
    FOREACH_STATEMENT(ASTSTRARRAY, 0)
};

static void ASTStatementPrint(ASTStatement* ast, int depth) {
    PrintTabs(depth);
    printf("ASTStatement %s:\n", ASTStatementTypeNames[ast->type]);

    switch(ast->type) {
        case AST_STATEMENT_RETURN:
            ASTExpressionPrint(ast->as.return_, depth + 1);
    }
}

static char* ASTBlockItemTypeNames[] = {
    FOREACH_BLOCKITEM(ASTSTRARRAY, 0)
};

static void ASTBlockItemPrint(ASTBlockItem* ast, int depth) {
    PrintTabs(depth);
    printf("ASTBlockItem %s:\n", ASTBlockItemTypeNames[ast->type]);

    switch(ast->type) {
        case AST_BLOCK_ITEM_STATEMENT:
            ASTStatementPrint(ast->as.statement, depth + 1);
    }
}

ASTARRAY_PRINT(CompoundStatement, BlockItem, item)

static void ASTFunctionDefinitionPrint(ASTFunctionDefinition* ast, int depth) {
    PrintTabs(depth);
    printf("ASTFunctionDefinition ");
    TokenPrint(&ast->name);
    printf("\n");
    ASTCompoundStatementPrint(ast->statement, depth + 1);
}

static char* ASTExternalDeclarationTypeNames[] = {
    FOREACH_BLOCKITEM(ASTSTRARRAY, 0)
};

static void ASTExternalDeclarationPrint(ASTExternalDeclaration* ast, int depth) {
    PrintTabs(depth);
    printf("ASTExternalDeclaration %s:\n", ASTExternalDeclarationTypeNames[ast->type]);

    switch(ast->type) {
        case AST_EXTERNAL_DECLARATION_FUNCTION_DEFINITION:
            ASTFunctionDefinitionPrint(ast->as.functionDefinition, depth + 1);
    }
}

ASTARRAY_PRINT(TranslationUnit, ExternalDeclaration, declaration)

void ASTPrint(ASTTranslationUnit* ast) {
    ASTTranslationUnitPrint(ast, 0);
}