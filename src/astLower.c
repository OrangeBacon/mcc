#include "astLower.h"

#include <stdlib.h>

typedef struct lowerCtx {
    IrContext* ir;
    IrFunction* fn;
    IrBasicBlock* blk;
} lowerCtx;

static _Noreturn void error(const char* s) {
    printf("%s\n", s);
    exit(1);
}

static void astLowerTypeArr(const ASTVariableType* type, IrParameter* arr) {
    switch(type->type) {
        case AST_VARIABLE_TYPE_INT: {
            IrParameterIntegerType(arr, 32);
        }; break;
        default: error("Unsupported type");
    }
}

static IrParameter* astLowerType(const ASTVariableType* type, lowerCtx* ctx) {
    IrParameter* retType = IrParameterCreate(ctx->ir);
    astLowerTypeArr(type, retType);
    return retType;
}

static IrParameter* astLowerConstant(ASTConstantExpression* exp, lowerCtx* ctx) {
    switch(exp->type) {
        case AST_CONSTANT_EXPRESSION_INTEGER: {
            IrParameter* param = IrParametersCreate(ctx->ir, 1);
            IrParameterConstant(param, exp->tok.numberValue);
            return param;
        }; break;
        default:
            error("Unsupported constant");
    }
}

static IrParameter* astLowerExpression(ASTExpression* exp, lowerCtx* ctx) {
    switch(exp->type) {
        case AST_EXPRESSION_CONSTANT:
            return astLowerConstant(&exp->as.constant, ctx);
        default:
            error("Unsupported expression");
    }
}

static void astLowerJump(ASTJumpStatement* ast, lowerCtx* ctx) {
    switch(ast->type) {
        case AST_JUMP_STATEMENT_RETURN: {
            IrParameter* param = IrParameterCreate(ctx->ir);
            IrParameterReference(param, astLowerExpression(ast->expr, ctx));
            IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_RETURN, param, 1);
        }; break;
        default:
            error("Unsupported jump");
    }
}

static void astLowerStatement(ASTStatement* ast, lowerCtx* ctx) {
    switch(ast->type) {
        case AST_STATEMENT_JUMP:
            astLowerJump(ast->as.jump, ctx);
            break;
        default:
            error("Unsupported statement");
    }
}

static void astLowerBlockItem(ASTBlockItem* ast, lowerCtx* ctx) {
    switch(ast->type) {
        case AST_BLOCK_ITEM_STATEMENT:
            astLowerStatement(ast->as.statement, ctx);
            break;
        case AST_BLOCK_ITEM_DECLARATION:
            error("Declaration statement unsupported");
    }
}

static void astLowerFnCompound(ASTFnCompoundStatement* ast, lowerCtx* ctx) {
    ctx->blk = IrBasicBlockCreate(ctx->fn);
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        astLowerBlockItem(ast->items[0], ctx);
    }
}

static void astLowerFunction(ASTInitDeclarator* decl, lowerCtx* ctx) {
    SymbolLocal* sym = decl->declarator->symbol;
    const ASTVariableTypeFunction* fnType = &decl->declarator->variableType->as.function;

    IrParameter* retType = astLowerType(fnType->ret, ctx);
    IrParameter* inType = IrParametersCreate(ctx->ir, fnType->paramCount);
    for(unsigned int i = 0; i < fnType->paramCount; i++) {
        astLowerTypeArr(fnType->params[i]->variableType, inType + i);
    }


    IrFunction* fn = IrFunctionCreate(ctx->ir, sym->name, sym->length, retType, inType, fnType->paramCount);
    ctx->fn = fn;

    astLowerFnCompound(decl->fn, ctx);
}

static void astLowerInitDeclarator(ASTInitDeclarator* decl, lowerCtx* ctx) {
    switch(decl->type) {
        case AST_INIT_DECLARATOR_FUNCTION:
            astLowerFunction(decl, ctx);
            break;
        default:
            error("Unsupported init declarator");
    }
}

static void astLowerDeclaration(ASTDeclaration* decl, lowerCtx* ctx) {
    for(unsigned int i = 0; i < decl->declaratorCount; i++) {
        astLowerInitDeclarator(decl->declarators[i], ctx);
    }
}

void astLower(ASTTranslationUnit* ast, IrContext* ir) {
    lowerCtx ctx = {
        .ir = ir,
    };
    for(unsigned int i = 0; i < ast->declarationCount; i++) {
        astLowerDeclaration(ast->declarations[i], &ctx);
    }
}