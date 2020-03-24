#ifndef AST_H
#define AST_H

#include "token.h"
#include "memory.h"

typedef enum ASTExpressionType {
    AST_EXPRESSION_INTEGER
} ASTExpressionType;

typedef struct ASTExpression {
    ASTExpressionType type;

    union {
        unsigned int integer;
    } as;
} ASTExpression;

typedef enum ASTStatementType {
    AST_STATEMENT_RETURN
} ASTStatementType;

typedef struct ASTStatement {
    ASTStatementType type;

    union {
        ASTExpression* return_;
    } as;
} ASTStatement;

typedef enum ASTBlockItemType {
    AST_BLOCK_ITEM_DECLARATION,
    AST_BLOCK_ITEM_STATEMENT,
} ASTBlockItemType;

typedef struct ASTBlockItem {
    ASTBlockItemType type;

    union {
        ASTStatement* statement;
    } as;
} ASTBlockItem;

typedef struct ASTCompoundStatement {
    ARRAY_DEFINE(ASTBlockItem*, item);
} ASTCompoundStatement;

typedef struct ASTFunctionDefinition {
    Token name;
    ASTCompoundStatement* statement;
} ASTFunctionDefinition;

typedef enum ASTExternalDeclarationType {
    AST_EXTERNAL_DECLARATION_FUNCTION_DEFINITION,
    AST_EXTERNAL_DECLARATION_DECLARATION,
} ASTExternalDeclarationType;

typedef struct ASTExternalDeclaration {
    ASTExternalDeclarationType type;

    union {
        ASTFunctionDefinition* functionDefinition;
    } as;
} ASTExternalDeclaration;

typedef struct ASTTranslationUnit {
    ARRAY_DEFINE(ASTExternalDeclaration*, declaration);
} ASTTranslationUnit;

void ASTPrint(ASTTranslationUnit* ast);

#endif