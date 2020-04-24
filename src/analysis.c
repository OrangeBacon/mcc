#include "analysis.h"
#include <stdlib.h>

typedef struct ctx {
    Parser* parser;
    bool inLoop;
} ctx;

// TODO - major saftey improvements - this allows far too much through
// treats c as an untyped language!

static ASTVariableType defaultInt = {
    .type = AST_VARIABLE_TYPE_INT,
};

static bool TypeEqual(ASTVariableType* a, ASTVariableType* b) {
    if(a->type != b->type) return false;

    switch(a->type) {
        case AST_VARIABLE_TYPE_INT:
            return true;
        case AST_VARIABLE_TYPE_POINTER:
            return TypeEqual(a->as.pointer, b->as.pointer);
        case AST_VARIABLE_TYPE_FUNCTION:
            if(a->as.function.paramCount != b->as.function.paramCount) {
                return false;
            }
            if(!TypeEqual(a->as.function.ret, b->as.function.ret)) {
                return false;
            }
            for(unsigned int i = 0; i < a->as.function.paramCount; i++) {
                if(!TypeEqual(a->as.function.params[i]->variableType, b->as.function.params[i]->variableType)) {
                    return false;
                }
            }
            return true;
    }

    // unreachable
    exit(1);
}

static void AnalyseExpression(ASTExpression* ast, ctx* ctx);

static void AnalyseAssignExpression(ASTExpression* ast, ctx* ctx) {
    ASTAssignExpression* assign = &ast->as.assign;
    if(!assign->target->isLvalue) {
        errorAt(ctx->parser, &assign->operator, "Operand must be an lvalue");
    }

    AnalyseExpression(assign->target, ctx);
    AnalyseExpression(assign->value, ctx);

    if(!TypeEqual(assign->target->exprType, assign->value->exprType)) {
        errorAt(ctx->parser, &assign->operator, "Cannot assign value to target of different type");
    }

    if(assign->operator.type != TOKEN_EQUAL_EQUAL && !TypeEqual(assign->value->exprType, &defaultInt)) {
        errorAt(ctx->parser, &assign->operator, "Cannot do arithmetic assignment with non arithmetic type");
    }

    ast->exprType = assign->target->exprType;
}

static void AnalyseBinaryExpression(ASTExpression* ast, ctx* ctx) {
    ASTBinaryExpression* bin = &ast->as.binary;
    AnalyseExpression(bin->left, ctx);
    AnalyseExpression(bin->right, ctx);

    // TODO - pointer arithmetic, integer conversions, ...
    if(!TypeEqual(bin->left->exprType, bin->right->exprType)) {
        errorAt(ctx->parser, &bin->operator, "Binary operator types must be equal");
    }

    if(!TypeEqual(bin->left->exprType, &defaultInt)) {
        errorAt(ctx->parser, &bin->operator, "Cannot use operator on non arithmetic type");
    }

    ast->exprType = bin->left->exprType;
}

static void AnalyseCallExpression(ASTExpression* ast, ctx* ctx) {
    // indirect call check
    //TODO - analyse functions, check multiple definition compatability,
    // possibly needs parser changes to record everything
    // check number/type of arguments
    // difference between int a() and int a(void)
    // get return value type
    // return type check

    ASTCallExpression* call = &ast->as.call;

    AnalyseExpression(call->target, ctx);
    for(unsigned int i = 0; i < call->paramCount; i++) {
        AnalyseExpression(call->params[i], ctx);
    }

    if(call->target->exprType->type != AST_VARIABLE_TYPE_FUNCTION) {
        errorAt(ctx->parser, &call->indirectErrorLoc, "Cannot call non function");
        return;
    }

    ast->exprType = call->target->exprType->as.function.ret;
}

static void AnalyseConstantExpression(ASTExpression* ast) {
    ASTConstantExpression* expr = &ast->as.constant;
    switch(expr->type) {
        case AST_CONSTANT_EXPRESSION_INTEGER:
            ast->exprType = &defaultInt;
            break;
        case AST_CONSTANT_EXPRESSION_LOCAL:
            ast->exprType = expr->local->type;
            break;
    }
}

static void AnalysePostfixExpression(ASTExpression* ast, ctx* ctx) {
    ASTPostfixExpression* post = &ast->as.postfix;
    if(!post->operand->isLvalue) {
        errorAt(ctx->parser, &post->operator, "Operand must be an lvalue");
    }

    AnalyseExpression(post->operand, ctx);

    // only postfix implemented are ++ and --
    if(!TypeEqual(post->operand->exprType, &defaultInt)) {
        errorAt(ctx->parser, &post->operator, "Cannot increment/decrement non arithmetic type");
    }

    ast->exprType = &defaultInt;
}

static void AnalyseTernaryExpression(ASTExpression* ast, ctx* ctx) {
    ASTTernaryExpression* op = &ast->as.ternary;

    AnalyseExpression(op->operand1, ctx);
    AnalyseExpression(op->operand2, ctx);
    AnalyseExpression(op->operand3, ctx);

    if(!TypeEqual(op->operand1->exprType, &defaultInt)) {
        errorAt(ctx->parser, &op->operator, "Condition must have scalar type");
    }

    if(!TypeEqual(op->operand2->exprType, op->operand3->exprType)) {
        errorAt(ctx->parser, &op->secondOperator, "condition values must have same type");
    }

    ast->exprType = op->operand2->exprType;
}

static void AnalyseUnaryExpression(ASTExpression* ast, ctx* ctx) {
    ASTUnaryExpression* unary = &ast->as.unary;

    AnalyseExpression(unary->operand, ctx);

    if(unary->operator.type == TOKEN_AND) {
        // elide &*var
        if(unary->operand->type == AST_EXPRESSION_UNARY &&
        unary->operand->as.unary.operator.type == TOKEN_STAR &&
        !unary->operand->as.unary.elide) {
            unary->elide = true;
            unary->operand->as.unary.elide = true;
        } else if(unary->operand->type != AST_EXPRESSION_CONSTANT ||
           unary->operand->as.constant.type != AST_CONSTANT_EXPRESSION_LOCAL) {
            // disallow &1, &(5+6), etc
            errorAt(ctx->parser, &unary->operator, "Cannot take address of not variable");
        }
    }

    // -a, ~a, &a, *a
    switch(unary->operator.type) {
        case TOKEN_NEGATE:
        case TOKEN_COMPLIMENT:
            if(!TypeEqual(unary->operand->exprType, &defaultInt)) {
                errorAt(ctx->parser, &unary->operator, "Cannot use operator on non arithmetic type");
            }
            ast->exprType = unary->operand->exprType;
            break;
        case TOKEN_AND: {
            ASTVariableType* addr = ArenaAlloc(sizeof(*addr));
            addr->type = AST_VARIABLE_TYPE_POINTER;
            addr->as.pointer = unary->operand->exprType;
            ast->exprType = addr;
        } break;
        case TOKEN_STAR:
            if(unary->operand->exprType->type != AST_VARIABLE_TYPE_POINTER) {
                errorAt(ctx->parser, &unary->operator, "Cannot dereference non pointer");
                return;
            }
            ast->exprType = unary->operand->exprType->as.pointer;
            break;
        default:
            printf("unreachable unary analysis");
            exit(0);
    }
}

static void AnalyseExpression(ASTExpression* ast, ctx* ctx) {
    if(ast == NULL) return;
    switch(ast->type) {
        case AST_EXPRESSION_ASSIGN:
            AnalyseAssignExpression(ast, ctx);
            break;
        case AST_EXPRESSION_BINARY:
            AnalyseBinaryExpression(ast, ctx);
            break;
        case AST_EXPRESSION_CALL:
            AnalyseCallExpression(ast, ctx);
            break;
        case AST_EXPRESSION_CONSTANT:
            AnalyseConstantExpression(ast);
            break;
        case AST_EXPRESSION_POSTFIX:
            AnalysePostfixExpression(ast, ctx);
            break;
        case AST_EXPRESSION_TERNARY:
            AnalyseTernaryExpression(ast, ctx);
            break;
        case AST_EXPRESSION_UNARY:
            AnalyseUnaryExpression(ast, ctx);
            break;
    }
}

static void AnalyseStatement(ASTStatement* ast, ctx* ctx);
static void AnalyseDeclaration(ASTDeclaration* ast, ctx* ctx);
static void AnalyseIterationStatement(ASTIterationStatement* ast, ctx* ctx) {
    bool oldLoop = ctx->inLoop;
    ctx->inLoop = true;
    AnalyseExpression(ast->control, ctx);
    AnalyseExpression(ast->post, ctx);
    AnalyseExpression(ast->preExpr, ctx);
    AnalyseDeclaration(ast->preDecl, ctx);
    AnalyseStatement(ast->body, ctx);
    ctx->inLoop = oldLoop;
}

static void AnalyseSelectionStatement(ASTSelectionStatement* ast, ctx* ctx) {
    AnalyseExpression(ast->condition, ctx);
    switch(ast->type) {
        case AST_SELECTION_STATEMENT_IF:
            AnalyseStatement(ast->block, ctx);
            break;
        case AST_SELECTION_STATEMENT_IFELSE:
            AnalyseStatement(ast->block, ctx);
            AnalyseStatement(ast->elseBlock, ctx);
            break;
    }
}

static void AnalyseBlockItem(ASTBlockItem* ast, ctx* ctx);
static void AnalyseCompoundStatement(ASTCompoundStatement* ast, ctx* ctx) {
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        AnalyseBlockItem(ast->items[i], ctx);
    }
}

static void AnalyseJumpStatement(ASTJumpStatement* ast, ctx* ctx) {
    switch(ast->type) {
        case AST_JUMP_STATEMENT_CONTINUE:
        case AST_JUMP_STATEMENT_BREAK:
            if(!ctx->inLoop) {
                errorAt(ctx->parser, &ast->statement, "Cannot break or continue"
                    " outside of a loop");
            }
            break;
        case AST_JUMP_STATEMENT_RETURN:
            AnalyseExpression(ast->expr, ctx);
            break;
    }
}

static void AnalyseStatement(ASTStatement* ast, ctx* ctx) {
    switch(ast->type) {
        case AST_STATEMENT_ITERATION:
            AnalyseIterationStatement(ast->as.iteration, ctx);
            break;
        case AST_STATEMENT_SELECTION:
            AnalyseSelectionStatement(ast->as.selection, ctx);
            break;
        case AST_STATEMENT_COMPOUND:
            AnalyseCompoundStatement(ast->as.compound, ctx);
            break;
        case AST_STATEMENT_JUMP:
            AnalyseJumpStatement(ast->as.jump, ctx);
            break;
        case AST_STATEMENT_EXPRESSION:
            AnalyseExpression(ast->as.expression, ctx);
        case AST_STATEMENT_NULL:
            break;
    }
}

static void AnalyseFnCompoundStatement(ASTFnCompoundStatement* ast, ctx* ctx);

static void AnalyseDeclaration(ASTDeclaration* ast, ctx* ctx) {
    if(ast == NULL) return;
    for(unsigned int i = 0; i < ast->declarators.declaratorCount; i++) {
        if(ast->declarators.declarators[i]->type == AST_INIT_DECLARATOR_FUNCTION) {
            AnalyseFnCompoundStatement(ast->declarators.declarators[i]->fn, ctx);
            continue;
        }
        ASTInitDeclarator* decl = ast->declarators.declarators[i];
        AnalyseExpression(decl->initializer, ctx);
    }
}

static void AnalyseBlockItem(ASTBlockItem* ast, ctx* ctx) {
    switch(ast->type) {
        case AST_BLOCK_ITEM_STATEMENT:
            AnalyseStatement(ast->as.statement, ctx);
            break;
        case AST_BLOCK_ITEM_DECLARATION:
            AnalyseDeclaration(ast->as.declaration, ctx);
            break;
    }
}

static void AnalyseFnCompoundStatement(ASTFnCompoundStatement* ast, ctx* ctx) {
    if(ast == NULL) return;
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        AnalyseBlockItem(ast->items[i], ctx);
    }
}

static void AnalyseTranslationUnit(ASTTranslationUnit* ast, ctx* ctx) {
    for(unsigned int i = 0; i < ast->declarationCount; i++) {
        AnalyseDeclaration(ast->declarations[i], ctx);
    }
}

void Analyse(Parser* parser) {
    ctx ctx = {
        .parser = parser,
        .inLoop = false,
    };
    AnalyseTranslationUnit(parser->ast, &ctx);
}