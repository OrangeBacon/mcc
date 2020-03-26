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
static char* ASTConstantExpressionTypeNames[] = {
    FOREACH_CONSTANTEXPRESSION(ASTSTRARRAY, 0)
};

static void ASTExpressionPrint(ASTExpression* ast, int depth) {
    PrintTabs(depth);
    printf("ASTExpression %s:\n", ASTExpressionTypeNames[ast->type]);

    switch(ast->type) {
        case AST_EXPRESSION_CONSTANT:
            PrintTabs(depth + 1);
            printf("%s: ", ASTConstantExpressionTypeNames[ast->as.constant.type]);
            TokenPrint(&ast->as.constant.tok);
            printf("\n");
            if(ast->as.constant.type == AST_CONSTANT_EXPRESSION_VARIABLE) {
                PrintTabs(depth + 2);
                printf("stack offset: %d\n", ast->as.constant.stackDepth);
            }
            break;
        case AST_EXPRESSION_TERNARY:
            PrintTabs(depth + 1);
            printf("Operator1: ");
            TokenPrint(&ast->as.ternary.operator);
            printf("\n");
            PrintTabs(depth + 1);
            printf("Operator2: ");
            TokenPrint(&ast->as.ternary.secondOperator);
            printf("\n");
            ASTExpressionPrint(ast->as.ternary.operand1, depth + 1);
            ASTExpressionPrint(ast->as.ternary.operand2, depth + 1);
            ASTExpressionPrint(ast->as.ternary.operand3, depth + 1);
            break;
        case AST_EXPRESSION_BINARY:
            PrintTabs(depth + 1);
            printf("Operator: ");
            TokenPrint(&ast->as.binary.operator);
            printf("\n");
            ASTExpressionPrint(ast->as.binary.left, depth + 1);
            ASTExpressionPrint(ast->as.binary.right, depth + 1);
            break;
        case AST_EXPRESSION_POSTFIX:
            PrintTabs(depth + 1);
            printf("Operator: ");
            TokenPrint(&ast->as.postfix.operator);
            printf("\n");
            ASTExpressionPrint(ast->as.postfix.operand, depth + 1);
            break;
        case AST_EXPRESSION_UNARY:
            PrintTabs(depth + 1);
            printf("Operator: ");
            TokenPrint(&ast->as.postfix.operator);
            printf("\n");
            ASTExpressionPrint(ast->as.postfix.operand, depth + 1);
            break;
        case AST_EXPRESSION_ASSIGN:
            PrintTabs(depth + 1);
            printf("Target: %d\n", ast->as.assign.stackOffset);
            ASTExpressionPrint(ast->as.assign.value, depth + 1);
            break;
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
            break;
        case AST_STATEMENT_EXPRESSION:
            ASTExpressionPrint(ast->as.expression, depth + 1);
            break;
    }
}

static char* ASTInitDeclaratorTypeNames[] = {
    FOREACH_INITDECLARATOR(ASTSTRARRAY, 0)
};

static void ASTInitDeclaratorPrint(ASTInitDeclarator* ast, int depth) {
    PrintTabs(depth);
    printf("ASTInitDeclarator %s:\n", ASTInitDeclaratorTypeNames[ast->type]);
    PrintTabs(depth + 1);
    printf("Identifier: ");
    TokenPrint(&ast->declarator);
    printf("\n");

    if(ast->type == AST_INIT_DECLARATOR_INITIALIZE) {
        ASTExpressionPrint(ast->initializer, depth + 1);
    }
}

ASTARRAY_PRINT(InitDeclaratorList, InitDeclarator, declarator)

static void ASTDeclarationPrint(ASTDeclaration* ast, int depth) {
    PrintTabs(depth);
    printf("ASTDeclaration:\n");
    ASTInitDeclaratorListPrint(&ast->declarators, depth + 1);
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
            break;
        case AST_BLOCK_ITEM_DECLARATION:
            ASTDeclarationPrint(ast->as.declaration, depth + 1);
            break;
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
    printf("\n");
}