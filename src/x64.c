#include "x64.h"

#include <stdio.h>
#include <stdlib.h>

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

static void x64ASTGenExpression(ASTExpression* ast, FILE* f);

static void x64ASTGenBinary(ASTBinaryExpression* ast, FILE* f) {
    switch(ast->operator.type) {
        case TOKEN_PLUS:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tadd %%rcx, %%rax\n");
            break;
        case TOKEN_NEGATE:
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tsub %%rcx, %%rax\n");
            break;
        case TOKEN_STAR:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\timul %%rcx, %%rax\n");
            break;
        case TOKEN_SLASH:
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tcqo\n"
                       "\tidiv %%rcx\n");
            break;
        case TOKEN_EQUAL_EQUAL:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tcmp %%rax, %%rcx\n"
                       "\tmov $0, %%rax\n"
                       "\tsete %%al\n");
            break;
        case TOKEN_NOT_EQUAL:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tcmp %%rax, %%rcx\n"
                       "\tmov $0, %%rax\n"
                       "\tsetne %%al\n");
            break;
        case TOKEN_LESS:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tcmp %%rax, %%rcx\n"
                       "\tmov $0, %%rax\n"
                       "\tsetl %%al\n");
            break;
        case TOKEN_LESS_EQUAL:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tcmp %%rax, %%rcx\n"
                       "\tmov $0, %%rax\n"
                       "\tsetle %%al\n");
            break;
        case TOKEN_GREATER:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tcmp %%rax, %%rcx\n"
                       "\tmov $0, %%rax\n"
                       "\tsetg %%al\n");
            break;
        case TOKEN_GREATER_EQUAL:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tcmp %%rax, %%rcx\n"
                       "\tmov $0, %%rax\n"
                       "\tsetge %%al\n");
            break;
        case TOKEN_OR_OR: {
            unsigned int clause2 = getID();
            unsigned int end = getID();
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tcmp $0, %%rax\n"
                       "\tje _%u\n"
                       "\tmov $1, %%rax\n"
                       "\tjmp _%u\n"
                       "_%u:\n", clause2, end, clause2);
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tcmp $0, %%rax\n"
                       "\tmov $0, %%rax\n"
                       "\tsetne %%al\n"
                       "_%u:\n", end);
        }; break;
        case TOKEN_AND_AND: {
            unsigned int clause2 = getID();
            unsigned int end = getID();
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tcmp $0, %%rax\n"
                       "\tjne _%u\n"
                       "\tjmp _%u\n"
                       "_%u:\n", clause2, end, clause2);
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tcmp $0, %%rax\n"
                       "\tmov $0, %%rax\n"
                       "\tsetne %%al\n"
                       "_%u:\n", end);
        }; break;
        case TOKEN_OR:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tor %%rcx, %%rax\n");
            break;
        case TOKEN_AND:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tand %%rcx, %%rax\n");
            break;
        case TOKEN_PERCENT:
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tcqo\n"
                       "\tidiv %%rcx\n"
                       "\tmov %%rdx, %%rax\n");
            break;
        case TOKEN_XOR:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\txor %%rcx, %%rax\n");
            break;
        case TOKEN_SHIFT_LEFT:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tsal %%cl, %%rax\n");
            break;
        case TOKEN_SHIFT_RIGHT:
            x64ASTGenExpression(ast->left, f);
            fprintf(f, "\tpush %%rax\n");
            x64ASTGenExpression(ast->right, f);
            fprintf(f, "\tpop %%rcx\n"
                       "\tsar %%cl, %%rax\n");
            break;
        case TOKEN_COMMA:
            x64ASTGenExpression(ast->left, f);
            x64ASTGenExpression(ast->right, f);
            break;
        default:
            printf("x64 unreachable binary\n");
            exit(0);
    }
}

static void x64ASTGenUnary(ASTUnaryExpression* ast, FILE* f) {
    switch(ast->operator.type) {
        case TOKEN_NOT:
            x64ASTGenExpression(ast->operand, f);
            fprintf(f, "\tcmp $0, %%rax\n"
                       "\tmov $0, %%rax\n"
                       "\tsete %%al\n");
            break;
        case TOKEN_NEGATE:
            x64ASTGenExpression(ast->operand, f);
            fprintf(f, "\tneg %%rax\n");
            break;
        case TOKEN_COMPLIMENT:
            x64ASTGenExpression(ast->operand, f);
            fprintf(f, "\tnot %%rax\n");
            break;
        case TOKEN_PLUS_PLUS:
            fprintf(f, "\tincq %d(%%rbp)\n"
                       "\tmov %d(%%rbp), %%rax\n", ast->local->stackOffset, ast->local->stackOffset);
            break;
        case TOKEN_MINUS_MINUS:
        fprintf(f, "\tdecq %d(%%rbp)\n"
                       "\tmov %d(%%rbp), %%rax\n", ast->local->stackOffset, ast->local->stackOffset);
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
        case AST_CONSTANT_EXPRESSION_GLOBAL:
            fprintf(f, "\tmov %.*s, %%rax\n", ast->global->length, ast->global->name);
            break;
    }
}

static void x64ASTGenAssign(ASTAssignExpression* ast, FILE* f) {
    x64ASTGenExpression(ast->value, f);
    switch(ast->operator.type) {
        case TOKEN_EQUAL:
            fprintf(f, "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset);
            break;
        case TOKEN_PLUS_EQUAL:
            fprintf(f, "\tmov %%rax, %%rcx\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\tadd %%rcx, %%rax\n"
                       "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset, ast->target->stackOffset);
            break;
        case TOKEN_MINUS_EQUAL:
            fprintf(f, "\tmov %%rax, %%rcx\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\tsub %%rcx, %%rax\n"
                       "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset, ast->target->stackOffset);
            break;
        case TOKEN_SLASH_EQUAL:
            fprintf(f, "\tmov %%rax, %%rcx\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\tcqo\n"
                       "\tidiv %%rcx\n"
                       "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset, ast->target->stackOffset);
            break;
        case TOKEN_STAR_EQUAL:
            fprintf(f, "\tmov %%rax, %%rcx\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\timul %%rcx, %%rax\n"
                       "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset, ast->target->stackOffset);
            break;
        case TOKEN_PERCENT_EQUAL:
            fprintf(f, "\tmov %%rax, %%rcx\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\tcqo\n"
                       "\tidiv %%rcx\n"
                       "\tmov %%rdx, %%rax\n"
                       "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset, ast->target->stackOffset);
            break;
        case TOKEN_LEFT_SHIFT_EQUAL:
            fprintf(f, "\tmov %%rax, %%rcx\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\tsal %%cl, %%rax\n"
                       "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset, ast->target->stackOffset);
            break;
        case TOKEN_RIGHT_SHIFT_EQUAL:
            fprintf(f, "\tmov %%rax, %%rcx\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\tsar %%cl, %%rax\n"
                       "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset, ast->target->stackOffset);
            break;
        case TOKEN_AND_EQUAL:
            fprintf(f, "\tmov %%rax, %%rcx\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\tand %%rcx, %%rax\n"
                       "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset, ast->target->stackOffset);
            break;
        case TOKEN_OR_EQUAL:
            fprintf(f, "\tmov %%rax, %%rcx\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\tor %%rcx, %%rax\n"
                       "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset, ast->target->stackOffset);
            break;
        case TOKEN_XOR_EQUAL:
            fprintf(f, "\tmov %%rax, %%rcx\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\txor %%rcx, %%rax\n"
                       "\tmov %%rax, %d(%%rbp)\n", ast->target->stackOffset, ast->target->stackOffset);
            break;
        default:
            printf("Unknown assignment\n");
            exit(0);
    }
}

static void x64ASTGenPostfix(ASTPostfixExpression* ast, FILE* f) {
    switch(ast->operator.type) {
        case TOKEN_PLUS_PLUS:
            fprintf(f, "\tincq %d(%%rbp)\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\tdec %%rax\n", ast->local->stackOffset, ast->local->stackOffset);
            break;
        case TOKEN_MINUS_MINUS:
            fprintf(f, "\tdecq %d(%%rbp)\n"
                       "\tmov %d(%%rbp), %%rax\n"
                       "\tinc %%rax\n", ast->local->stackOffset, ast->local->stackOffset);
            break;
        default:
            printf("Postfix undefined\n");
            exit(0);
    }
}

static void x64ASTGenTernary(ASTTernaryExpression* ast, FILE* f) {
    unsigned int elseExp = getID();
    unsigned int endExp = getID();
    x64ASTGenExpression(ast->operand1, f);
    fprintf(f, "\tcmp $0, %%rax\n"
               "\tje _%u\n", elseExp);
    x64ASTGenExpression(ast->operand2, f);
    fprintf(f, "\tjmp _%u\n"
               "_%u:\n", endExp, elseExp);
    x64ASTGenExpression(ast->operand3, f);
    fprintf(f, "_%u:\n", endExp);
}

static void x64ASTGenCall(ASTCallExpression* ast, FILE* f) {
    for(int i = ast->paramCount - 1; i >= 0; i--) {
        x64ASTGenExpression(ast->params[i], f);
        fprintf(f, "\tpush %%rax\n");
    }
    SymbolGlobal* fn = ast->target->as.constant.global;
    fprintf(f, "\tcall %.*s\n"
               "\tadd $%u, %%rsp\n",
               fn->length, fn->name,
               ast->paramCount * 8);
}

static void x64ASTGenExpression(ASTExpression* ast, FILE* f) {
    if(ast == NULL) {
        return;
    }
    switch(ast->type) {
        case AST_EXPRESSION_CONSTANT:
            x64ASTGenConstant(&ast->as.constant, f);
            break;
        case AST_EXPRESSION_UNARY:
            x64ASTGenUnary(&ast->as.unary, f);
            break;
        case AST_EXPRESSION_BINARY:
            x64ASTGenBinary(&ast->as.binary, f);
            break;
        case AST_EXPRESSION_ASSIGN:
            x64ASTGenAssign(&ast->as.assign, f);
            break;
        case AST_EXPRESSION_POSTFIX:
            x64ASTGenPostfix(&ast->as.postfix, f);
            break;
        case AST_EXPRESSION_TERNARY:
            x64ASTGenTernary(&ast->as.ternary, f);
            break;
        case AST_EXPRESSION_CALL:
            x64ASTGenCall(&ast->as.call, f);
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
    x64ASTGenExpression(ast->condition, ctx->f);
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
            x64ASTGenExpression(ast->control, ctx->f);
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
            x64ASTGenExpression(ast->control, ctx->f);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                            "\tjne _%u\n"
                            "_%u:\n", start, end);
        }; break;
        case AST_ITERATION_STATEMENT_FOR_EXPR: {
            unsigned int cond = getID();
            x64ASTGenExpression(ast->preExpr, ctx->f);
            fprintf(ctx->f, "_%u:\n", cond);
            x64ASTGenExpression(ast->control, ctx->f);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                            "\tje _%u\n", end);
            x64ASTGenStatement(ast->body, ctx);
            fprintf(ctx->f, "_%u:\n", cont);
            x64ASTGenExpression(ast->post, ctx->f);
            fprintf(ctx->f, "\tjmp _%u\n"
                            "_%u:\n", cond, end);
        }; break;
        case AST_ITERATION_STATEMENT_FOR_DECL: {
            unsigned int cond = getID();
            x64ASTGenDeclaration(ast->preDecl, ctx);
            fprintf(ctx->f, "_%u:\n", cond);
            x64ASTGenExpression(ast->control, ctx->f);
            fprintf(ctx->f, "\tcmp $0, %%rax\n"
                       "\tje _%u\n", end);
            x64ASTGenStatement(ast->body, ctx);
            fprintf(ctx->f, "_%u:\n", cont);
            x64ASTGenExpression(ast->post, ctx->f);
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
            x64ASTGenExpression(ast->expr, ctx->f);
            fprintf(ctx->f, "\tmov %%rbp, %%rsp\n"
                            "\tpop %%rbp\n"
                            "\tret\n\n");
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
            x64ASTGenExpression(ast->as.expression, ctx->f);
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

static void x64ASTGenDeclaration(ASTDeclaration* ast, x64Ctx* ctx) {
    for(unsigned int i = 0; i < ast->declarators.declaratorCount; i++) {
        ASTInitDeclarator* a = ast->declarators.declarators[i];
        if(a->type == AST_INIT_DECLARATOR_INITIALIZE) {
            x64ASTGenExpression(a->initializer, ctx->f);
        } else {
            fprintf(ctx->f, "\tmov $0xcafebabe, %%rax\n");
        }
        fprintf(ctx->f, "\tpush %%rax\n");
        a->declarator->stackOffset = ctx->stackIndex;
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

static void x64ASTGenFunctionDefinition(ASTFunctionDefinition* ast, x64Ctx* ctx) {
    fprintf(ctx->f, ".globl %.*s\n", ast->name.length, ast->name.start);
    fprintf(ctx->f, "%.*s:\n"
                    "\tpush %%rbp\n"
                    "\tmov %%rsp, %%rbp\n", ast->name.length, ast->name.start);

    ASTFnCompoundStatement* s = ast->statement;
    ctx->stackIndex = -8;

    int paramOffset = 8;
    for(unsigned int i = 0; i < ast->paramCount; i++) {
        ast->params[i]->declarator->stackOffset = paramOffset;
        paramOffset += 8;
    }
    x64ASTGenFnCompoundStatement(s, ctx);
    if(s->items[s->itemCount - 1]->type != AST_BLOCK_ITEM_STATEMENT ||
       s->items[s->itemCount - 1]->as.statement->type != AST_STATEMENT_JUMP ||
       s->items[s->itemCount - 1]->as.statement->as.jump->type != AST_JUMP_STATEMENT_RETURN) {
        fprintf(ctx->f, "\tmov $0, %%rax\n"
                        "\tmov %%rbp, %%rsp\n"
                        "\tpop %%rbp\n"
                        "\tret\n\n");
    }
}

static void x64ASTGenExternalDeclaration(ASTExternalDeclaration* ast, x64Ctx* ctx) {
    switch(ast->type) {
        case AST_EXTERNAL_DECLARATION_FUNCTION_DEFINITION:
            x64ASTGenFunctionDefinition(ast->as.functionDefinition, ctx);
    }
}

static void x64ASTGenTranslationUnit(ASTTranslationUnit* ast, x64Ctx* ctx) {
    for(unsigned int i = 0; i < ast->declarationCount; i++) {
        x64ASTGenExternalDeclaration(ast->declarations[i], ctx);
    }
}

void x64ASTGen(ASTTranslationUnit* ast) {
    x64Ctx ctx = {0};
    ctx.f = fopen("a.s", "w");
    x64ASTGenTranslationUnit(ast, &ctx);
}