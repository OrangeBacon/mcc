#include "analysis.h"

typedef struct ctx {
    Parser* parser;
    bool inLoop;
} ctx;

static void AnalyseStatement(ASTStatement* ast, ctx* ctx);
static void AnalyseIterationStatement(ASTIterationStatement* ast, ctx* ctx) {
    bool oldLoop = ctx->inLoop;
    ctx->inLoop = true;
    AnalyseStatement(ast->body, ctx);
    ctx->inLoop = oldLoop;
}

static void AnalyseSelectionStatement(ASTSelectionStatement* ast, ctx* ctx) {
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
        case AST_JUMP_STATEMENT_RETURN:
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
        case AST_STATEMENT_NULL:
            break;
    }
}

static void AnalyseBlockItem(ASTBlockItem* ast, ctx* ctx) {
    switch(ast->type) {
        case AST_BLOCK_ITEM_STATEMENT:
            AnalyseStatement(ast->as.statement, ctx);
            break;
        case AST_BLOCK_ITEM_DECLARATION:
            break;
    }
}

static void AnalyseFnCompoundStatement(ASTFnCompoundStatement* ast, ctx* ctx) {
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        AnalyseBlockItem(ast->items[i], ctx);
    }
}

static void AnalyseFunctionDefinition(ASTFunctionDefinition* ast, ctx* ctx) {
    AnalyseFnCompoundStatement(ast->statement, ctx);
}

static void AnalyseExternalDeclaration(ASTExternalDeclaration* ast, ctx* ctx) {
    switch(ast->type) {
        case AST_EXTERNAL_DECLARATION_FUNCTION_DEFINITION:
            AnalyseFunctionDefinition(ast->as.functionDefinition, ctx);
    }
}

static void AnalyseTranslationUnit(ASTTranslationUnit* ast, ctx* ctx) {
    for(unsigned int i = 0; i < ast->declarationCount; i++) {
        AnalyseExternalDeclaration(ast->declarations[i], ctx);
    }
}

void Analyse(Parser* parser) {
    ctx ctx = {
        .parser = parser,
        .inLoop = false,
    };
    AnalyseTranslationUnit(parser->ast, &ctx);
}