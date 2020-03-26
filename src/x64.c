#include "x64.h"

#include <stdio.h>
#include <stdlib.h>

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
        default:
            printf("x64 unreachable unary\n");
            exit(0);
    }
}

static void x64ASTGenConstant(ASTConstantExpression* ast, FILE* f) {
    switch(ast->type) {
        case AST_CONSTANT_EXPRESSION_VARIABLE: {
            fprintf(f, "\tmov %d(%%rbp), %%rax\n", ast->stackDepth);
        }; break;
        case AST_CONSTANT_EXPRESSION_INTEGER:
            fprintf(f, "\tmov $%i, %%rax\n", ast->tok.numberValue);
            break;
    }
}

static void x64ASTGenAssign(ASTAssignExpression* ast, FILE* f) {
    x64ASTGenExpression(ast->value, f);
    fprintf(f, "\tmov %%rax, %d(%%rbp)\n", ast->stackOffset);
}

static void x64ASTGenExpression(ASTExpression* ast, FILE* f) {
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
        default:
            printf("Output unspecified\n");
            exit(0);
    }
}

static void x64ASTGenStatement(ASTStatement* ast, FILE* f) {
    switch(ast->type) {
        case AST_STATEMENT_RETURN:
            x64ASTGenExpression(ast->as.return_, f);
            fprintf(f, "\tmov %%rbp, %%rsp\n"
                       "\tpop %%rbp\n"
                       "\tret\n\n");
            break;
        case AST_STATEMENT_EXPRESSION:
            x64ASTGenExpression(ast->as.expression, f);
            break;
    }
}

static void x64ASTGenDeclaration(ASTDeclaration* ast, FILE* f) {
    for(unsigned int i = 0; i < ast->declarators.declaratorCount; i++) {
        ASTInitDeclarator* a = ast->declarators.declarators[i];
        if(a->type == AST_INIT_DECLARATOR_INITIALIZE) {
            x64ASTGenExpression(a->initializer, f);
        } else {
            fprintf(f, "\tmov $0xcafebabe, %%rax\n");
        }
        fprintf(f, "\tpush %%rax\n");
    }
}

static void x64ASTGenBlockItem(ASTBlockItem* ast, FILE* f) {
    switch(ast->type) {
        case AST_BLOCK_ITEM_STATEMENT:
            x64ASTGenStatement(ast->as.statement, f);
            break;
        case AST_BLOCK_ITEM_DECLARATION:
            x64ASTGenDeclaration(ast->as.declaration, f);
            break;
    }
}

static void x64ASTGenCompoundStatement(ASTCompoundStatement* ast, FILE* f) {
    for(unsigned int i = 0; i < ast->itemCount; i++) {
        x64ASTGenBlockItem(ast->items[i], f);
    }
}

static void x64ASTGenFunctionDefinition(ASTFunctionDefinition* ast, FILE* f) {
    fprintf(f, ".globl %.*s\n", ast->name.length, ast->name.start);
    fprintf(f, "%.*s:\n"
               "\tpush %%rbp\n"
               "\tmov %%rsp, %%rbp\n", ast->name.length, ast->name.start);

    ASTCompoundStatement* s = ast->statement;
    x64ASTGenCompoundStatement(s, f);
    if(s->items[s->itemCount - 1]->type != AST_BLOCK_ITEM_STATEMENT &&
       s->items[s->itemCount - 1]->as.statement->type != AST_STATEMENT_RETURN) {
        fprintf(f, "\tmov $0, %%rax\n"
                   "\tmov %%rbp, %%rsp\n"
                   "\tpop %%rbp\n"
                   "\tret\n\n");
    }
}

static void x64ASTGenExternalDeclaration(ASTExternalDeclaration* ast, FILE* f) {
    switch(ast->type) {
        case AST_EXTERNAL_DECLARATION_FUNCTION_DEFINITION:
            x64ASTGenFunctionDefinition(ast->as.functionDefinition, f);
    }
}

static void x64ASTGenTranslationUnit(ASTTranslationUnit* ast, FILE* f) {
    for(unsigned int i = 0; i < ast->declarationCount; i++) {
        x64ASTGenExternalDeclaration(ast->declarations[i], f);
    }
}

void x64ASTGen(ASTTranslationUnit* ast) {
    x64ASTGenTranslationUnit(ast, fopen("a.s", "w"));
}