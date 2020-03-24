#ifndef AST_H
#define AST_H

#include "token.h"
#include "memory.h"

#define ASTARRAY(name, type, arr) \
    typedef struct AST##name { \
        ARRAY_DEFINE(AST##type*, arr);\
    } AST##name

#define ASTENUM(ns, name) ns##_##name,

struct ASTExpression;

typedef struct ASTBinaryExpression {
    Token operator;
    struct ASTExpression* left;
    struct ASTExpression* right;
} ASTBinaryExpression;

typedef struct ASTTernaryExpression {
    Token operator;
    Token secondOperator;
    struct ASTExpression* operand1;
    struct ASTExpression* operand2;
    struct ASTExpression* operand3;
} ASTTernaryExpression;

typedef struct ASTUnaryExpression {
    Token operator;
    struct ASTExpression* operand;
} ASTUnaryExpression;

typedef struct ASTPostfixExpression {
    Token operator;
    struct ASTExpression* operand;
} ASTPostfixExpression;

typedef struct ASTConstantExpression {
    Token integer;
} ASTConstantExpression;


#define FOREACH_EXPRESSION(x, ns) \
    x(ns, BINARY) x(ns, TERNARY) x(ns, UNARY) \
    x(ns, POSTFIX) x(ns, CONSTANT)
typedef enum ASTExpressionType {
    FOREACH_EXPRESSION(ASTENUM, AST_EXPRESSION)
} ASTExpressionType;

typedef struct ASTExpression {
    ASTExpressionType type;

    union {
        ASTBinaryExpression binary;
        ASTTernaryExpression ternary;
        ASTUnaryExpression unary;
        ASTPostfixExpression postfix;
        ASTConstantExpression constant;
    } as;
} ASTExpression;

#define FOREACH_STATEMENT(x, ns) \
    x(ns, RETURN)
typedef enum ASTStatementType {
    FOREACH_STATEMENT(ASTENUM, AST_STATEMENT)
} ASTStatementType;

typedef struct ASTStatement {
    ASTStatementType type;

    union {
        ASTExpression* return_;
    } as;
} ASTStatement;

#define FOREACH_BLOCKITEM(x, ns) \
    x(ns, STATEMENT)
typedef enum ASTBlockItemType {
    FOREACH_BLOCKITEM(ASTENUM, AST_BLOCK_ITEM)
} ASTBlockItemType;

typedef struct ASTBlockItem {
    ASTBlockItemType type;

    union {
        ASTStatement* statement;
    } as;
} ASTBlockItem;

ASTARRAY(CompoundStatement, BlockItem, item);

typedef struct ASTFunctionDefinition {
    Token name;
    ASTCompoundStatement* statement;
} ASTFunctionDefinition;

#define FOREACH_EXTERNALDECLARATION(x, ns) \
    x(ns, FUNCTION_DEFINITION)
typedef enum ASTExternalDeclarationType {
    FOREACH_EXTERNALDECLARATION(ASTENUM, AST_EXTERNAL_DECLARATION)
} ASTExternalDeclarationType;

typedef struct ASTExternalDeclaration {
    ASTExternalDeclarationType type;

    union {
        ASTFunctionDefinition* functionDefinition;
    } as;
} ASTExternalDeclaration;

ASTARRAY(TranslationUnit, ExternalDeclaration, declaration);

void ASTPrint(ASTTranslationUnit* ast);

#endif