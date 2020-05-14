#include "x64.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "assemble.h"

static unsigned int getID() {
    static int i = 0;
    return i++;
}

static void x64ASTGenExpression(ASTExpression* ast, x64Ctx* ctx);

static void x64ASTGenBinary(ASTBinaryExpression* ast, x64Ctx* ctx) {
    switch(ast->operator.type) {
        case TOKEN_PLUS:
            x64ASTGenExpression(ast->left, ctx);
            if(ast->pointerShift && ast->left->exprType->type != AST_VARIABLE_TYPE_POINTER) {
                asmShiftLeftInt(ctx, RAX, 3);
            }
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            if(ast->pointerShift && ast->right->exprType->type != AST_VARIABLE_TYPE_POINTER) {
                asmShiftLeftInt(ctx, RAX, 3);
            }
            asmPop(ctx, RCX);
            asmAdd(ctx, RCX, RAX);
            break;
        case TOKEN_NEGATE:
            x64ASTGenExpression(ast->right, ctx);
            if(ast->pointerShift && ast->right->exprType->type != AST_VARIABLE_TYPE_POINTER) {
                asmShiftLeftInt(ctx, RAX, 3);
            }
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->left, ctx);
            if(ast->pointerShift && ast->left->exprType->type != AST_VARIABLE_TYPE_POINTER) {
                asmShiftLeftInt(ctx, RAX, 3);
            }
            asmPop(ctx, RCX);
            asmSub(ctx, RCX, RAX);
            break;
        case TOKEN_STAR:
            x64ASTGenExpression(ast->left, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            asmPop(ctx, RCX);
            asmIMul(ctx, RCX, RAX);
            break;
        case TOKEN_SLASH:
            x64ASTGenExpression(ast->right, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->left, ctx);
            asmPop(ctx, RCX);
            asmRAXExtend(ctx);
            asmIDiv(ctx, RCX);
            break;
        case TOKEN_EQUAL_EQUAL:
            x64ASTGenExpression(ast->left, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            asmPop(ctx, RCX);
            asmCmp(ctx, RAX, RCX);
            asmRegSet(ctx, RAX, 0);
            asmSetcc(ctx, EQUAL, RAX);
            break;
        case TOKEN_NOT_EQUAL:
            x64ASTGenExpression(ast->left, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            asmPop(ctx, RCX);
            asmCmp(ctx, RAX, RCX);
            asmRegSet(ctx, RAX, 0);
            asmSetcc(ctx, NOT_EQUAL, RAX);
            break;
        case TOKEN_LESS:
            x64ASTGenExpression(ast->left, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            asmPop(ctx, RCX);
            asmCmp(ctx, RAX, RCX);
            asmRegSet(ctx, RAX, 0);
            asmSetcc(ctx, LESS, RAX);
            break;
        case TOKEN_LESS_EQUAL:
            x64ASTGenExpression(ast->left, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            asmPop(ctx, RCX);
            asmCmp(ctx, RAX, RCX);
            asmRegSet(ctx, RAX, 0);
            asmSetcc(ctx, LESS_EQUAL, RAX);
            break;
        case TOKEN_GREATER:
            x64ASTGenExpression(ast->left, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            asmPop(ctx, RCX);
            asmCmp(ctx, RAX, RCX);
            asmRegSet(ctx, RAX, 0);
            asmSetcc(ctx, GREATER, RAX);
            break;
        case TOKEN_GREATER_EQUAL:
            x64ASTGenExpression(ast->left, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            asmPop(ctx, RCX);
            asmCmp(ctx, RAX, RCX);
            asmRegSet(ctx, RAX, 0);
            asmSetcc(ctx, GREATER_EQUAL, RAX);
            break;
        case TOKEN_OR_OR: {
            unsigned int clause2 = getID();
            unsigned int end = getID();
            x64ASTGenExpression(ast->left, ctx);
            asmICmp(ctx, RAX, 0);
            asmJumpCC(ctx, EQUAL, clause2);
            asmRegSet(ctx, RAX, 1);
            asmJump(ctx, end);
            asmJumpTarget(ctx, clause2);
            x64ASTGenExpression(ast->right, ctx);
            asmICmp(ctx, RAX, 0);
            asmRegSet(ctx, RAX, 0);
            asmSetcc(ctx, NOT_EQUAL, RAX);
            asmJumpTarget(ctx, end);
        }; break;
        case TOKEN_AND_AND: {
            unsigned int clause2 = getID();
            unsigned int end = getID();
            x64ASTGenExpression(ast->left, ctx);
            asmICmp(ctx, RAX, 0);
            asmJumpCC(ctx, NOT_EQUAL, clause2);
            asmJump(ctx, end);
            asmJumpTarget(ctx, clause2);
            x64ASTGenExpression(ast->right, ctx);
            asmICmp(ctx, RAX, 0);
            asmRegSet(ctx, RAX, 0);
            asmSetcc(ctx, NOT_EQUAL, RAX);
            asmJumpTarget(ctx, end);
        }; break;
        case TOKEN_OR:
            x64ASTGenExpression(ast->left, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            asmPop(ctx, RCX);
            asmOr(ctx, RCX, RAX);
            break;
        case TOKEN_AND:
            x64ASTGenExpression(ast->left, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            asmPop(ctx, RCX);
            asmAnd(ctx, RCX, RAX);
            break;
        case TOKEN_PERCENT:
            x64ASTGenExpression(ast->right, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->left, ctx);
            asmPop(ctx, RCX);
            asmRAXExtend(ctx);
            asmIDiv(ctx, RCX);
            asmRegMov(ctx, RDX, RAX);
            break;
        case TOKEN_XOR:
            x64ASTGenExpression(ast->left, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->right, ctx);
            asmPop(ctx, RCX);
            asmXor(ctx, RCX, RAX);
            break;
        case TOKEN_SHIFT_LEFT:
            x64ASTGenExpression(ast->right, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->left, ctx);
            asmPop(ctx, RCX);
            asmShiftLeft(ctx, RCX, RAX);
            break;
        case TOKEN_SHIFT_RIGHT:
            x64ASTGenExpression(ast->right, ctx);
            asmPush(ctx, RAX);
            x64ASTGenExpression(ast->left, ctx);
            asmPop(ctx, RCX);
            asmShiftRight(ctx, RCX, RAX);
            break;
        case TOKEN_COMMA:
            x64ASTGenExpression(ast->left, ctx);
            x64ASTGenExpression(ast->right, ctx);
            break;
        default:
            printf("x64 unreachable binary\n");
            exit(0);
    }
}

static void x64ASTGenUnary(ASTUnaryExpression* ast, x64Ctx* ctx) {
    if(ast->elide) {
        x64ASTGenExpression(ast->operand, ctx);
        return;
    }
    switch(ast->operator.type) {
        case TOKEN_NOT:
            x64ASTGenExpression(ast->operand, ctx);
            asmCmp(ctx, RAX, 0);
            asmRegSet(ctx, RAX, 0);
            asmSetcc(ctx, EQUAL, RAX);
            break;
        case TOKEN_NEGATE:
            x64ASTGenExpression(ast->operand, ctx);
            asmNeg(ctx, RAX);
            break;
        case TOKEN_COMPLIMENT:
            x64ASTGenExpression(ast->operand, ctx);
            asmNot(ctx, RAX);
            break;
        case TOKEN_AND: {
            SymbolLocal* symbol = ast->operand->as.constant.local;
            if(symbol->scopeDepth == 0) {
                asmLoadName(ctx, symbol->length, symbol->name, RAX);
            } else {
                asmLeaDerefOffset(ctx, RBP, symbol->stackOffset, RAX);
            }
        }; break;
        case TOKEN_STAR:
            x64ASTGenExpression(ast->operand, ctx);
            asmDeref(ctx, RAX, RAX);
            break;
        default:
            printf("x64 unreachable unary\n");
            exit(0);
    }
}

static void x64ASTGenConstant(ASTConstantExpression* ast, x64Ctx* ctx) {
    switch(ast->type) {
        case AST_CONSTANT_EXPRESSION_LOCAL:
            if(ast->local->scopeDepth == 0) {
                asmLoadName(ctx, ast->local->length, ast->local->name, RAX);
                asmDeref(ctx, RAX, RAX);
            } else {
                asmDerefOffset(ctx, RBP, ast->local->stackOffset, RAX);
            }
            break;
        case AST_CONSTANT_EXPRESSION_INTEGER:
            asmRegSet(ctx, RAX, ast->tok.numberValue);
            break;
    }
}

static ASTExpression* x64RAXLoadAddress(ASTExpression* ast) {
    if(ast->type == AST_EXPRESSION_UNARY &&
    ast->as.unary.operator.type == TOKEN_STAR &&
    !ast->as.unary.elide) {
        return ast->as.unary.operand;
    } else {
        ASTExpression* adrOf = ArenaAlloc(sizeof(*adrOf));
        adrOf->type = AST_EXPRESSION_UNARY;
        adrOf->as.unary.elide = false;
        adrOf->as.unary.operator = TokenMake(TOKEN_AND);
        adrOf->as.unary.operand = ast;
        return adrOf;
    }
}

static void x64ASTGenAssign(ASTAssignExpression* ast, x64Ctx* ctx) {
    x64ASTGenExpression(x64RAXLoadAddress(ast->target), ctx);
    asmPush(ctx, RAX);

    x64ASTGenExpression(ast->value, ctx);
    if(ast->pointerShift) {
        asmShiftLeftInt(ctx, RAX, 3);
    }

    asmPop(ctx, R9);

    // value to store/operate on is in rax
    // address to store value to is in r9
    // rcx is required by shift
    // rdx is required by division
    // r8  is required by division

    switch(ast->operator.type) {
        case TOKEN_EQUAL:
            asmRegMovAddr(ctx, RAX, R9);
            break;
        case TOKEN_PLUS_EQUAL:
            asmAddDeref(ctx, R9, RAX);
            asmRegMovAddr(ctx, RAX, R9);
            break;
        case TOKEN_MINUS_EQUAL:
            asmDeref(ctx, R9, RDX);
            asmSub(ctx, RAX, RDX);
            asmRegMovAddr(ctx, RDX, R9);
            break;
        case TOKEN_SLASH_EQUAL:
            asmRegMov(ctx, RAX, R8);
            asmDeref(ctx, R9, RAX);
            asmRAXExtend(ctx);
            asmIDiv(ctx, R8);
            asmRegMovAddr(ctx, RAX, R9);
            break;
        case TOKEN_STAR_EQUAL:
            asmIMulDeref(ctx, R9, RAX);
            asmRegMovAddr(ctx, RAX, R9);
            break;
        case TOKEN_PERCENT_EQUAL:
            asmRegMov(ctx, RAX, R8);
            asmDeref(ctx, R9, RAX);
            asmRAXExtend(ctx);
            asmIDiv(ctx, R8);
            asmRegMov(ctx, RDX, RAX);
            asmRegMovAddr(ctx, RAX, R9);
            break;
        case TOKEN_LEFT_SHIFT_EQUAL:
            asmDeref(ctx, R9, RDX);
            asmRegMov(ctx, RAX, RCX);
            asmShiftLeft(ctx, RCX, RDX);
            asmRegMovAddr(ctx, RDX, R9);
            break;
        case TOKEN_RIGHT_SHIFT_EQUAL:
            asmDeref(ctx, R9, RDX);
            asmRegMov(ctx, RAX, RCX);
            asmShiftRight(ctx, RCX, RDX);
            asmRegMovAddr(ctx, RDX, R9);
            break;
        case TOKEN_AND_EQUAL:
            asmAndDeref(ctx, R9, RAX);
            asmRegMovAddr(ctx, RAX, R9);
            break;
        case TOKEN_OR_EQUAL:
            asmOrDeref(ctx, R9, RAX);
            asmRegMovAddr(ctx, RAX, R9);
            break;
        case TOKEN_XOR_EQUAL:
            asmXorDeref(ctx, R9, RAX);
            asmRegMovAddr(ctx, RAX, R9);
            break;
        default:
            printf("Unknown assignment\n");
            exit(0);
    }
}

static void x64ASTGenPostfix(ASTPostfixExpression* ast, x64Ctx* ctx) {
    x64ASTGenExpression(x64RAXLoadAddress(ast->operand), ctx);
    asmDeref(ctx, RAX, RCX);

    switch(ast->operator.type) {
        case TOKEN_PLUS_PLUS:
            if(ast->pointerShift)
                asmAddIStoreRef(ctx, 8, RAX);
            else
                asmIncDeref(ctx, RAX);
            break;
        case TOKEN_MINUS_MINUS:
            if(ast->pointerShift)
                asmSubIStoreRef(ctx, 8, RAX);
            else
                asmDecDeref(ctx, RAX);
            break;
        default:
            printf("Postfix undefined\n");
            exit(0);
    }

    asmRegMov(ctx, RCX, RAX);
}

static void x64ASTGenTernary(ASTTernaryExpression* ast, x64Ctx* ctx) {
    unsigned int elseExp = getID();
    unsigned int endExp = getID();
    x64ASTGenExpression(ast->operand1, ctx);
    asmICmp(ctx, RAX, 0);
    asmJumpCC(ctx, EQUAL, elseExp);
    x64ASTGenExpression(ast->operand2, ctx);
    asmJump(ctx, endExp);
    asmJumpTarget(ctx, elseExp);
    x64ASTGenExpression(ast->operand3, ctx);
    asmJumpTarget(ctx, endExp);
}

static Register registers[] = {
    RCX, RDX, R8, R9
};

static void x64ASTGenCall(ASTCallExpression* ast, x64Ctx* ctx) {
    bool usedAlign = false;
    if((ast->paramCount > 4 && abs(ctx->stackIndex - (ast->paramCount - 4)) % 16 != 0) || abs(ctx->stackIndex) % 16 != 0) {
        usedAlign = true;
        asmSubI(ctx, RSP, 8);
    }
    for(unsigned int i = 4; i < ast->paramCount; i++) {
        x64ASTGenExpression(ast->params[i], ctx);
        asmPush(ctx, RAX);
    }
    for(int i = ast->paramCount - 1; i >= 0; i--) {
        if(i < 4) {
            x64ASTGenExpression(ast->params[i], ctx);
            asmPush(ctx, RAX);
        }
    }
    for(unsigned int i = 0; i < ast->paramCount && i < 4; i++) {
        asmPop(ctx, registers[i]);
    }
    SymbolLocal* fn = ast->target->as.constant.local;

    asmSubI(ctx, RSP, 0x20);
    fprintf(ctx->f, "\tcall %.*s\n", fn->length, fn->name);
    // shadow space does not affect ctx->stackAlignment as it is a multiple of 16

    // if extra padding used for alignment needs cleaning up
    if(usedAlign) {
        if(ast->paramCount > 4) {
            // number of parameters passed on the stack + alignment + shadow
            asmAddI(ctx, RSP, 8 * (ast->paramCount - 4 + 1) + 0x20);
        } else {
            // only used register call
            // shadow + alignment = 32 + 8 = 40 = 0x28
            asmAddI(ctx, RSP, 0x28);
        }
    } else {
        if(ast->paramCount > 4) {
            // number of parameters passed on the stack + shadow
            asmAddI(ctx, RSP, 8 * (ast->paramCount - 4) + 0x20);
        } else {
            // only used register call
            asmAddI(ctx, RSP, 0x20);
        }
    }
    // the above allocates 0x20 on the stack because windows
    // - the stack has to be aligned to 16 bytes
    // - call adds 8 bytes, push %rbp adds 8 bytes
    // - this leaves stack aligned properly
    // - All functions must allocate 32 == 0x20 bytes of shadow space adjacent
    //   to the return address for the previous function, allocated by the
    //   caller - i.e. all functions.
    // - Uses variable ammount of stackalloc, depending on the stack alignment
    //   to ensure 16 byte alignment for function calls
    // - https://github.com/simon-whitehead/assembly-fun/tree/master/windows-x64
}

static void x64ASTGenExpression(ASTExpression* ast, x64Ctx* ctx) {
    if(ast == NULL) {
        return;
    }
    switch(ast->type) {
        case AST_EXPRESSION_CONSTANT:
            x64ASTGenConstant(&ast->as.constant, ctx);
            break;
        case AST_EXPRESSION_UNARY:
            x64ASTGenUnary(&ast->as.unary, ctx);
            break;
        case AST_EXPRESSION_BINARY:
            x64ASTGenBinary(&ast->as.binary, ctx);
            break;
        case AST_EXPRESSION_ASSIGN:
            x64ASTGenAssign(&ast->as.assign, ctx);
            break;
        case AST_EXPRESSION_POSTFIX:
            x64ASTGenPostfix(&ast->as.postfix, ctx);
            break;
        case AST_EXPRESSION_TERNARY:
            x64ASTGenTernary(&ast->as.ternary, ctx);
            break;
        case AST_EXPRESSION_CALL:
            x64ASTGenCall(&ast->as.call, ctx);
            break;
        case AST_EXPRESSION_CAST:
            x64ASTGenExpression(ast->as.cast.expression, ctx);
            break;
        default:
            printf("Output unspecified\n");
            exit(0);
    }
}

static void x64ASTGenStatement(ASTStatement* ast, x64Ctx* f);

static void x64ASTGenSelectionStatement(ASTSelectionStatement* ast, x64Ctx* ctx) {
    unsigned int elseExp = getID();
    unsigned int endExp = getID();
    x64ASTGenExpression(ast->condition, ctx);
    asmICmp(ctx, RAX, 0);
    if(ast->type == AST_SELECTION_STATEMENT_IF) {
        asmJumpCC(ctx, EQUAL, endExp);
    } else {
        asmJumpCC(ctx, EQUAL, elseExp);
    }
    x64ASTGenStatement(ast->block, ctx);
    if(ast->type == AST_SELECTION_STATEMENT_IFELSE) {
        asmJump(ctx, endExp);
        asmJumpTarget(ctx, elseExp);
        x64ASTGenStatement(ast->elseBlock, ctx);
    }
    asmJumpTarget(ctx, endExp);
}

static void x64ASTGenBlockItem(ASTBlockItem*, x64Ctx*);
static void x64ASTGenCompoundStatement(ASTCompoundStatement* ast, x64Ctx* ctx) {
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        x64ASTGenBlockItem(ast->items[i], ctx);
    }
    asmAddI(ctx, RSP, ast->popCount->localCount * 8);
}

static void x64ASTGenDeclaration(ASTDeclaration* ast, x64Ctx* ctx);
static void x64ASTGenIterationStatement(ASTIterationStatement* ast, x64Ctx* ctx) {

    unsigned int oldEnd = ctx->loopBreak;
    unsigned int end = getID();
    ctx->loopBreak = end;

    unsigned int oldContinue = ctx->loopContinue;
    unsigned int cont = getID();
    ctx->loopContinue = cont;

    switch(ast->type) {
        case AST_ITERATION_STATEMENT_WHILE: {
            asmJumpTarget(ctx, cont);
            x64ASTGenExpression(ast->control, ctx);
            asmICmp(ctx, RAX, 0);
            asmJumpCC(ctx, EQUAL, end);
            x64ASTGenStatement(ast->body, ctx);
            asmJump(ctx, cont);
            asmJumpTarget(ctx, end);
        }; break;
        case AST_ITERATION_STATEMENT_DO: {
            unsigned int start = getID();
            asmJumpTarget(ctx, start);
            x64ASTGenStatement(ast->body, ctx);
            asmJumpTarget(ctx, cont);
            x64ASTGenExpression(ast->control, ctx);
            asmICmp(ctx, RAX, 0);
            asmJumpCC(ctx, NOT_EQUAL, start);
            asmJumpTarget(ctx, end);
        }; break;
        case AST_ITERATION_STATEMENT_FOR_EXPR: {
            unsigned int cond = getID();
            x64ASTGenExpression(ast->preExpr, ctx);
            asmJumpTarget(ctx, cond);
            x64ASTGenExpression(ast->control, ctx);
            asmICmp(ctx, RAX, 0);
            asmJumpCC(ctx, EQUAL, end);
            x64ASTGenStatement(ast->body, ctx);
            asmJumpTarget(ctx, cont);
            x64ASTGenExpression(ast->post, ctx);
            asmJump(ctx, cond);
            asmJumpTarget(ctx, end);
        }; break;
        case AST_ITERATION_STATEMENT_FOR_DECL: {
            unsigned int cond = getID();
            x64ASTGenDeclaration(ast->preDecl, ctx);
            asmJumpTarget(ctx, cond);
            x64ASTGenExpression(ast->control, ctx);
            asmICmp(ctx, RAX, 0);
            asmJumpCC(ctx, EQUAL, end);
            x64ASTGenStatement(ast->body, ctx);
            asmJumpTarget(ctx, cont);
            x64ASTGenExpression(ast->post, ctx);
            asmJump(ctx, cond);
            asmJumpTarget(ctx, end);
            asmAddI(ctx, RSP, ast->freeCount->localCount * 8);
        }; break;
    }

    ctx->loopBreak = oldEnd;
    ctx->loopContinue = oldContinue;
}

static void x64ASTGenJumpStatement(ASTJumpStatement* ast, x64Ctx* ctx) {
    switch(ast->type) {
        case AST_JUMP_STATEMENT_RETURN:
            x64ASTGenExpression(ast->expr, ctx);
            asmRegMov(ctx, RBP, RSP);
            asmPop(ctx, RBP);
            asmRet(ctx);
            break;
        case AST_JUMP_STATEMENT_BREAK:
            asmJump(ctx, ctx->loopBreak);
            break;
        case AST_JUMP_STATEMENT_CONTINUE:
            asmJump(ctx, ctx->loopContinue);
            break;
    }
}

static void x64ASTGenStatement(ASTStatement* ast, x64Ctx* ctx) {
    switch(ast->type) {
        case AST_STATEMENT_JUMP:
            x64ASTGenJumpStatement(ast->as.jump, ctx);
            break;
        case AST_STATEMENT_EXPRESSION:
            x64ASTGenExpression(ast->as.expression, ctx);
            break;
        case AST_STATEMENT_SELECTION:
            x64ASTGenSelectionStatement(ast->as.selection, ctx);
            break;
        case AST_STATEMENT_COMPOUND:
            x64ASTGenCompoundStatement(ast->as.compound, ctx);
            break;
        case AST_STATEMENT_ITERATION:
            x64ASTGenIterationStatement(ast->as.iteration, ctx);
            break;
        case AST_STATEMENT_NULL:
            break;
    }
}

static void x64ASTGenGlobal(ASTInitDeclarator* ast, x64Ctx* ctx) {
    SymbolLocal* symbol = ast->declarator->symbol;
    if(ast->type == AST_INIT_DECLARATOR_NO_INITIALIZE) {
        return; // gas treats undefined symbols as extern automatically
    }
    asmGlobl(ctx, symbol->length, symbol->name);
    asmSection(ctx, "data");
    asmAlign(ctx, 8);
    asmFnName(ctx, symbol->length, symbol->name);
    asmLong(ctx, ast->initializer->as.constant.tok.numberValue);
    asmSection(ctx, "text");
}

static void x64ASTGenFunctionDefinition(ASTInitDeclarator* ast, x64Ctx* ctx);
static void x64ASTGenDeclaration(ASTDeclaration* ast, x64Ctx* ctx) {
    for(unsigned int i = 0; i < ast->declaratorCount; i++) {
        ASTInitDeclarator* a = ast->declarators[i];
        if(a->type == AST_INIT_DECLARATOR_FUNCTION) {
            x64ASTGenFunctionDefinition(a, ctx);
            return;
        }
        if(a->declarator->symbol->scopeDepth == 0) {
            x64ASTGenGlobal(a, ctx);
            return;
        }
        if(a->type == AST_INIT_DECLARATOR_INITIALIZE) {
            x64ASTGenExpression(a->initializer, ctx);
        } else {
            asmRegSet(ctx, RAX, 0xcafebabe);
        }
        a->declarator->symbol->stackOffset = ctx->stackIndex;
        asmPush(ctx, RAX);
    }
}

static void x64ASTGenBlockItem(ASTBlockItem* ast, x64Ctx* ctx) {
    switch(ast->type) {
        case AST_BLOCK_ITEM_STATEMENT:
            x64ASTGenStatement(ast->as.statement, ctx);
            break;
        case AST_BLOCK_ITEM_DECLARATION:
            x64ASTGenDeclaration(ast->as.declaration, ctx);
            break;
    }
}

static void x64ASTGenFnCompoundStatement(ASTFnCompoundStatement* ast, x64Ctx* ctx) {
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        x64ASTGenBlockItem(ast->items[i], ctx);
    }
}

static void x64ASTGenFunctionDefinition(ASTInitDeclarator* ast, x64Ctx* ctx) {
    if(ast->fn == NULL) return;

    ctx->stackIndex = 0;

    SymbolLocal* symbol = ast->declarator->symbol;
    asmGlobl(ctx, symbol->length, symbol->name);
    asmFnName(ctx, symbol->length, symbol->name);
    asmPush(ctx, RBP);
    asmRegMov(ctx, RSP, RBP);

    ASTFnCompoundStatement* s = ast->fn;

    const ASTVariableTypeFunction* fnType = &ast->declarator->variableType->as.function;
    for(unsigned int i = 0; i < fnType->paramCount && i < 4; i++) {
        fnType->params[i]->symbol->stackOffset = ctx->stackIndex;
        asmPush(ctx, registers[i]);
    }
    int stackParamIndex = 48;
    for(int i = fnType->paramCount - 1; i > 3; i--) {
        fnType->params[i]->symbol->stackOffset = stackParamIndex;
        stackParamIndex += 8;
    }

    x64ASTGenFnCompoundStatement(s, ctx);

    if(s->itemCount < 1 ||
       s->items[s->itemCount - 1]->type != AST_BLOCK_ITEM_STATEMENT ||
       s->items[s->itemCount - 1]->as.statement->type != AST_STATEMENT_JUMP ||
       s->items[s->itemCount - 1]->as.statement->as.jump->type != AST_JUMP_STATEMENT_RETURN) {
        asmRegSet(ctx, RAX, 0);
        asmRegMov(ctx, RBP, RSP);
        asmPop(ctx, RBP);
        asmRet(ctx);
    }
}

static void x64ASTGenTranslationUnit(ASTTranslationUnit* ast, x64Ctx* ctx) {
    for(unsigned int i = 0; i < ast->undefinedSymbols.entryCapacity; i++) {
        Entry* entry = &ast->undefinedSymbols.entrys[i];
        if(entry->key.key == NULL) {
            continue;
        }
        SymbolLocal* symbol = entry->value;
        asmGlobl(ctx, symbol->length, symbol->name);
        asmSection(ctx, "data");
        asmAlign(ctx, 8);
        asmComm(ctx, symbol->length, symbol->name, 8);
        asmSection(ctx, "text");
    }

    for(unsigned int i = 0; i < ast->declarationCount; i++) {
        x64ASTGenDeclaration(ast->declarations[i], ctx);
    }
}

void x64ASTGen(ASTTranslationUnit* ast) {
    x64Ctx ctx = {0};
    ctx.f = fopen("a.s", "w");
    x64ASTGenTranslationUnit(ast, &ctx);
}