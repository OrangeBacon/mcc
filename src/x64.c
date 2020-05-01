#include "x64.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct x64Ctx {
    // file to write the assembly to
    FILE* f;

    // location break statements should jump to
    unsigned int loopBreak;

    // location continue statements should jump to
    unsigned int loopContinue;

    // distance to top of stack
    int stackIndex;
} x64Ctx;

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
                fprintf(ctx->f, "\tshl $3, %%rax\n");
            }
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            if(ast->pointerShift && ast->right->exprType->type != AST_VARIABLE_TYPE_POINTER) {
                fprintf(ctx->f, "\tshl $3, %%rax\n");
            }
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tadd %%rcx, %%rax\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_NEGATE:
            x64ASTGenExpression(ast->right, ctx);
            if(ast->pointerShift && ast->right->exprType->type != AST_VARIABLE_TYPE_POINTER) {
                fprintf(ctx->f, "\tshl $3, %%rax\n");
            }
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->left, ctx);
            if(ast->pointerShift && ast->left->exprType->type != AST_VARIABLE_TYPE_POINTER) {
                fprintf(ctx->f, "\tshl $3, %%rax\n");
            }
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tsub %%rcx, %%rax\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_STAR:
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\timul %%rcx, %%rax\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_SLASH:
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tcqo\n"
                            "\tidiv %%rcx\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_EQUAL_EQUAL:
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tcmp %%rax, %%rcx\n"
                            "\tmov $0, %%rax\n"
                            "\tsete %%al\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_NOT_EQUAL:
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tcmp %%rax, %%rcx\n"
                            "\tmov $0, %%rax\n"
                            "\tsetne %%al\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_LESS:
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tcmp %%rax, %%rcx\n"
                            "\tmov $0, %%rax\n"
                            "\tsetl %%al\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_LESS_EQUAL:
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tcmp %%rax, %%rcx\n"
                            "\tmov $0, %%rax\n"
                            "\tsetle %%al\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_GREATER:
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tcmp %%rax, %%rcx\n"
                            "\tmov $0, %%rax\n"
                            "\tsetg %%al\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_GREATER_EQUAL:
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tcmp %%rax, %%rcx\n"
                            "\tmov $0, %%rax\n"
                            "\tsetge %%al\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_OR_OR: {
            unsigned int clause2 = getID();
            unsigned int end = getID();
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                            "\tje _%u\n"
                            "\tmov $1, %%rax\n"
                            "\tjmp _%u\n"
                            "_%u:\n", clause2, end, clause2);
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                            "\tmov $0, %%rax\n"
                            "\tsetne %%al\n"
                            "_%u:\n", end);
        }; break;
        case TOKEN_AND_AND: {
            unsigned int clause2 = getID();
            unsigned int end = getID();
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                            "\tjne _%u\n"
                            "\tjmp _%u\n"
                            "_%u:\n", clause2, end, clause2);
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                            "\tmov $0, %%rax\n"
                            "\tsetne %%al\n"
                            "_%u:\n", end);
        }; break;
        case TOKEN_OR:
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tor %%rcx, %%rax\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_AND:
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tand %%rcx, %%rax\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_PERCENT:
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tcqo\n"
                            "\tidiv %%rcx\n"
                            "\tmov %%rdx, %%rax\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_XOR:
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\txor %%rcx, %%rax\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_SHIFT_LEFT:
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tsal %%cl, %%rax\n");
            ctx->stackIndex += 8;
            break;
        case TOKEN_SHIFT_RIGHT:
            x64ASTGenExpression(ast->right, ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
            x64ASTGenExpression(ast->left, ctx);
            fprintf(ctx->f, "\tpop %%rcx\n"
                            "\tsar %%cl, %%rax\n");
            ctx->stackIndex += 8;
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
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                       "\tmov $0, %%rax\n"
                       "\tsete %%al\n");
            break;
        case TOKEN_NEGATE:
            x64ASTGenExpression(ast->operand, ctx);
            fprintf(ctx->f, "\tneg %%rax\n");
            break;
        case TOKEN_COMPLIMENT:
            x64ASTGenExpression(ast->operand, ctx);
            fprintf(ctx->f, "\tnot %%rax\n");
            break;
        case TOKEN_AND:
            fprintf(ctx->f, "\tlea %d(%%rbp), %%rax\n",
                ast->operand->as.constant.local->stackOffset);
            break;
        case TOKEN_STAR:
            x64ASTGenExpression(ast->operand, ctx);
            fprintf(ctx->f, "\tmov (%%rax), %%rax\n");
            break;
        default:
            printf("x64 unreachable unary\n");
            exit(0);
    }
}

static void x64ASTGenConstant(ASTConstantExpression* ast, FILE* f) {
    switch(ast->type) {
        case AST_CONSTANT_EXPRESSION_LOCAL: {
            fprintf(f, "\tmov %d(%%rbp), %%rax\n", ast->local->stackOffset);
        }; break;
        case AST_CONSTANT_EXPRESSION_INTEGER:
            fprintf(f, "\tmov $%i, %%rax\n", ast->tok.numberValue);
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
    fprintf(ctx->f, "\tpush %%rax\n");
    ctx->stackIndex -= 8;

    x64ASTGenExpression(ast->value, ctx);
    if(ast->pointerShift) {
        fprintf(ctx->f, "\tshl $3, %%rax\n");
    }

    fprintf(ctx->f, "\tpop %%r9\n");
    ctx->stackIndex += 8;

    // value to store/operate on is in rax
    // address to store value to is in r9
    // rcx is required by shift
    // rdx is required by division
    // r8  is required by division

    switch(ast->operator.type) {
        case TOKEN_EQUAL:
            fprintf(ctx->f, "\tmov %%rax, (%%r9)\n");
            break;
        case TOKEN_PLUS_EQUAL:
            fprintf(ctx->f, "\tadd (%%r9), %%rax\n"
                            "\tmov %%rax, (%%r9)\n");
            break;
        case TOKEN_MINUS_EQUAL:
            fprintf(ctx->f, "\tmov (%%r9), %%rdx\n"
                            "\tsub %%rax, %%rdx\n"
                            "\tmov %%rdx, (%%r9)\n");
            break;
        case TOKEN_SLASH_EQUAL:
            fprintf(ctx->f, "\tmov %%rax, %%r8\n"
                            "\tmov (%%r9), %%rax\n"
                            "\tcqo\n"
                            "\tidiv %%r8\n"
                            "\tmov %%rax, (%%r9)\n");
            break;
        case TOKEN_STAR_EQUAL:
            fprintf(ctx->f, "\timul (%%r9), %%rax\n"
                            "\tmov %%rax, (%%r9)\n");
            break;
        case TOKEN_PERCENT_EQUAL:
            fprintf(ctx->f, "\tmov %%rax, %%r8\n"
                            "\tmov (%%r9), %%rax\n"
                            "\tcqo\n"
                            "\tidiv %%r8\n"
                            "\tmov %%rdx, %%rax\n"
                            "\tmov %%rax, (%%r9)\n");
            break;
        case TOKEN_LEFT_SHIFT_EQUAL:
            fprintf(ctx->f, "\tmov (%%r9), %%rdx\n"
                            "\tmov %%rax, %%rcx\n"
                            "\tsal %%cl, %%rdx\n"
                            "\tmov %%rdx, (%%r9)\n");
            break;
        case TOKEN_RIGHT_SHIFT_EQUAL:
            fprintf(ctx->f, "\tmov (%%r9), %%rdx\n"
                            "\tmov %%rax, %%rcx\n"
                            "\tsar %%cl, %%rdx\n"
                            "\tmov %%rdx, (%%r9)\n");
            break;
        case TOKEN_AND_EQUAL:
            fprintf(ctx->f, "\tand (%%r9), %%rax\n"
                            "\tmov %%rax, (%%r9)\n");
            break;
        case TOKEN_OR_EQUAL:
            fprintf(ctx->f, "\tor (%%r9), %%rax\n"
                            "\tmov %%rax, (%%r9)\n");
            break;
        case TOKEN_XOR_EQUAL:
            fprintf(ctx->f, "\txor (%%r9), %%rax\n"
                            "\tmov %%rax, (%%r9)\n");
            break;
        default:
            printf("Unknown assignment\n");
            exit(0);
    }
}

static void x64ASTGenPostfix(ASTPostfixExpression* ast, x64Ctx* ctx) {
    x64ASTGenExpression(x64RAXLoadAddress(ast->operand), ctx);
    fprintf(ctx->f, "\tmov (%%rax), %%rcx\n");

    switch(ast->operator.type) {
        case TOKEN_PLUS_PLUS:
            if(ast->pointerShift)
                fprintf(ctx->f, "\tadd $8, (%%rax)\n");
            else
                fprintf(ctx->f, "\tincq (%%rax)\n");
            break;
        case TOKEN_MINUS_MINUS:
            if(ast->pointerShift)
                fprintf(ctx->f, "\tsub $8, (%%rax)\n");
            else
                fprintf(ctx->f, "\tdecq (%%rax)\n");
            break;
        default:
            printf("Postfix undefined\n");
            exit(0);
    }
    fprintf(ctx->f, "\tmov %%rcx, %%rax\n");
}

static void x64ASTGenTernary(ASTTernaryExpression* ast, x64Ctx* ctx) {
    unsigned int elseExp = getID();
    unsigned int endExp = getID();
    x64ASTGenExpression(ast->operand1, ctx);
    fprintf(ctx->f, "\tcmp $0, %%rax\n"
                    "\tje _%u\n", elseExp);
    x64ASTGenExpression(ast->operand2, ctx);
    fprintf(ctx->f, "\tjmp _%u\n"
                    "_%u:\n", endExp, elseExp);
    x64ASTGenExpression(ast->operand3, ctx);
    fprintf(ctx->f, "_%u:\n", endExp);
}

static char* registers[] = {
    "rcx", "rdx", "r8", "r9"
};

static void x64ASTGenCall(ASTCallExpression* ast, x64Ctx* ctx) {
    bool usedAlign = false;
    if((ast->paramCount > 4 && abs(ctx->stackIndex - (ast->paramCount - 4)) % 16 != 0) || abs(ctx->stackIndex) % 16 != 0) {
        usedAlign = true;
        fprintf(ctx->f, "\tsub $8, %%rsp\n");
        ctx->stackIndex -= 8;
    }
    for(unsigned int i = 4; i < ast->paramCount; i++) {
        x64ASTGenExpression(ast->params[i], ctx);
        fprintf(ctx->f, "\tpush %%rax\n");
        ctx->stackIndex -= 8;
    }
    for(int i = ast->paramCount - 1; i >= 0; i--) {
        if(i < 4) {
            x64ASTGenExpression(ast->params[i], ctx);
            fprintf(ctx->f, "\tpush %%rax\n");
            ctx->stackIndex -= 8;
        }
    }
    for(unsigned int i = 0; i < ast->paramCount && i < 4; i++) {
        fprintf(ctx->f, "\tpop %%%s\n", registers[i]);
        ctx->stackIndex += 8;
    }
    SymbolLocal* fn = ast->target->as.constant.local;

    fprintf(ctx->f, "\tsub $0x20, %%rsp\n"
                    "\tcall %.*s\n", fn->length, fn->name);
    // shadow space does not affect ctx->stackAlignment as it is a multiple of 16

    // if extra padding used for alignment needs cleaning up
    if(usedAlign) {
        if(ast->paramCount > 4) {
            // number of parameters passed on the stack + alignment + shadow
            ctx->stackIndex += 8 * (ast->paramCount - 4 + 1);
            fprintf(ctx->f, "\tadd $%d, %%rsp\n", 8 * (ast->paramCount - 4 + 1) + 0x20);
        } else {
            // only used register call
            // shadow + alignment = 32 + 8 = 40 = 0x28
            ctx->stackIndex += 8;
            fprintf(ctx->f, "\tadd $0x28, %%rsp\n");
        }
    } else {
        if(ast->paramCount > 4) {
            // number of parameters passed on the stack + shadow
            ctx->stackIndex += 8 * (ast->paramCount - 4);
            fprintf(ctx->f, "\tadd $%d, %%rsp\n", 8 * (ast->paramCount - 4) + 0x20);
        } else {
            // only used register call
            fprintf(ctx->f, "\tadd $0x20, %%rsp\n");
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
            x64ASTGenConstant(&ast->as.constant, ctx->f);
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
    fprintf(ctx->f, "\tcmp $0, %%rax\n");
    if(ast->type == AST_SELECTION_STATEMENT_IF) {
        fprintf(ctx->f, "\tje _%u\n", endExp);
    } else {
        fprintf(ctx->f, "\tje _%u\n", elseExp);
    }
    x64ASTGenStatement(ast->block, ctx);
    if(ast->type == AST_SELECTION_STATEMENT_IFELSE) {
        fprintf(ctx->f, "\tjmp _%u\n"
                   "_%u:\n", endExp, elseExp);
        x64ASTGenStatement(ast->elseBlock, ctx);
    }
    fprintf(ctx->f, "_%u:\n", endExp);
}

static void x64ASTGenBlockItem(ASTBlockItem*, x64Ctx*);
static void x64ASTGenCompoundStatement(ASTCompoundStatement* ast, x64Ctx* ctx) {
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        x64ASTGenBlockItem(ast->items[i], ctx);
    }
    fprintf(ctx->f, "\tadd $%u, %%rsp\n", ast->popCount->localCount * 8);
    ctx->stackIndex += 8 * ast->popCount->localCount;
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
            fprintf(ctx->f, "_%u:\n", cont);
            x64ASTGenExpression(ast->control, ctx);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                            "\tje _%u\n", end);
            x64ASTGenStatement(ast->body, ctx);
            fprintf(ctx->f, "\tjmp _%u\n"
                            "_%u:\n", cont, end);
        }; break;
        case AST_ITERATION_STATEMENT_DO: {
            unsigned int start = getID();
            fprintf(ctx->f, "_%u:\n", start);
            x64ASTGenStatement(ast->body, ctx);
            fprintf(ctx->f, "_%u:\n", cont);
            x64ASTGenExpression(ast->control, ctx);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                            "\tjne _%u\n"
                            "_%u:\n", start, end);
        }; break;
        case AST_ITERATION_STATEMENT_FOR_EXPR: {
            unsigned int cond = getID();
            x64ASTGenExpression(ast->preExpr, ctx);
            fprintf(ctx->f, "_%u:\n", cond);
            x64ASTGenExpression(ast->control, ctx);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                            "\tje _%u\n", end);
            x64ASTGenStatement(ast->body, ctx);
            fprintf(ctx->f, "_%u:\n", cont);
            x64ASTGenExpression(ast->post, ctx);
            fprintf(ctx->f, "\tjmp _%u\n"
                            "_%u:\n", cond, end);
        }; break;
        case AST_ITERATION_STATEMENT_FOR_DECL: {
            unsigned int cond = getID();
            x64ASTGenDeclaration(ast->preDecl, ctx);
            fprintf(ctx->f, "_%u:\n", cond);
            x64ASTGenExpression(ast->control, ctx);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                       "\tje _%u\n", end);
            x64ASTGenStatement(ast->body, ctx);
            fprintf(ctx->f, "_%u:\n", cont);
            x64ASTGenExpression(ast->post, ctx);
            fprintf(ctx->f, "\tjmp _%u\n"
                       "_%u:\n"
                       "\tadd $%u, %%rsp\n", cond, end, ast->freeCount->localCount * 8);
            ctx->stackIndex += ast->freeCount->localCount * 8;
        }; break;
    }

    ctx->loopBreak = oldEnd;
    ctx->loopContinue = oldContinue;
}

static void x64ASTGenJumpStatement(ASTJumpStatement* ast, x64Ctx* ctx) {
    switch(ast->type) {
        case AST_JUMP_STATEMENT_RETURN:
            x64ASTGenExpression(ast->expr, ctx);
            fprintf(ctx->f, "\tmov %%rbp, %%rsp\n"
                            "\tpop %%rbp\n"
                            "\tret\n\n");
            ctx->stackIndex += 8;
            break;
        case AST_JUMP_STATEMENT_BREAK:
            fprintf(ctx->f, "\tjmp _%u # break\n", ctx->loopBreak);
            break;
        case AST_JUMP_STATEMENT_CONTINUE:
            fprintf(ctx->f, "\tjmp _%u\n", ctx->loopContinue);
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

static void x64ASTGenFunctionDefinition(ASTInitDeclarator* ast, x64Ctx* ctx);

static void x64ASTGenDeclaration(ASTDeclaration* ast, x64Ctx* ctx) {
    for(unsigned int i = 0; i < ast->declaratorCount; i++) {
        ASTInitDeclarator* a = ast->declarators[i];
        if(a->type == AST_INIT_DECLARATOR_FUNCTION) {
            x64ASTGenFunctionDefinition(a, ctx);
            return;
        }
        if(a->type == AST_INIT_DECLARATOR_INITIALIZE) {
            x64ASTGenExpression(a->initializer, ctx);
        } else {
            fprintf(ctx->f, "\tmov $0xcafebabe, %%rax\n");
        }
        fprintf(ctx->f, "\tpush %%rax\n");
        a->declarator->declarator->stackOffset = ctx->stackIndex;
        ctx->stackIndex -= 8;
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

    SymbolLocal* symbol = ast->declarator->declarator;
    fprintf(ctx->f, ".globl %.*s\n", symbol->length, symbol->name);
    fprintf(ctx->f, "%.*s:\n"
                    "\tpush %%rbp\n"
                    "\tmov %%rsp, %%rbp\n", symbol->length, symbol->name);

    ASTFnCompoundStatement* s = ast->fn;
    ctx->stackIndex = -8;

    const ASTVariableTypeFunction* fnType = &ast->declarator->variableType->as.function;
    for(unsigned int i = 0; i < fnType->paramCount && i < 4; i++) {
        fnType->params[i]->declarator->stackOffset = ctx->stackIndex;
        ctx->stackIndex -= 8;
        fprintf(ctx->f, "\tpush %%%s\n", registers[i]);
        ctx->stackIndex -= 8;
    }
    int stackParamIndex = 48;
    for(int i = fnType->paramCount - 1; i > 3; i--) {
        fnType->params[i]->declarator->stackOffset = stackParamIndex;
        stackParamIndex += 8;
    }

    x64ASTGenFnCompoundStatement(s, ctx);

    if(s->itemCount < 1 ||
       s->items[s->itemCount - 1]->type != AST_BLOCK_ITEM_STATEMENT ||
       s->items[s->itemCount - 1]->as.statement->type != AST_STATEMENT_JUMP ||
       s->items[s->itemCount - 1]->as.statement->as.jump->type != AST_JUMP_STATEMENT_RETURN) {
        fprintf(ctx->f, "\tmov $0, %%rax\n"
                        "\tmov %%rbp, %%rsp\n"
                        "\tpop %%rbp\n"
                        "\tret\n\n");
    }
}

static void x64ASTGenTranslationUnit(ASTTranslationUnit* ast, x64Ctx* ctx) {
    for(unsigned int i = 0; i < ast->declarationCount; i++) {
        x64ASTGenDeclaration(ast->declarations[i], ctx);
    }
}

void x64ASTGen(ASTTranslationUnit* ast) {
    x64Ctx ctx = {0};
    ctx.f = fopen("a.s", "w");
    x64ASTGenTranslationUnit(ast, &ctx);
}