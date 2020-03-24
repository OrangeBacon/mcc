#ifndef AST_H
#define AST_H

#include "token.h"
#include "memory.h"

#define ASTARRAY(name, type, arr) \
    typedef struct AST##name { \
        ARRAY_DEFINE(AST##type*, arr);\
    } AST##name

#define ASTENUM(ns, name) ns##_##name,

#define FOREACH_EXPRESSION(x, ns) \
    x(ns, INTEGER)
typedef enum ASTExpressionType {
    FOREACH_EXPRESSION(ASTENUM, AST_EXPRESSION)
} ASTExpressionType;

typedef struct ASTExpression {
    ASTExpressionType type;

    union {
        Token integer;
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