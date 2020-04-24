#include "ast.h"

#include <stdio.h>

#define ASTARRAY_PRINT(name, type, arr) \
    static void AST##name##Print(AST##name* ast, int depth) { \
        if(ast == NULL) return; \
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

static void ASTVariableTypePrint(ASTVariableType* ast) {
    switch(ast->type) {
        case AST_VARIABLE_TYPE_POINTER:
            printf("pointer to ");
            ASTVariableTypePrint(ast->as.pointer);
            break;
        case AST_VARIABLE_TYPE_INT:
            printf("int");
            break;
        case AST_VARIABLE_TYPE_FUNCTION:
            printf("function (");
            for(unsigned int i = 0; i < ast->as.function.paramCount; i++) {
                ASTDeclarator* decl = ast->as.function.params[i];
                ASTVariableTypePrint(decl->variableType);
                printf(" '%.*s'", decl->declarator->length, decl->declarator->name);
                if(i != ast->as.function.paramCount - 1) {
                    printf(", ");
                }
            }
            printf(") returning ");
            ASTVariableTypePrint(ast->as.function.ret);
    }
}

static char* ASTExpressionTypeNames[] = {
    FOREACH_EXPRESSION(ASTSTRARRAY, 0)
};
static char* ASTConstantExpressionTypeNames[] = {
    FOREACH_CONSTANTEXPRESSION(ASTSTRARRAY, 0)
};

static void ASTExpressionPrint(ASTExpression* ast, int depth) {
    PrintTabs(depth);
    if(ast == NULL) {
        printf("ASTExpression NULL\n");
        return;
    }

    printf("ASTExpression %s:\n", ASTExpressionTypeNames[ast->type]);
    PrintTabs(depth + 1);
    printf("type: ");
    ASTVariableTypePrint(ast->exprType);
    printf("\n");

    switch(ast->type) {
        case AST_EXPRESSION_CONSTANT:
            PrintTabs(depth + 1);
            printf("%s: ", ASTConstantExpressionTypeNames[ast->as.constant.type]);
            TokenPrint(&ast->as.constant.tok);
            printf("\n");
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
            printf("Elided: %s\n", ast->as.unary.elide ? "true" : "false");
            PrintTabs(depth + 1);
            printf("Operator: ");
            TokenPrint(&ast->as.postfix.operator);
            printf("\n");
            ASTExpressionPrint(ast->as.postfix.operand, depth + 1);
            break;
        case AST_EXPRESSION_ASSIGN:
            ASTExpressionPrint(ast->as.assign.target, depth + 1);
            PrintTabs(depth + 1);
            printf("Operator: ");
            TokenPrint(&ast->as.assign.operator);
            printf("\n");
            ASTExpressionPrint(ast->as.assign.value, depth + 1);
            break;
        case AST_EXPRESSION_CALL:
            if(ast->as.call.paramCount == 0) {
                PrintTabs(depth + 1);
                printf("No parameters\n");
            }
            for(unsigned int i = 0; i < ast->as.call.paramCount; i++) {
                ASTExpressionPrint(ast->as.call.params[i], depth + 1);
            }
    }
}

static void ASTStatementPrint(ASTStatement* ast, int depth);

static char* ASTSelectionStatementTypeNames[] = {
    FOREACH_SELECTIONSTATEMENT(ASTSTRARRAY, 0)
};

static void ASTSelectionStatementPrint(ASTSelectionStatement* ast, int depth) {
    PrintTabs(depth);
    printf("ASTSelectionStatement %s:\n", ASTSelectionStatementTypeNames[ast->type]);

    switch(ast->type) {
        case AST_SELECTION_STATEMENT_IF:
            ASTExpressionPrint(ast->condition, depth + 1);
            ASTStatementPrint(ast->block, depth + 1);
            break;
        case AST_SELECTION_STATEMENT_IFELSE:
            ASTExpressionPrint(ast->condition, depth + 1);
            ASTStatementPrint(ast->block, depth + 1);
            ASTStatementPrint(ast->elseBlock, depth + 1);
            break;
    }
}

static void ASTDeclaratorPrint(ASTDeclarator* ast, int depth) {
    PrintTabs(depth);
    printf("ASTDeclarator:\n");
    PrintTabs(depth + 1);
    printf("Identifier: %.*s\n", ast->declarator->length, ast->declarator->name);
    PrintTabs(depth + 1);
    printf("Type: ");
    ASTVariableTypePrint(ast->variableType);
    printf("\n");
}

static void ASTBlockItemPrint(ASTBlockItem* ast, int depth);
ASTARRAY_PRINT(FnCompoundStatement, BlockItem, item)

static char* ASTInitDeclaratorTypeNames[] = {
    FOREACH_INITDECLARATOR(ASTSTRARRAY, 0)
};

static void ASTInitDeclaratorPrint(ASTInitDeclarator* ast, int depth) {
    PrintTabs(depth);
    printf("ASTInitDeclarator %s:\n", ASTInitDeclaratorTypeNames[ast->type]);
    ASTDeclaratorPrint(ast->declarator, depth + 1);

    if(ast->type == AST_INIT_DECLARATOR_INITIALIZE) {
        ASTExpressionPrint(ast->initializer, depth + 1);
    } else if(ast->type == AST_INIT_DECLARATOR_FUNCTION) {
        ASTFnCompoundStatementPrint(ast->fn, depth + 1);
    }
}

ASTARRAY_PRINT(InitDeclaratorList, InitDeclarator, declarator)

static void ASTDeclarationPrint(ASTDeclaration* ast, int depth) {
    PrintTabs(depth);
    printf("ASTDeclaration:\n");
    ASTInitDeclaratorListPrint(&ast->declarators, depth + 1);
}

static char* ASTIterationStatementTypeNames[] = {
    FOREACH_ITERATIONSTATEMENT(ASTSTRARRAY, 0)
};

static void ASTIterationStatementPrint(ASTIterationStatement* ast, int depth) {
    PrintTabs(depth);
    printf("ASTIterationStatement %s:\n", ASTIterationStatementTypeNames[ast->type]);

    switch(ast->type) {
        case AST_ITERATION_STATEMENT_DO:
            PrintTabs(depth + 1);
            printf("body: \n");
            ASTStatementPrint(ast->body, depth + 1);
            PrintTabs(depth + 1);
            printf("expression: \n");
            ASTExpressionPrint(ast->control, depth + 1);
            break;
        case AST_ITERATION_STATEMENT_FOR_DECL:
            PrintTabs(depth + 1);
            printf("decl: \n");
            ASTDeclarationPrint(ast->preDecl, depth + 1);
            PrintTabs(depth + 1);
            printf("freeCount: %d\n", ast->freeCount->localCount);
            PrintTabs(depth + 1);
            printf("control: \n");
            ASTExpressionPrint(ast->control, depth + 1);
            PrintTabs(depth + 1);
            printf("post: \n");
            ASTExpressionPrint(ast->post, depth + 1);
            PrintTabs(depth + 1);
            printf("body: \n");
            ASTStatementPrint(ast->body, depth + 1);
            break;
        case AST_ITERATION_STATEMENT_FOR_EXPR:
            PrintTabs(depth + 1);
            printf("start: \n");
            ASTExpressionPrint(ast->preExpr, depth + 1);
            PrintTabs(depth + 1);
            printf("control: \n");
            ASTExpressionPrint(ast->control, depth + 1);
            PrintTabs(depth + 1);
            printf("post: \n");
            ASTExpressionPrint(ast->post, depth + 1);
            PrintTabs(depth + 1);
            printf("body: \n");
            ASTStatementPrint(ast->body, depth + 1);
            break;
        case AST_ITERATION_STATEMENT_WHILE:
            PrintTabs(depth + 1);
            printf("expression: \n");
            ASTExpressionPrint(ast->control, depth + 1);
            PrintTabs(depth + 1);
            printf("body: \n");
            ASTStatementPrint(ast->body, depth + 1);
            break;
    }
}

static char* ASTJumpStatementTypeNames[] = {
    FOREACH_JUMPSTATEMENT(ASTSTRARRAY, 0)
};

static void ASTJumpStatementPrint(ASTJumpStatement* ast, int depth) {
    PrintTabs(depth);
    printf("ASTJumpStatement %s:\n", ASTJumpStatementTypeNames[ast->type]);

    if(ast->type == AST_JUMP_STATEMENT_RETURN) {
        ASTExpressionPrint(ast->expr, depth + 1);
    }
}

static void ASTCompoundStatementPrint(ASTCompoundStatement* ast, int depth);

static char* ASTStatementTypeNames[] = {
    FOREACH_STATEMENT(ASTSTRARRAY, 0)
};

static void ASTStatementPrint(ASTStatement* ast, int depth) {
    PrintTabs(depth);
    printf("ASTStatement %s:\n", ASTStatementTypeNames[ast->type]);

    switch(ast->type) {
        case AST_STATEMENT_JUMP:
            ASTJumpStatementPrint(ast->as.jump, depth + 1);
            break;
        case AST_STATEMENT_EXPRESSION:
            ASTExpressionPrint(ast->as.expression, depth + 1);
            break;
        case AST_STATEMENT_SELECTION:
            ASTSelectionStatementPrint(ast->as.selection, depth + 1);
            break;
        case AST_STATEMENT_COMPOUND:
            ASTCompoundStatementPrint(ast->as.compound, depth + 1);
            break;
        case AST_STATEMENT_ITERATION:
            ASTIterationStatementPrint(ast->as.iteration, depth + 1);
            break;
        case AST_STATEMENT_NULL:
            break;
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
            break;
        case AST_BLOCK_ITEM_DECLARATION:
            ASTDeclarationPrint(ast->as.declaration, depth + 1);
            break;
    }
}

ASTARRAY_PRINT(CompoundStatement, BlockItem, item)
ASTARRAY_PRINT(TranslationUnit, Declaration, declaration)

void ASTPrint(ASTTranslationUnit* ast) {
    ASTTranslationUnitPrint(ast, 0);
    printf("\n");
}