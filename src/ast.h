#ifndef AST_H
#define AST_H

#include "token.h"

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
        ASTExpression* ret;
    } as;
} ASTStatement;

typedef struct ASTFunctionDefinition {
    Token name;
    unsigned int statementCount;
    unsigned int statementCapacity;
    ASTStatement** statements;
} ASTFunctionDefinition;

typedef enum ASTExternalDeclarationType {
    AST_EXTERNAL_DECLARATION_FUNCTION_DEFINITION,
    AST_EXTERNAL_DECLARATION_DECLARATION
} ASTExternalDeclarationType;

typedef struct ASTExternalDeclaration {
    ASTExternalDeclarationType type;

    union {
        ASTFunctionDefinition* functionDefinition;
    } as;
} ASTExternalDeclaration;

typedef struct ASTTranslationUnit {
    unsigned int declarationCount;
    unsigned int declarationCapacity;
    ASTExternalDeclaration** declarations;
} ASTTranslationUnit;

void ASTPrint(ASTTranslationUnit* ast);

#endif