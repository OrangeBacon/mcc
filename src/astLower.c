#include "astLower.h"

#include <stdlib.h>

// SETTINGS
bool constantFold = true;
bool copyPropagation = true;

typedef struct lowerCtx {
    IrContext* ir;
    IrFunction* fn;
    IrBasicBlock* blk;

    IrBasicBlock* breakLocation;
    IrBasicBlock* continueLocation;
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

static IrParameter* constFoldUnary(IrParameter* operand, IrOpcode op, lowerCtx* ctx) {
    if(operand->as.constant.undefined) {
        printf("Applying %s to undefined value\n", IrInstructionNames[op]);
    }

    int value = operand->as.constant.value;
    int retValue;
    switch(op) {
        case IR_INS_NEGATE: retValue = -value; break;
        case IR_INS_NOT: retValue = ~value; break;
        default:
            error("Undefined unary fold");
    }

    IrParameter* ret = IrParameterCreate(ctx->ir);
    IrParameterConstant(ret, retValue, 32);

    return ret;
}

static IrParameter* astLowerType(const ASTVariableType* type, lowerCtx* ctx);
static void astLowerTypeParameter(const ASTVariableType* type, IrParameter* param, lowerCtx* ctx);

static void astLowerTypeArr(const ASTVariableType* type, IrType* arr, lowerCtx* ctx) {
    switch(type->type) {
        case AST_VARIABLE_TYPE_INT: {
            arr->kind = IR_TYPE_INTEGER;
            arr->as.integer = 32;
        }; break;
        case AST_VARIABLE_TYPE_POINTER: {
            astLowerTypeArr(type->as.pointer, arr, ctx);
            arr->pointerDepth++;
        }; break;
        case AST_VARIABLE_TYPE_FUNCTION: {
            arr->kind = IR_TYPE_FUNCTION;
            arr->as.function.retType = astLowerType(type->as.function.ret, ctx);

            arr->as.function.parameterCount = type->as.function.paramCount;
            if(type->as.function.paramCount < 1) break;
            arr->as.function.parameters = IrParametersCreate(ctx->ir, arr->as.function.parameterCount);
            for(unsigned int i = 0; i < type->as.function.paramCount; i++) {
                astLowerTypeParameter(type->as.function.params[i]->variableType, &arr->as.function.parameters[i], ctx);
            }
        }; break;
        default: error("Unsupported type");
    }
}

static void astLowerTypeParameter(const ASTVariableType* type, IrParameter* param, lowerCtx* ctx) {
    param->kind = IR_PARAMETER_TYPE;
    astLowerTypeArr(type, &param->as.type, ctx);
}

static IrParameter* astLowerType(const ASTVariableType* type, lowerCtx* ctx) {
    IrParameter* retType = IrParameterCreate(ctx->ir);
    astLowerTypeParameter(type, retType, ctx);
    return retType;
}

static IrParameter* astLowerExpression(ASTExpression* exp, lowerCtx* ctx);

static IrParameter* variableArithAssign(ASTExpression* exp, IrOpcode op, lowerCtx* ctx) {

    // value of right hand side
    IrParameter* right;

    // the variable being assigned to
    ASTExpression* leftExp;
    SymbolLocal* leftSym;

    if(exp->type == AST_EXPRESSION_ASSIGN) {
        right = astLowerExpression(exp->as.assign.value, ctx);
        leftExp = exp->as.assign.target;
        leftSym = leftExp->as.constant.local;
    } else {
        right = IrParameterCreate(ctx->ir);
        IrParameterConstant(right, 1, 32);
        leftExp = exp->as.postfix.operand;
        leftSym = leftExp->as.constant.local;
    }

    // should GEP be used instead of add
    bool usePointerArithmetic =
        exp->type == AST_EXPRESSION_ASSIGN
        ? exp->as.assign.pointerShift
        : exp->as.postfix.pointerShift;

    // the original value of the variable (used when returning value from
    // ++ or --, so will always get set if relavant)
    IrParameter* initialValue;

    // get thee true value to be set
    if(op != IR_INS_MAX) {
        initialValue = astLowerExpression(leftExp, ctx);

        if(usePointerArithmetic) {
            if(op == IR_INS_SUB) {
                if(constantFold && right->kind == IR_PARAMETER_CONSTANT) {
                    right = constFoldUnary(right, IR_INS_NEGATE, ctx);
                } else {
                    IrParameter* negate = IrParametersCreate(ctx->ir, 2);
                    IrParameterNewVReg(ctx->fn, negate);
                    IrParameterReference(negate + 1, right);
                    IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_NEGATE, negate, 2);
                    right = negate;
                }
            }
            op = IR_INS_GET_ELEMENT_POINTER;
        }

        if(!usePointerArithmetic && constantFold && initialValue->kind == IR_PARAMETER_CONSTANT && right->kind == IR_PARAMETER_CONSTANT) {
            // cannot constant fold GEP
            right = constFoldArith(initialValue, right, op, ctx);
        } else {
            IrParameter* params = IrParametersCreate(ctx->ir, 3);
            IrParameterNewVReg(ctx->fn, params);
            IrParameterReference(params + 1, initialValue);
            IrParameterReference(params + 2, right);
            IrInstructionSetCreate(ctx->ir, ctx->blk, op, params, 3);
            right = params;
        }
    }

    if(leftSym->vregToAlloca) {
        if(leftSym->toGenerateParameter) {
            astLowerExpression(leftExp, ctx);
        }

        IrParameter* params = IrParametersCreate(ctx->ir, 2);
        IrParameterReference(params, leftSym->vreg);
        IrParameterReference(params + 1, right);
        IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_STORE, params, 2);
    } else {
        IrWriteVariable(ctx->fn, leftSym, ctx->blk, right);
    }

    if(exp->type == AST_EXPRESSION_ASSIGN) {
        return right;
    }
    return initialValue;
}

static IrParameter* pointerArithAssign(ASTExpression* exp, IrOpcode op, lowerCtx* ctx) {
    IrParameter* right;

    ASTExpression* leftExp;
    if(exp->type == AST_EXPRESSION_ASSIGN) {
        right = astLowerExpression(exp->as.assign.value, ctx);
        leftExp = exp->as.assign.target;
    } else {
        right = IrParameterCreate(ctx->ir);
        IrParameterConstant(right, 1, 32);
        leftExp = exp->as.postfix.operand;
    }

    // should GEP be used instead of add
    bool usePointerArithmetic =
        exp->type == AST_EXPRESSION_ASSIGN
        ? exp->as.assign.pointerShift
        : exp->as.postfix.pointerShift;

    // remove dereference
    ASTExpression* storeExp = leftExp->as.unary.operand;
    IrParameter* address = astLowerExpression(storeExp, ctx);

    // the original value of the variable (used when returning value from
    // ++ or --, so will always get set if relavant)
    IrParameter* initialValue;

    // get thee true value to be set
    if(op != IR_INS_MAX) {
        initialValue = IrParametersCreate(ctx->ir, 2);
        IrParameterNewVReg(ctx->fn, initialValue);
        IrParameterReference(initialValue + 1, address);
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_LOAD, initialValue, 2);

        if(usePointerArithmetic) {
            if(op == IR_INS_SUB) {
                if(constantFold && right->kind == IR_PARAMETER_CONSTANT) {
                    right = constFoldUnary(right, IR_INS_NEGATE, ctx);
                } else {
                    IrParameter* negate = IrParametersCreate(ctx->ir, 2);
                    IrParameterNewVReg(ctx->fn, negate);
                    IrParameterReference(negate + 1, right);
                    IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_NEGATE, negate, 2);
                    right = negate;
                }
            }
            op = IR_INS_GET_ELEMENT_POINTER;
        }

        if(!usePointerArithmetic && constantFold && initialValue->kind == IR_PARAMETER_CONSTANT && right->kind == IR_PARAMETER_CONSTANT) {
            // cannot constant fold GEP
            right = constFoldArith(initialValue, right, op, ctx);
        } else {
            IrParameter* params = IrParametersCreate(ctx->ir, 3);
            IrParameterNewVReg(ctx->fn, params);
            IrParameterReference(params + 1, initialValue);
            IrParameterReference(params + 2, right);
            IrInstructionSetCreate(ctx->ir, ctx->blk, op, params, 3);
            right = params;
        }
    }

    IrParameter* params = IrParametersCreate(ctx->ir, 2);
    IrParameterReference(params, address);
    IrParameterReference(params + 1, right);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_STORE, params, 2);

    if(exp->type == AST_EXPRESSION_ASSIGN) {
        return right;
    }
    return initialValue;
}


// lowers =, +=, *=, etc and postfix ++/--
static IrParameter* basicArithAssign(ASTExpression* exp, IrOpcode op, lowerCtx* ctx) {

    // what is being assigned to
    ASTExpression *target;

    if(exp->type == AST_EXPRESSION_ASSIGN) {
        // =, +=, etc.
        target = exp->as.assign.target;
    } else {
        // ++, -- (postfix only - parser de-sugars prefix to +=)
        target = exp->as.postfix.operand;
    }

    if(target->type == AST_EXPRESSION_CONSTANT) {
        // simple assingment, eg a = 5, a += 5, a++
        return variableArithAssign(exp, op, ctx);
    } else if(target->type == AST_EXPRESSION_UNARY && target->as.unary.operator.type == TOKEN_STAR) {
        // complex assigment, eg ***a = 5, *(a + 1) += 5, (*a)++
        // (aka left is not a variable reference)
        return pointerArithAssign(exp, op, ctx);
    } else {
        error("Invalid lvalue in assign");
    }
}

static IrParameter* astLowerAssign(ASTExpression* exp, lowerCtx* ctx) {
    switch(exp->as.assign.operator.type) {
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
            if(exp->local->toGenerateParameter) {
                exp->local->toGenerateParameter = false;
                IrParameter* param = IrParametersCreate(ctx->ir, 2);
                IrParameterNewVReg(ctx->fn, param);
                IrParameterConstant(param + 1, exp->local->parameterNumber, 32);
                IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_PARAMETER, param, 2);

                exp->local->vreg = param;
                exp->local->vregToAlloca = false;

                if(exp->local->memoryRequired) {
                    IrParameter* alloca = IrParametersCreate(ctx->ir, 3);
                    IrParameterNewVReg(ctx->fn, alloca);
                    astLowerTypeParameter(exp->local->type, alloca + 1, ctx);
                    IrParameterReference(alloca + 2, param);
                    IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_ALLOCA, alloca, 3);

                    exp->local->vreg = alloca;
                    exp->local->vregToAlloca = true;

                    return param;
                } else {
                    IrWriteVariable(ctx->fn, exp->local, ctx->blk, param);
                    return param;
                }
            }
            if(!exp->local->vregToAlloca) {
                return IrReadVariable(ctx->fn, exp->local, ctx->blk);
            }
            IrParameter* load = IrParametersCreate(ctx->ir, 2);
            IrParameterNewVReg(ctx->fn, load);
            IrParameterReference(load + 1, exp->local->vreg);
            IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_LOAD, load, 2);
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
            if(constantFold && operand->kind == IR_PARAMETER_CONSTANT) {
                return constFoldUnary(operand, IR_INS_NEGATE, ctx);
            }
            IrParameter* params = IrParametersCreate(ctx->ir, 2);
            IrParameterNewVReg(ctx->fn, params);
            IrParameterReference(params + 1, operand);
            IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_NEGATE, params, 2);
            return params;
        }; break;
        case TOKEN_COMPLIMENT: {
            IrParameter* operand = astLowerExpression(exp->operand, ctx);
            if(constantFold && operand->kind == IR_PARAMETER_CONSTANT) {
                return constFoldUnary(operand, IR_INS_NOT, ctx);
            }
            IrParameter* params = IrParametersCreate(ctx->ir, 2);
            IrParameterNewVReg(ctx->fn, params);
            IrParameterReference(params + 1, operand);
            IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_NOT, params, 2);
            return params;
        }; break;
        case TOKEN_NOT: {
            IrParameter* operand = astLowerExpression(exp->operand, ctx);
            if(constantFold && operand->kind == IR_PARAMETER_CONSTANT) {
                IrParameter* zero = IrParameterCreate(ctx->ir);
                IrParameterConstant(zero, 0, 8);
                return constFoldCompare(zero, operand, IR_COMPARE_EQUAL, ctx);
            }
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
            if(!exp->operand->as.constant.local->memoryRequired && exp->operand->as.constant.local->scopeDepth != 0) {
                error("Address of virtual variable");
            }
            if(exp->operand->as.constant.local->toGenerateParameter) {
                astLowerExpression(exp->operand, ctx);
            }
            return exp->operand->as.constant.local->vreg;
        }; break;
        case TOKEN_STAR: {
            if(exp->elide) {
                return astLowerExpression(exp->operand, ctx);
            }
            IrParameter* operand = astLowerExpression(exp->operand, ctx);
            IrParameter* params = IrParametersCreate(ctx->ir, 2);
            IrParameterNewVReg(ctx->fn, params);
            IrParameterReference(params + 1, operand);
            IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_LOAD, params, 2);
            return params;
        }; break;
        case TOKEN_SIZEOF: {
            IrParameter* params = IrParametersCreate(ctx->ir, 2);
            IrParameterNewVReg(ctx->fn, params);
            if(exp->isSizeofType) astLowerTypeParameter(exp->typeExpr, params + 1, ctx);
            else astLowerTypeParameter(exp->operand->exprType, params + 1, ctx);
            IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_SIZEOF, params, 2);
            return params;
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

static IrParameter* maybePointerArith(ASTBinaryExpression* exp, IrOpcode op, lowerCtx* ctx) {
    if(exp->left->exprType->type == AST_VARIABLE_TYPE_POINTER
       && exp->right->exprType->type == AST_VARIABLE_TYPE_POINTER) {
        // pointer subtraction
        IrParameter* first = astLowerExpression(exp->left, ctx);
        IrParameter* firstParams = IrParametersCreate(ctx->ir, 3);
        IrParameterNewVReg(ctx->fn, firstParams);
        IrParameterIntegerType(firstParams + 1, 32);
        IrParameterReference(firstParams + 2, first);
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_CAST, firstParams, 3);

        IrParameter* second = astLowerExpression(exp->right, ctx);
        IrParameter* secondParams = IrParametersCreate(ctx->ir, 3);
        IrParameterNewVReg(ctx->fn, secondParams);
        IrParameterIntegerType(secondParams + 1, 32);
        IrParameterReference(secondParams + 2, second);
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_CAST, secondParams, 3);

        IrParameter* sub = IrParametersCreate(ctx->ir, 3);
        IrParameterNewVReg(ctx->fn, sub);
        IrParameterReference(sub + 1, firstParams);
        IrParameterReference(sub + 2, secondParams);
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_SUB, sub, 3);

        IrParameter* div = IrParametersCreate(ctx->ir, 3);
        IrParameterNewVReg(ctx->fn, div);
        IrParameterReference(div + 1, sub);
        IrParameterConstant(div + 2, 4, 32);
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_SDIV, div, 3);

        return div;
    }

    if(!exp->pointerShift) return basicArith(exp, op, ctx);

    // pointer/integer add/subtract
    IrParameter* pointer;
    IrParameter* integer;
    if(exp->left->exprType->type == AST_VARIABLE_TYPE_POINTER) {
        pointer = astLowerExpression(exp->left, ctx);
        integer = astLowerExpression(exp->right, ctx);
    } else {
        pointer = astLowerExpression(exp->right, ctx);
        integer = astLowerExpression(exp->left, ctx);
    }

    if(op == IR_INS_SUB) {
        if(constantFold && integer->kind == IR_PARAMETER_CONSTANT) {
            integer = constFoldUnary(integer, IR_INS_NEGATE, ctx);
        } else {
            IrParameter* negate = IrParametersCreate(ctx->ir, 2);
            IrParameterNewVReg(ctx->fn, negate);
            IrParameterReference(negate + 1, integer);
            IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_NEGATE, negate, 2);
            integer = negate;
        }
    }

    IrParameter* gep = IrParametersCreate(ctx->ir, 3);
    IrParameterNewVReg(ctx->fn, gep);
    IrParameterReference(gep + 1, pointer);
    IrParameterReference(gep + 2, integer);
    IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_GET_ELEMENT_POINTER, gep, 3);

    return gep;
}

static IrParameter* astLowerShortCircuit(ASTBinaryExpression* exp, lowerCtx* ctx, IrComparison cmp, IrParameter* earlyValue) {
    IrBasicBlock* initialBlock = ctx->blk;
    IrParameter* left = astLowerExpression(exp->left, ctx);

    IrParameter* compare = IrParametersCreate(ctx->ir, 3);
    IrParameterNewVReg(ctx->fn, compare);
    IrParameterConstant(compare + 1, 0, 8);
    IrParameterReference(compare + 2, left);
    IrInstruction* cmpInst =
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_COMPARE, compare, 3);
    IrInstructionCondition(cmpInst, cmp);

    IrBasicBlock* rightBlock = IrBasicBlockCreate(ctx->fn);

    IrBasicBlock* retBlock = IrBasicBlockCreate(ctx->fn);

    IrParameter* branch = IrParametersCreate(ctx->ir, 3);
    IrParameterReference(branch, compare);
    IrParameterBlock(branch + 1, rightBlock);
    IrParameterBlock(branch + 2, retBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP_IF, branch, 3);

    ctx->blk = rightBlock;
    IrParameter* right = astLowerExpression(exp->right, ctx);

    IrParameter* compare2 = IrParametersCreate(ctx->ir, 3);
    IrParameterNewVReg(ctx->fn, compare2);
    IrParameterConstant(compare2 + 1, 0, 8);
    IrParameterReference(compare2 + 2, right);
    IrInstruction* cmpInst2 =
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_COMPARE, compare2, 3);
    IrInstructionCondition(cmpInst2, IR_COMPARE_NOT_EQUAL);

    IrParameter* jump = IrParametersCreate(ctx->ir, 1);
    IrParameterBlock(jump, retBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, jump, 1);

    ctx->blk = retBlock;

    IrPhi* phi = IrPhiCreate(ctx->ir, ctx->blk, NULL);
    IrPhiAddOperand(ctx->ir, phi, initialBlock, earlyValue);
    IrPhiAddOperand(ctx->ir, phi, rightBlock, compare2);
    phi->incomplete = false;

    IrParameter* value = IrParametersCreate(ctx->ir, 3);
    IrParameterNewVReg(ctx->fn, value);
    IrParameterIntegerType(value + 1, 32);
    IrParameterReference(value + 2, &phi->result);
    IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_CAST, value, 3);

    IrSealBlock(ctx->fn, rightBlock);
    IrSealBlock(ctx->fn, retBlock);

    return value;
}

static IrParameter* astLowerOr(ASTBinaryExpression* exp, lowerCtx* ctx) {
    IrParameter* one = IrParameterCreate(ctx->ir);
    IrParameterConstant(one, 1, 8);
    return astLowerShortCircuit(exp, ctx, IR_COMPARE_EQUAL, one);
}

static IrParameter* astLowerAnd(ASTBinaryExpression* exp, lowerCtx* ctx) {
    IrParameter* zero = IrParameterCreate(ctx->ir);
    IrParameterConstant(zero, 0, 8);
    return astLowerShortCircuit(exp, ctx, IR_COMPARE_NOT_EQUAL, zero);
}

static IrParameter* astLowerBinary(ASTBinaryExpression* exp, lowerCtx* ctx) {
    switch(exp->operator.type) {
        case TOKEN_PLUS: return maybePointerArith(exp, IR_INS_ADD, ctx);
        case TOKEN_NEGATE: return maybePointerArith(exp, IR_INS_SUB, ctx);
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
        case TOKEN_OR_OR: return astLowerOr(exp, ctx);
        case TOKEN_AND_AND: return astLowerAnd(exp, ctx);
        case TOKEN_COMMA:
            astLowerExpression(exp->left, ctx);
            return astLowerExpression(exp->right, ctx);
        default:
            error("Unsupported binary");
    }
}

static IrParameter* astLowerPostfix(ASTExpression* exp, lowerCtx* ctx) {
    IrOpcode opcode = IR_INS_ADD;
    if(exp->as.postfix.operator.type == TOKEN_MINUS_MINUS) {
        opcode = IR_INS_SUB;
    }

    return basicArithAssign(exp, opcode, ctx);
}

static IrParameter* astLowerCast(ASTCastExpression* exp, lowerCtx* ctx) {
    IrParameter* value = astLowerExpression(exp->expression, ctx);

    IrParameter* castType = astLowerType(exp->type->variableType, ctx);
    if(IrTypeEqual(IrParameterGetType(value), &castType->as.type)) return value;

    IrParameter* params = IrParametersCreate(ctx->ir, 3);
    IrParameterNewVReg(ctx->fn, params);
    params[1] = *castType;
    IrParameterReference(params + 2, value);
    IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_CAST, params, 3);

    return params;
}

static IrParameter* astLowerCall(ASTCallExpression* exp, lowerCtx* ctx) {
    IrParameter* target = astLowerExpression(exp->target, ctx);

    IrParameter* params = IrParametersCreate(ctx->ir, 2 + exp->paramCount);
    IrParameterReference(params + 1, target);

    for(unsigned int i = 0; i < exp->paramCount; i++) {
        IrParameterReference(params + i + 2, astLowerExpression(exp->params[i], ctx));
    }

    // create new register after arguments so ir is created in
    // more readable order
    IrParameterNewVReg(ctx->fn, params);
    IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_CALL, params, 2 + exp->paramCount);

    return params;
}

static IrParameter* astLowerTernary(ASTTernaryExpression* exp, lowerCtx* ctx) {
    IrParameter* condition = astLowerExpression(exp->operand1, ctx);

    if(constantFold && condition->kind == IR_PARAMETER_CONSTANT) {
        if(condition->as.constant.value == 0) {
            return astLowerExpression(exp->operand2, ctx);
        } else {
            return astLowerExpression(exp->operand3, ctx);
        }
    }

    IrBasicBlock* thenBlock = IrBasicBlockCreate(ctx->fn);
    IrSealBlock(ctx->fn, thenBlock);

    IrBasicBlock* elseBlock = IrBasicBlockCreate(ctx->fn);
    IrSealBlock(ctx->fn, elseBlock);

    IrBasicBlock* retBlock = IrBasicBlockCreate(ctx->fn);

    IrParameter* jump = IrParametersCreate(ctx->ir, 3);
    IrParameterReference(jump, condition);
    IrParameterBlock(jump + 1, thenBlock);
    IrParameterBlock(jump + 2, elseBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP_IF, jump, 3);
    IrSealBlock(ctx->fn, ctx->blk);

    ctx->blk = thenBlock;
    IrParameter* thenValue = astLowerExpression(exp->operand2, ctx);

    IrParameter* thenJump = IrParametersCreate(ctx->ir, 1);
    IrParameterBlock(thenJump, retBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, thenJump, 1);
    IrSealBlock(ctx->fn, ctx->blk);

    ctx->blk = elseBlock;
    IrParameter* elseValue = astLowerExpression(exp->operand3, ctx);

    IrParameter* elseJump = IrParametersCreate(ctx->ir, 1);
    IrParameterBlock(elseJump, retBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, elseJump, 1);
    IrSealBlock(ctx->fn, ctx->blk);

    ctx->blk = retBlock;

    IrPhi* value = IrPhiCreate(ctx->ir, ctx->blk, NULL);
    IrPhiAddOperand(ctx->ir, value, thenBlock, thenValue);
    IrPhiAddOperand(ctx->ir, value, elseBlock, elseValue);
    value->incomplete = false;



    IrSealBlock(ctx->fn, retBlock);

    return &value->result;
}

static IrParameter* astLowerExpression(ASTExpression* exp, lowerCtx* ctx) {
    switch(exp->type) {
        case AST_EXPRESSION_ASSIGN:
            return astLowerAssign(exp, ctx);
        case AST_EXPRESSION_CONSTANT:
            return astLowerConstant(&exp->as.constant, ctx);
        case AST_EXPRESSION_UNARY:
            return astLowerUnary(&exp->as.unary, ctx);
        case AST_EXPRESSION_BINARY:
            return astLowerBinary(&exp->as.binary, ctx);
        case AST_EXPRESSION_POSTFIX:
            return astLowerPostfix(exp, ctx);
        case AST_EXPRESSION_CAST:
            return astLowerCast(&exp->as.cast, ctx);
        case AST_EXPRESSION_CALL:
            return astLowerCall(&exp->as.call, ctx);
        case AST_EXPRESSION_TERNARY:
            return astLowerTernary(&exp->as.ternary, ctx);
        default:
            error("Unsupported expression");
    }
}

static void astLowerJump(ASTJumpStatement* ast, lowerCtx* ctx) {
    switch(ast->type) {
        case AST_JUMP_STATEMENT_RETURN: {
            IrParameter* param = IrParameterCreate(ctx->ir);
            IrParameter* expr = astLowerExpression(ast->expr, ctx);
            IrParameterReference(param, expr);
            IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_RETURN, param, 1);
        }; break;
        case AST_JUMP_STATEMENT_BREAK: {

            IrParameter* param = IrParameterCreate(ctx->ir);
            IrParameterBlock(param, ctx->breakLocation);
            IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, param, 1);
        }; break;
        case AST_JUMP_STATEMENT_CONTINUE: {

            IrParameter* param = IrParameterCreate(ctx->ir);
            IrParameterBlock(param, ctx->continueLocation);
            IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, param, 1);
        }; break;
    }
}

static void astLowerBlockItem(ASTBlockItem* ast, lowerCtx* ctx);
static void astLowerCompound(ASTCompoundStatement* ast, lowerCtx* ctx) {
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        astLowerBlockItem(ast->items[i], ctx);
    }
}

static void astLowerStatement(ASTStatement* ast, lowerCtx* ctx);
static void astLowerSelection(ASTSelectionStatement* ast, lowerCtx* ctx) {
    IrParameter* condition = astLowerExpression(ast->condition, ctx);
    if(constantFold && condition->kind == IR_PARAMETER_CONSTANT) {
        if(condition->as.constant.value == 0) {
            if(ast->type == AST_SELECTION_STATEMENT_IF) {
                return;
            } else {
                astLowerStatement(ast->elseBlock, ctx);
                return;
            }
        } else {
            astLowerStatement(ast->block, ctx);
            return;
        }
    }

    IrBasicBlock* thenBlock = IrBasicBlockCreate(ctx->fn);
    IrSealBlock(ctx->fn, thenBlock);

    IrBasicBlock* elseBlock = IrBasicBlockCreate(ctx->fn);

    IrParameter* jump = IrParametersCreate(ctx->ir, 3);
    IrParameterReference(jump, condition);
    IrParameterBlock(jump + 1, thenBlock);
    IrParameterBlock(jump + 2, elseBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP_IF, jump, 3);

    ctx->blk = thenBlock;
    astLowerStatement(ast->block, ctx);

    if(ast->type == AST_SELECTION_STATEMENT_IF) {
        IrParameter* jump = IrParametersCreate(ctx->ir, 1);
        IrParameterBlock(jump, elseBlock);
        IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, jump, 1);

        IrSealBlock(ctx->fn, thenBlock);
        IrSealBlock(ctx->fn, elseBlock);
        ctx->blk = elseBlock;
    } else {
        IrBasicBlock* retBlock = IrBasicBlockCreate(ctx->fn);


        IrParameter* thenJump = IrParametersCreate(ctx->ir, 1);
        IrParameterBlock(thenJump, retBlock);
        IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, thenJump, 1);

        ctx->blk = elseBlock;
        astLowerStatement(ast->elseBlock, ctx);

        IrParameter* elseJump = IrParametersCreate(ctx->ir, 1);
        IrParameterBlock(elseJump, retBlock);
        IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, elseJump, 1);

        IrSealBlock(ctx->fn, thenBlock);
        IrSealBlock(ctx->fn, elseBlock);
        IrSealBlock(ctx->fn, retBlock);
        ctx->blk = retBlock;
    }
}

static void astLowerWhile(ASTIterationStatement* ast, lowerCtx* ctx) {
    // while
    //   initial block
    //     jump block 1
    //   block 1
    //     condition
    //     jump if block 2 block 3
    //   block 2
    //     statment
    //   seal block 1, block 2
    //     jump block 1
    //   block 3
    //     exit block (set, do not use)
    // break -> block 3
    // continue -> block 1

    IrBasicBlock* conditionBlock = IrBasicBlockCreate(ctx->fn);
    IrBasicBlock* statementBlock = IrBasicBlockCreate(ctx->fn);
    IrBasicBlock* exitBlock = IrBasicBlockCreate(ctx->fn);

    IrParameter* startJump = IrParametersCreate(ctx->ir, 1);
    IrParameterBlock(startJump, conditionBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, startJump, 1);

    ctx->blk = conditionBlock;
    IrParameter* condition = astLowerExpression(ast->control, ctx);

    IrParameter* compare = IrParametersCreate(ctx->ir, 3);
    IrParameterNewVReg(ctx->fn, compare);
    IrParameterConstant(compare + 1, 0, 8);
    IrParameterReference(compare + 2, condition);
    IrInstruction* compareInstruction =
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_COMPARE, compare, 3);
    IrInstructionCondition(compareInstruction, IR_COMPARE_NOT_EQUAL);

    IrParameter* compareJump = IrParametersCreate(ctx->ir, 3);
    IrParameterReference(compareJump, compare);
    IrParameterBlock(compareJump + 1, statementBlock);
    IrParameterBlock(compareJump + 2, exitBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP_IF, compareJump, 3);

    ctx->blk = statementBlock;
    IrBasicBlock* oldBreak = ctx->breakLocation;
    IrBasicBlock* oldContinue = ctx->continueLocation;
    ctx->breakLocation = exitBlock;
    ctx->continueLocation = conditionBlock;
    astLowerStatement(ast->body, ctx);
    ctx->breakLocation = oldBreak;
    ctx->continueLocation = oldContinue;

    IrParameter* afterJump = IrParametersCreate(ctx->ir, 1);
    IrParameterBlock(afterJump, conditionBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, afterJump, 1);

    IrSealBlock(ctx->fn, conditionBlock);
    IrSealBlock(ctx->fn, statementBlock);
    IrSealBlock(ctx->fn, exitBlock);

    ctx->blk = exitBlock;
}

static void astLowerDoWhile(ASTIterationStatement* ast, lowerCtx* ctx) {
    // do while
    //   initial block
    //     jump block 1
    //   block 1
    //     statement
    //     jump block 2
    //   block 2
    //     expression
    //     compare
    //     branch true -> 1, false -> 3
    //   block 3
    //     exit block
    // continue = block 2
    // break = block 3

    IrBasicBlock* statementBlock = IrBasicBlockCreate(ctx->fn);
    IrBasicBlock* conditionBlock = IrBasicBlockCreate(ctx->fn);
    IrBasicBlock* exitBlock = IrBasicBlockCreate(ctx->fn);

    IrParameter* startJump = IrParametersCreate(ctx->ir, 1);
    IrParameterBlock(startJump, statementBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, startJump, 1);

    ctx->blk = statementBlock;
    IrBasicBlock* oldBreak = ctx->breakLocation;
    IrBasicBlock* oldContinue = ctx->continueLocation;
    ctx->breakLocation = exitBlock;
    ctx->continueLocation = conditionBlock;
    astLowerStatement(ast->body, ctx);
    ctx->breakLocation = oldBreak;
    ctx->continueLocation = oldContinue;

    IrParameter* afterJump = IrParametersCreate(ctx->ir, 1);
    IrParameterBlock(afterJump, conditionBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, afterJump, 1);

    ctx->blk = conditionBlock;
    IrParameter* condition = astLowerExpression(ast->control, ctx);

    IrParameter* compare = IrParametersCreate(ctx->ir, 3);
    IrParameterNewVReg(ctx->fn, compare);
    IrParameterConstant(compare + 1, 0, 8);
    IrParameterReference(compare + 2, condition);
    IrInstruction* compareInstruction =
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_COMPARE, compare, 3);
    IrInstructionCondition(compareInstruction, IR_COMPARE_NOT_EQUAL);

    IrParameter* compareJump = IrParametersCreate(ctx->ir, 3);
    IrParameterReference(compareJump, compare);
    IrParameterBlock(compareJump + 1, exitBlock);
    IrParameterBlock(compareJump + 2, statementBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP_IF, compareJump, 3);

    IrSealBlock(ctx->fn, conditionBlock);
    IrSealBlock(ctx->fn, statementBlock);
    IrSealBlock(ctx->fn, exitBlock);

    ctx->blk = exitBlock;
}

static void astLowerDeclaration(ASTDeclaration* decl, lowerCtx* ctx);
static void astLowerFor(ASTIterationStatement* ast, lowerCtx* ctx) {
    // for loop
    //   initial block
    //     initial statement
    //     jump condition block
    //   condition block
    //     condition
    //     compare not equal 0
    //     jump if true -> statement false -> exit
    //   statement block
    //     statement
    //     jump post block
    //   post block
    //     post statement
    //     jump condition block
    //   exit block
    //     (empty)
    // continue = post block
    // break = exit block

    IrBasicBlock* conditionBlock = IrBasicBlockCreate(ctx->fn);
    IrBasicBlock* statementBlock = IrBasicBlockCreate(ctx->fn);
    IrBasicBlock* postBlock = IrBasicBlockCreate(ctx->fn);
    IrBasicBlock* exitBlock = IrBasicBlockCreate(ctx->fn);

    if(ast->type == AST_ITERATION_STATEMENT_FOR_DECL) {
        astLowerDeclaration(ast->preDecl, ctx);
    } else {
        if(ast->preExpr) astLowerExpression(ast->preExpr, ctx);
    }

    IrParameter* startJump = IrParametersCreate(ctx->ir, 1);
    IrParameterBlock(startJump, conditionBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, startJump, 1);

    ctx->blk = conditionBlock;
    IrParameter* condition = astLowerExpression(ast->control, ctx);

    IrParameter* compare = IrParametersCreate(ctx->ir, 3);
    IrParameterNewVReg(ctx->fn, compare);
    IrParameterConstant(compare + 1, 0, 8);
    IrParameterReference(compare + 2, condition);
    IrInstruction* compareInstruction =
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_COMPARE, compare, 3);
    IrInstructionCondition(compareInstruction, IR_COMPARE_NOT_EQUAL);

    IrParameter* compareJump = IrParametersCreate(ctx->ir, 3);
    IrParameterReference(compareJump, compare);
    IrParameterBlock(compareJump + 1, statementBlock);
    IrParameterBlock(compareJump + 2, exitBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP_IF, compareJump, 3);

    ctx->blk = statementBlock;
    IrBasicBlock* oldBreak = ctx->breakLocation;
    IrBasicBlock* oldContinue = ctx->continueLocation;
    ctx->breakLocation = exitBlock;
    ctx->continueLocation = conditionBlock;
    astLowerStatement(ast->body, ctx);
    ctx->breakLocation = oldBreak;
    ctx->continueLocation = oldContinue;

    IrParameter* statementJump = IrParametersCreate(ctx->ir, 1);
    IrParameterBlock(statementJump, postBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, statementJump, 1);

    ctx->blk = postBlock;
    if(ast->post) astLowerExpression(ast->post, ctx);

    IrParameter* postJump = IrParametersCreate(ctx->ir, 1);
    IrParameterBlock(postJump, conditionBlock);
    IrInstructionVoidCreate(ctx->ir, ctx->blk, IR_INS_JUMP, postJump, 1);

    IrSealBlock(ctx->fn, exitBlock);
    IrSealBlock(ctx->fn, postBlock);
    IrSealBlock(ctx->fn, statementBlock);
    IrSealBlock(ctx->fn, conditionBlock);

    ctx->blk = exitBlock;
}

static void astLowerIteration(ASTIterationStatement* ast, lowerCtx* ctx) {
    switch(ast->type) {
        case AST_ITERATION_STATEMENT_WHILE:
            astLowerWhile(ast, ctx); break;
        case AST_ITERATION_STATEMENT_DO:
            astLowerDoWhile(ast, ctx); break;
        case AST_ITERATION_STATEMENT_FOR_DECL:
        case AST_ITERATION_STATEMENT_FOR_EXPR:
            astLowerFor(ast, ctx); break;
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
        case AST_STATEMENT_COMPOUND:
            astLowerCompound(ast->as.compound, ctx);
            break;
        case AST_STATEMENT_SELECTION:
            astLowerSelection(ast->as.selection, ctx);
            break;
        case AST_STATEMENT_ITERATION:
            astLowerIteration(ast->as.iteration, ctx);
        case AST_STATEMENT_NULL:
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
    IrSealBlock(ctx->fn, ctx->blk);

    for(unsigned int i = 0; i < ast->itemCount; i++) {
        astLowerBlockItem(ast->items[i], ctx);
    }
}

static void astLowerFunction(ASTInitDeclarator* decl, lowerCtx* ctx) {
    SymbolLocal* sym = decl->declarator->symbol;
    const ASTVariableTypeFunction* fnType = &decl->declarator->variableType->as.function;

    for(unsigned int i = 0; i < fnType->paramCount; i++) {
        fnType->params[i]->symbol->toGenerateParameter = true;
        fnType->params[i]->symbol->parameterNumber = i;
    }

    IrFunction* fn;
    if(sym->vreg == NULL) {
        IrParameter* retType = astLowerType(fnType->ret, ctx);
        IrParameter* inType = IrParametersCreate(ctx->ir, fnType->paramCount);
        for(unsigned int i = 0; i < fnType->paramCount; i++) {
            astLowerTypeParameter(fnType->params[i]->variableType, inType + i, ctx);
        }
        IrTopLevel* topFn = IrFunctionCreate(ctx->ir, sym->name, sym->length, retType, inType, fnType->paramCount);
        fn = &topFn->as.function;

        sym->vreg = IrParameterCreate(ctx->ir);
        sym->vregToAlloca = true;
        IrParameterTopLevel(sym->vreg, topFn);
        topFn->type.kind = IR_TYPE_FUNCTION;
        topFn->type.pointerDepth = 0;
        topFn->type.as.function.parameterCount = fnType->paramCount;
        topFn->type.as.function.parameters = inType;
        topFn->type.as.function.retType = retType;
    } else {
        fn = &sym->vreg->as.topLevel->as.function;
    }

    if(decl->fn != NULL) {
        ctx->fn = fn;
        astLowerFnCompound(decl->fn, ctx);
        // finish all phis in the function
        IrSealBlock(ctx->fn, ctx->blk);
        IrTryRemoveTrivialBlocks(fn);
    }
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
        IrWriteVariable(ctx->fn, decl->declarator->symbol, ctx->blk, value);
        decl->declarator->symbol->vregToAlloca = false;
    } else {
        IrParameter* alloca = IrParametersCreate(ctx->ir, 3);
        IrParameterNewVReg(ctx->fn, alloca);
        astLowerTypeParameter(decl->declarator->variableType, alloca + 1, ctx);
        IrParameterReference(alloca + 2, value);
        IrInstructionSetCreate(ctx->ir, ctx->blk, IR_INS_ALLOCA, alloca, 3);

        decl->declarator->symbol->vreg = alloca;
        decl->declarator->symbol->vregToAlloca = true;
    }
}

static void astLowerGlobal(ASTInitDeclarator* decl, lowerCtx* ctx) {
    SymbolLocal* sym = decl->declarator->symbol;

    IrTopLevel* globl;
    if(sym->vreg == NULL) {
        globl = IrGlobalPrototypeCreate(ctx->ir, sym->name, sym->length);
        sym->vreg = IrParameterCreate(ctx->ir);
        sym->vregToAlloca = true;
        IrParameterTopLevel(sym->vreg, globl);
        astLowerTypeArr(decl->declarator->variableType, &globl->type, ctx);
        globl->type.pointerDepth++;
    } else {
        globl = sym->vreg->as.topLevel;
    }

    if(globl->as.global.undefined && decl->initializer != NULL) {
        IrGlobalInitialize(globl, decl->initializer->as.constant.tok.numberValue, 32);
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
                astLowerGlobal(decl, ctx);
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