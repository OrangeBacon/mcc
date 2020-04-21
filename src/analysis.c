#include "analysis.h"

typedef struct ctx {
    Parser* parser;
    bool inLoop;
} ctx;

// TODO - majoor saftey improvements - this allows far too much through
// treats c as an untyped language!

static void AnalyseExpression(ASTExpression* ast, ctx* ctx);

static void AnalyseAssignExpression(ASTAssignExpression* ast, ctx* ctx) {
    if(!ast->target->isLvalue) {
        errorAt(ctx->parser, &ast->operator, "Operand must be an lvalue");
    }
    AnalyseExpression(ast->target, ctx);
    AnalyseExpression(ast->value, ctx);
}

static void AnalyseCallExpression(ASTCallExpression* ast, ctx* ctx) {
    // indirect call check
    (void)ast;
    (void)ctx;
}

static void AnalysePostfixExpression(ASTPostfixExpression* ast, ctx* ctx) {
    if(!ast->operand->isLvalue) {
        errorAt(ctx->parser, &ast->operator, "Operand must be an lvalue");
    }

    AnalyseExpression(ast->operand, ctx);
}

static void AnalyseUnaryExpression(ASTUnaryExpression* ast, ctx* ctx) {
    AnalyseExpression(ast->operand, ctx);
    if(ast->operator.type == TOKEN_AND) {
        if(ast->operand->type == AST_EXPRESSION_UNARY &&
        ast->operand->as.unary.operator.type == TOKEN_STAR &&
        !ast->operand->as.unary.elide) {
            ast->elide = true;
            ast->operand->as.unary.elide = true;
            return;
        }

        if(ast->operand->type != AST_EXPRESSION_CONSTANT ||
           ast->operand->as.constant.type != AST_CONSTANT_EXPRESSION_LOCAL) {
            errorAt(ctx->parser, &ast->operator, "Cannot take address of not variable");
        }
    }
}

static void AnalyseExpression(ASTExpression* ast, ctx* ctx) {
    if(ast == NULL) return;
    switch(ast->type) {
        case AST_EXPRESSION_ASSIGN:
            AnalyseAssignExpression(&ast->as.assign, ctx);
            break;
        case AST_EXPRESSION_BINARY:
            AnalyseExpression(ast->as.binary.left, ctx);
            AnalyseExpression(ast->as.binary.right, ctx);
            break;
        case AST_EXPRESSION_CALL:
            AnalyseCallExpression(&ast->as.call, ctx);
            break;
        case AST_EXPRESSION_CONSTANT:
            break;
        case AST_EXPRESSION_POSTFIX:
            AnalysePostfixExpression(&ast->as.postfix, ctx);
            break;
        case AST_EXPRESSION_TERNARY:
            AnalyseExpression(ast->as.ternary.operand1, ctx);
            AnalyseExpression(ast->as.ternary.operand2, ctx);
            AnalyseExpression(ast->as.ternary.operand3, ctx);
            break;
        case AST_EXPRESSION_UNARY:
            AnalyseUnaryExpression(&ast->as.unary, ctx);
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