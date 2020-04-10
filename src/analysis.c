#include "analysis.h"

typedef struct ctx {
    Parser* parser;
    bool inLoop;
} ctx;

static void AnalyseCallExpression(ASTCallExpression* ast, ctx* ctx) {
    if(ast->target->type != AST_EXPRESSION_CONSTANT ||
            ast->target->as.constant.type != AST_CONSTANT_EXPRESSION_GLOBAL) {
        errorAt(ctx->parser, &ast->indirectErrorLoc,
            "Indirect calls not supported yet");
    }
    if(ast->target->as.constant.global->defines[0]->paramCount
            != ast->paramCount) {
        errorAt(ctx->parser, &ast->target->as.constant.tok,
            "Incorrect number of parameters passed");
    }
}

static void AnalyseExpression(ASTExpression* ast, ctx* ctx) {
    if(ast == NULL) return;
    switch(ast->type) {
        case AST_EXPRESSION_ASSIGN:
            AnalyseExpression(ast->as.assign.value, ctx);
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
            AnalyseExpression(ast->as.postfix.operand, ctx);
            break;
        case AST_EXPRESSION_TERNARY:
            AnalyseExpression(ast->as.ternary.operand1, ctx);
            AnalyseExpression(ast->as.ternary.operand2, ctx);
            AnalyseExpression(ast->as.ternary.operand3, ctx);
            break;
        case AST_EXPRESSION_UNARY:
            AnalyseExpression(ast->as.unary.operand, ctx);
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

static void AnalyseDeclaration(ASTDeclaration* ast, ctx* ctx) {
    if(ast == NULL) return;
    for(unsigned int i = 0; i < ast->declarators.declaratorCount; i++) {
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
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        AnalyseBlockItem(ast->items[i], ctx);
    }
}

static void AnalyseFunctionDefinition(ASTFunctionDefinition* ast, ctx* ctx) {
    if(!ast->name->functionAnalysed) {
        bool defined = false;
        int paramCount = -1;
        for(unsigned int i = 0; i < ast->name->defineCount; i++) {
            ASTFunctionDefinition* fn = ast->name->defines[i];

            for(unsigned int j = 0; j < fn->paramCount; j++) {
                ASTInitDeclarator* initDecl = fn->params[j];
                if(initDecl->type == AST_INIT_DECLARATOR_INITIALIZE) {
                    errorAt(ctx->parser, &initDecl->initializerStart,
                        "Cannot have an initializer inside a function definition");
                }
            }

            if(fn->statement != NULL) {
                if(defined) {
                    errorAt(ctx->parser, &fn->errorLoc,
                        "Cannot re-define function");
                }
                defined = true;
            }
            if(paramCount == -1) {
                paramCount = fn->paramCount;
            } else {
                if(paramCount != (int)fn->paramCount) {
                    errorAt(ctx->parser, &fn->errorLoc,
                        "Mismatch in function parameter count");
                }
            }
        }
        ast->name->functionAnalysed = true;
    }
    if(ast->statement == NULL) return;
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