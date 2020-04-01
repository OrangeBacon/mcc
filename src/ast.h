#ifndef AST_H
#define AST_H

#include "token.h"
#include "memory.h"
#include "symbolTable.h"

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
    int stackOffset;
} ASTUnaryExpression;

typedef struct ASTPostfixExpression {
    Token operator;
    struct ASTExpression* operand;
    int stackOffset;
} ASTPostfixExpression;

#define FOREACH_CONSTANTEXPRESSION(x, ns) \
    x(ns, INTEGER) x(ns, VARIABLE)
typedef enum ASTConstantExpressionType {
    FOREACH_CONSTANTEXPRESSION(ASTENUM, AST_CONSTANT_EXPRESSION)
} ASTConstantExpressionType;

typedef struct ASTConstantExpression {
    ASTConstantExpressionType type;
    Token tok;
    int stackDepth;
} ASTConstantExpression;

typedef struct ASTAssignExpression {
    int stackOffset;
    struct ASTExpression* value;
    Token operator;
} ASTAssignExpression;

#define FOREACH_EXPRESSION(x, ns) \
    x(ns, BINARY) x(ns, TERNARY) x(ns, UNARY) \
    x(ns, POSTFIX) x(ns, CONSTANT) x(ns, ASSIGN)
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
        ASTAssignExpression assign;
    } as;
} ASTExpression;

struct ASTStatement;

#define FOREACH_SELECTIONSTATEMENT(x, ns) \
    x(ns, IF) x(ns, IFELSE)
typedef enum ASTSelectionStatementType {
    FOREACH_SELECTIONSTATEMENT(ASTENUM, AST_SELECTION_STATEMENT)
} ASTSelectionStatementType;

typedef struct ASTSelectionStatement {
    ASTSelectionStatementType type;
    ASTExpression* condition;
    struct ASTStatement* block;
    struct ASTStatement* elseBlock;
} ASTSelectionStatement;

#define FOREACH_ITERATIONSTATEMENT(x, ns) \
    x(ns, FOR_DECL) x(ns, FOR_EXPR) \
    x(ns, WHILE) x(ns, DO)
typedef enum ASTIterationStatementType {
    FOREACH_ITERATIONSTATEMENT(ASTENUM, AST_ITERATION_STATEMENT)
} ASTIterationStatementType;

typedef struct ASTIterationStatement {
    ASTIterationStatementType type;

    ASTExpression* control;
    ASTExpression* preExpr;
    struct ASTDeclaration* preDecl;
    ASTExpression* post;
    struct ASTStatement* body;
    SymbolExitList* freeCount;
} ASTIterationStatement;

#define FOREACH_JUMPSTATEMENT(x, ns) \
    x(ns, RETURN) x(ns, CONTINUE) x(ns, BREAK)
typedef enum ASTJumpStatementType {
    FOREACH_JUMPSTATEMENT(ASTENUM, AST_JUMP_STATEMENT)
} ASTJumpStatementType;

typedef struct ASTJumpStatement {
    ASTJumpStatementType type;
    Token statement;
    ASTExpression* expr;
} ASTJumpStatement;

#define FOREACH_STATEMENT(x, ns) \
    x(ns, EXPRESSION) x(ns, SELECTION) x(ns, JUMP) \
    x(ns, COMPOUND) x(ns, ITERATION) x(ns, NULL)
typedef enum ASTStatementType {
    FOREACH_STATEMENT(ASTENUM, AST_STATEMENT)
} ASTStatementType;

typedef struct ASTStatement {
    ASTStatementType type;

    union {
        ASTExpression* expression;
        ASTSelectionStatement* selection;
        ASTIterationStatement* iteration;
        struct ASTCompoundStatement* compound;
        ASTJumpStatement* jump;
    } as;
} ASTStatement;

#define FOREACH_INITDECLARATOR(x, ns) \
    x(ns, INITIALIZE) x(ns, NO_INITIALIZE)
typedef enum ASTInitDeclaratorType {
    FOREACH_INITDECLARATOR(ASTENUM, AST_INIT_DECLARATOR)
} ASTInitDeclaratorType;

typedef struct ASTInitDeclarator {
    ASTInitDeclaratorType type;

    Token declarator;
    ASTExpression* initializer;
} ASTInitDeclarator;

ASTARRAY(InitDeclaratorList, InitDeclarator, declarator);

typedef struct ASTDeclaration {
    ASTInitDeclaratorList declarators;
} ASTDeclaration;

#define FOREACH_BLOCKITEM(x, ns) \
    x(ns, STATEMENT) x(ns, DECLARATION)
typedef enum ASTBlockItemType {
    FOREACH_BLOCKITEM(ASTENUM, AST_BLOCK_ITEM)
} ASTBlockItemType;

typedef struct ASTBlockItem {
    ASTBlockItemType type;

    union {
        struct ASTStatement* statement;
        ASTDeclaration* declaration;
    } as;
} ASTBlockItem;

ASTARRAY(FnCompoundStatement, BlockItem, item);

typedef struct ASTCompoundStatement {
    ARRAY_DEFINE(ASTBlockItem*, item);
    SymbolExitList* popCount;
} ASTCompoundStatement;

typedef struct ASTFunctionDefinition {
    Token name;
    ASTFnCompoundStatement* statement;
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