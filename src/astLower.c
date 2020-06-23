#include "astLower.h"

#include <stdlib.h>

// SETTINGS
bool constantFold = false;
bool copyPropagation = false;
bool redundantLoadElimination = false;

typedef struct lowerCtx {
    IrContext* ir;
    IrFunction* fn;
    IrBasicBlock* blk;
} lowerCtx;

static _Noreturn void error(const char* s) {
    printf("%s\n", s);
    exit(1);
}

static IrParameter* constFoldArith(IrParameter* leftParam, IrParameter* rightParam, IrOpcode op, lowerCtx* ctx) {
    if(leftParam->as.constant.undefined || rightParam->as.constant.undefined) {
        printf("Applying %s to undefined value\n", IrInstructionNames[op]);
        // ir builder assumes 0 value at start
    }

    int left = leftParam->as.constant.value;
    int right = rightParam->as.constant.value;

    int value;
    switch(op) {
        case IR_INS_ADD: value = left + right; break;
        case IR_INS_SUB: value = left - right; break;
        case IR_INS_SMUL: value = left * right; break;
        case IR_INS_AND: value = left & right; break;
        case IR_INS_OR: value = left | right; break;
        case IR_INS_XOR: value = left ^ right; break;
        case IR_INS_SHL: value = left << right; break;
        case IR_INS_ASR: value = left >> right; break;
        case IR_INS_SDIV:
            if(right == 0) error("Constant fold div 0");
            value = left / right; break;
        case IR_INS_SREM:
            if(right == 0) error("Constant fold div 0");
            value = left % right; break;
        default:
            error("Undefined constant fold");
    }

    IrParameter* ret = IrParameterCreate(ctx->ir);
    IrParameterConstant(ret, value, 32);

    return ret;
}

static IrParameter* constFoldCompare(IrParameter* leftParam, IrParameter* rightParam, IrComparison op, lowerCtx* ctx) {
    if(leftParam->as.constant.undefined || rightParam->as.constant.undefined) {
        printf("Applying %s to undefined value\n", IrConditionNames[op]);
        // ir builder assumes 0 value at start
    }

    int left = leftParam->as.constant.value;
    int right = rightParam->as.constant.value;

    int value;
    switch(op) {
        case IR_COMPARE_GREATER: value = left > right; break;
        case IR_COMPARE_EQUAL: value = left == right; break;
        case IR_COMPARE_GREATER_EQUAL: value = left >= right; break;
        case IR_COMAPRE_LESS: value = left < right; break;
        case IR_COMPARE_NOT_EQUAL: value = left != right; break;
        case IR_COMPARE_LESS_EQUAL: value = left <= right; break;
        default:
            error("Undefined compare fold");
    }

    IrParameter* ret = IrParameterCreate(ctx->ir);
    IrParameterConstant(ret, value, 32);

    return ret;
}

static void astLowerTypeArr(const ASTVariableType* type, IrParameter* arr) {
    switch(type->type) {
        case AST_VARIABLE_TYPE_INT: {
            IrParameterIntegerType(arr, 32);
        }; break;
        case AST_VARIABLE_TYPE_POINTER: {
            astLowerTypeArr(type->as.pointer, arr);
            IrTypeAddPointer(arr);
        }; break;
        default: error("Unsupported type");
    }
}

static IrParameter* astLowerType(const ASTVariableType* type, lowerCtx* ctx) {
    IrParameter* retType = IrParameterCreate(ctx->ir);
    astLowerTypeArr(type, retType);
    return retType;
}

static IrParameter* astLowerExpression(ASTExpression* exp, lowerCtx* ctx);

static IrParameter* basicArithAssign(ASTAssignExpression* exp, IrOpcode op, lowerCtx* ctx) {
    IrParameter* value = astLowerExpression(exp->value, ctx);
    IrParameter* target = astLowerExpression(exp->target, ctx);
    IrParameter** loc = &exp->target->as.constant.local->vreg;

    IrParameter* params;
    if(op == IR_INS_MAX) {
        params = value;
    } else if(constantFold && value->kind == IR_PARAMETER_CONSTANT && target->kind == IR_PARAMETER_CONSTANT) {
         params = constFoldArith(target, value, op, ctx);
    } else {
        params = IrParametersCreate(ctx->ir, 3);
        IrParameterNewVReg(ctx->fn, params);
        IrParameterReference(params + 1, target);
        IrParameterReference(params + 2, value);
        IrInstructionSetCreate(ctx->ir, ctx->blk, op, params, 3);
    }

    if(exp->target->as.constant.local->vregToAlloca) {
        IrParameter* store = IrParametersCreate(ctx->ir, 2);
        IrParameterVRegRef(store, *loc);
        IrParameterReference(store + 1, params);
        IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_STORE, store, 2);

        exp->target->as.constant.local->prevLoad = NULL;
        return params;
    }

    return *loc = params;
}

static IrParameter* astLowerAssign(ASTAssignExpression* exp, lowerCtx* ctx) {
    if(exp->target->type != AST_EXPRESSION_CONSTANT ||
       exp->target->as.constant.type != AST_CONSTANT_EXPRESSION_LOCAL) {
        error("Unsupported assign target");
    }

    switch(exp->operator.type) {
        case TOKEN_EQUAL: return basicArithAssign(exp, IR_INS_MAX, ctx);
        case TOKEN_PLUS_EQUAL: return basicArithAssign(exp, IR_INS_ADD, ctx);
        case TOKEN_MINUS_EQUAL: return basicArithAssign(exp, IR_INS_SUB, ctx);
        case TOKEN_SLASH_EQUAL: return basicArithAssign(exp, IR_INS_SDIV, ctx);
        case TOKEN_STAR_EQUAL: return basicArithAssign(exp, IR_INS_SMUL, ctx);
        case TOKEN_PERCENT_EQUAL: return basicArithAssign(exp, IR_INS_SREM, ctx);
        case TOKEN_LEFT_SHIFT_EQUAL: return basicArithAssign(exp, IR_INS_SHL, ctx);
        case TOKEN_RIGHT_SHIFT_EQUAL: return basicArithAssign(exp, IR_INS_ASR, ctx);
        case TOKEN_AND_EQUAL: return basicArithAssign(exp, IR_INS_AND, ctx);
        case TOKEN_OR_EQUAL: return basicArithAssign(exp, IR_INS_OR, ctx);
        case TOKEN_XOR_EQUAL: return basicArithAssign(exp, IR_INS_XOR, ctx);
        default:
            error("Unsupported assign");
    }
}

static IrParameter* astLowerConstant(ASTConstantExpression* exp, lowerCtx* ctx) {
    switch(exp->type) {
        case AST_CONSTANT_EXPRESSION_INTEGER: {
            IrParameter* param = IrParametersCreate(ctx->ir, 1);
            IrParameterConstant(param, exp->tok.numberValue, 32);
            return param;
        }; break;
        case AST_CONSTANT_EXPRESSION_LOCAL: {
            if(!exp->local->vregToAlloca) {
                return exp->local->vreg;
            }
            if(redundantLoadElimination && exp->local->prevLoad != NULL) {
                return exp->local->prevLoad;
            }
            IrParameter* load = IrParametersCreate(ctx->ir, 2);
            IrParameterNewVReg(ctx->fn, load);
            IrParameterReference(load + 1, exp->local->vreg);
            IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_LOAD, load, 2);
            exp->local->prevLoad = load;
            return load;
        }
        default:
            error("Unsupported constant");
    }
}

static IrParameter* astLowerUnary(ASTUnaryExpression* exp, lowerCtx* ctx) {
    switch(exp->operator.type) {
        case TOKEN_NEGATE: {
            IrParameter* operand = astLowerExpression(exp->operand, ctx);
            IrParameter* params = IrParametersCreate(ctx->ir, 2);
            IrParameterNewVReg(ctx->fn, params);
            IrParameterReference(params + 1, operand);
            IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_NEGATE, params, 2);
            return params;
        }; break;
        case TOKEN_COMPLIMENT: {
            IrParameter* operand = astLowerExpression(exp->operand, ctx);
            IrParameter* params = IrParametersCreate(ctx->ir, 2);
            IrParameterNewVReg(ctx->fn, params);
            IrParameterReference(params + 1, operand);
            IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_NOT, params, 2);
            return params;
        }; break;
        case TOKEN_NOT: {
            IrParameter* operand = astLowerExpression(exp->operand, ctx);
            IrParameter* params = IrParametersCreate(ctx->ir, 3);
            IrParameterNewVReg(ctx->fn, params);
            IrParameterConstant(params + 1, 0, 8);
            IrParameterReference(params + 2, operand);
            IrInstruction* cmpInst =
                IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_COMPARE, params, 3);
            IrInstructionCondition(cmpInst, IR_COMPARE_EQUAL);
            return params;
        }; break;
        case TOKEN_AND: {
            if(exp->elide) {
                return astLowerExpression(exp->operand, ctx);
            }
            if(!exp->operand->as.constant.local->memoryRequired) {
                error("Address of virtual variable");
            }
            return exp->operand->as.constant.local->vreg;
        }; break;
        default:
            error("Unsupported unary");
    }
}

static IrParameter* basicArith(ASTBinaryExpression* exp, IrOpcode op, lowerCtx* ctx) {
    IrParameter* left = astLowerExpression(exp->left, ctx);
    IrParameter* right = astLowerExpression(exp->right, ctx);

    if(constantFold && left->kind == IR_PARAMETER_CONSTANT && right->kind == IR_PARAMETER_CONSTANT) {
         return constFoldArith(left, right, op, ctx);
    }

    IrParameter* params = IrParametersCreate(ctx->ir, 3);

    IrParameterNewVReg(ctx->fn, params);
    IrParameterReference(params + 1, left);
    IrParameterReference(params + 2, right);
    IrInstructionSetCreate(ctx->ir, ctx->blk, op, params, 3);

    return params;
}

static IrParameter* basicCompare(ASTBinaryExpression* exp, IrComparison op, lowerCtx* ctx) {
    IrParameter* left = astLowerExpression(exp->left, ctx);
    IrParameter* right = astLowerExpression(exp->right, ctx);

    if(constantFold && left->kind == IR_PARAMETER_CONSTANT && right->kind == IR_PARAMETER_CONSTANT) {
         return constFoldCompare(left, right, op, ctx);
    }

    IrParameter* params = IrParametersCreate(ctx->ir, 3);

    IrParameterNewVReg(ctx->fn, params);
    IrParameterReference(params + 1, left);
    IrParameterReference(params + 2, right);
    IrInstruction* cmpInst =
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_COMPARE, params, 3);
    IrInstructionCondition(cmpInst, op);

    return params;
}

static IrParameter* astLowerBinary(ASTBinaryExpression* exp, lowerCtx* ctx) {
    switch(exp->operator.type) {
        case TOKEN_PLUS: return basicArith(exp, IR_INS_ADD, ctx);
        case TOKEN_NEGATE: return basicArith(exp, IR_INS_SUB, ctx);
        case TOKEN_STAR: return basicArith(exp, IR_INS_SMUL, ctx);
        case TOKEN_SLASH: return basicArith(exp, IR_INS_SDIV, ctx);
        case TOKEN_AND: return basicArith(exp, IR_INS_AND, ctx);
        case TOKEN_OR: return basicArith(exp, IR_INS_OR, ctx);
        case TOKEN_XOR: return basicArith(exp, IR_INS_XOR, ctx);
        case TOKEN_SHIFT_LEFT: return basicArith(exp, IR_INS_SHL, ctx);
        case TOKEN_SHIFT_RIGHT: return basicArith(exp, IR_INS_ASR, ctx);
        case TOKEN_EQUAL_EQUAL: return basicCompare(exp, IR_COMPARE_EQUAL, ctx);
        case TOKEN_NOT_EQUAL: return basicCompare(exp, IR_COMPARE_NOT_EQUAL, ctx);
        case TOKEN_LESS: return basicCompare(exp, IR_COMAPRE_LESS, ctx);
        case TOKEN_LESS_EQUAL: return basicCompare(exp, IR_COMPARE_LESS_EQUAL, ctx);
        case TOKEN_GREATER: return basicCompare(exp, IR_COMPARE_GREATER, ctx);
        case TOKEN_GREATER_EQUAL: return basicCompare(exp, IR_COMPARE_GREATER_EQUAL, ctx);
        case TOKEN_PERCENT: return basicArith(exp, IR_INS_SREM, ctx);
        case TOKEN_COMMA:
            astLowerExpression(exp->left, ctx);
            return astLowerExpression(exp->right, ctx);
        default:
            error("Unsupported binary");
    }
}

static IrParameter* astLowerExpression(ASTExpression* exp, lowerCtx* ctx) {
    switch(exp->type) {
        case AST_EXPRESSION_ASSIGN:
            return astLowerAssign(&exp->as.assign, ctx);
        case AST_EXPRESSION_CONSTANT:
            return astLowerConstant(&exp->as.constant, ctx);
        case AST_EXPRESSION_UNARY:
            return astLowerUnary(&exp->as.unary, ctx);
        case AST_EXPRESSION_BINARY:
            return astLowerBinary(&exp->as.binary, ctx);
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
        case AST_STATEMENT_EXPRESSION:
            astLowerExpression(ast->as.expression, ctx);
            break;
        default:
            error("Unsupported statement");
    }
}

static void astLowerDeclaration(ASTDeclaration* decl, lowerCtx* ctx);
static void astLowerBlockItem(ASTBlockItem* ast, lowerCtx* ctx) {
    switch(ast->type) {
        case AST_BLOCK_ITEM_STATEMENT:
            astLowerStatement(ast->as.statement, ctx);
            break;
        case AST_BLOCK_ITEM_DECLARATION:
            astLowerDeclaration(ast->as.declaration, ctx);
            break;
    }
}

static void astLowerFnCompound(ASTFnCompoundStatement* ast, lowerCtx* ctx) {
    ctx->blk = IrBasicBlockCreate(ctx->fn);
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        astLowerBlockItem(ast->items[i], ctx);
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

static void astLowerLocal(ASTInitDeclarator* decl, lowerCtx* ctx) {
    IrParameter* value;

    if(decl->type == AST_INIT_DECLARATOR_INITIALIZE) {
        value = astLowerExpression(decl->initializer, ctx);
    } else {
        value = IrParameterCreate(ctx->ir);
        IrParameterUndefined(value);
    }

    if(copyPropagation && !decl->declarator->symbol->memoryRequired) {
        decl->declarator->symbol->vreg = value;
        decl->declarator->symbol->vregToAlloca = false;
    } else {
        IrParameter* alloca = IrParametersCreate(ctx->ir, 3);
        IrParameterNewVReg(ctx->fn, alloca);
        astLowerTypeArr(decl->declarator->variableType, alloca + 1);
        IrParameterReference(alloca + 2, value);
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_ALLOCA, alloca, 3);

        decl->declarator->symbol->vreg = alloca;
        decl->declarator->symbol->vregToAlloca = true;
        decl->declarator->symbol->prevLoad = NULL;
    }
}

static void astLowerInitDeclarator(ASTInitDeclarator* decl, lowerCtx* ctx) {
    switch(decl->type) {
        case AST_INIT_DECLARATOR_FUNCTION:
            astLowerFunction(decl, ctx);
            break;
        case AST_INIT_DECLARATOR_INITIALIZE:
        case AST_INIT_DECLARATOR_NO_INITIALIZE:
            if(decl->declarator->symbol->scopeDepth == 0) {
                error("Globals unsupported");
            } else {
                astLowerLocal(decl, ctx);
            }
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