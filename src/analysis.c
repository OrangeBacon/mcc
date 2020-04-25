#include "analysis.h"
#include <stdlib.h>

typedef struct ctx {
    Parser* parser;
    bool inLoop;

    const ASTVariableType* currentFn;
} ctx;

// TODO - major saftey improvements - this allows far too much through
// treats c as an untyped language!

static const ASTVariableType defaultInt = {
    .type = AST_VARIABLE_TYPE_INT,
    .token = {
        .type = TOKEN_INT,
        .start = "internal",
        .length = 9,
        .column = -1,
        .line = -1,
        .numberValue = -1,
    }
};

static bool TypeCompat(const ASTVariableType* a, const ASTVariableType* b) {
    if(a->type != b->type) return false;

    switch(a->type) {
        case AST_VARIABLE_TYPE_INT:
            return true;
        case AST_VARIABLE_TYPE_POINTER:
            return TypeCompat(a->as.pointer, b->as.pointer);
        case AST_VARIABLE_TYPE_FUNCTION:
            if(!TypeCompat(a->as.function.ret, b->as.function.ret)) {
                return false;
            }

            if((a->as.function.paramCount > 0 &&
                b->as.function.paramCount == 0 &&
                !b->as.function.isFromDefinition) ||
               (b->as.function.paramCount > 0 &&
                a->as.function.paramCount == 0 &&
                !a->as.function.isFromDefinition)) {
                return true;
            } else if(a->as.function.paramCount != b->as.function.paramCount) {
                return false;
            } else {
                for(unsigned int i = 0; i < a->as.function.paramCount; i++) {
                    if(!TypeCompat(a->as.function.params[i]->variableType, b->as.function.params[i]->variableType)) {
                        return false;
                    }
                }
                return true;
            }
    }

    // unreachable
    exit(1);
}

static const ASTVariableType* TypeComposite(const ASTVariableType* base, const ASTVariableType* apply) {
    // assumes TypeCompat(base, apply) == true

    switch(base->type) {
        case AST_VARIABLE_TYPE_INT:
            return &defaultInt;
        case AST_VARIABLE_TYPE_POINTER: {
            const ASTVariableType* inner = TypeComposite(base->as.pointer, apply->as.pointer);
            ASTVariableType* ptr = ArenaAlloc(sizeof(*ptr));
            ptr->token = TokenMake(TOKEN_STAR);
            ptr->type = AST_VARIABLE_TYPE_POINTER;
            ptr->as.pointer = inner;
            return ptr;
        }; break;
        case AST_VARIABLE_TYPE_FUNCTION: {
            const ASTVariableTypeFunction* bfn = &base->as.function;
            const ASTVariableTypeFunction* afn = &apply->as.function;

            ASTVariableType* fn = ArenaAlloc(sizeof*fn);
            fn->token = TokenMake(TOKEN_LEFT_PAREN);
            fn->type = AST_VARIABLE_TYPE_FUNCTION;
            fn->as.function.isFromDefinition = bfn->isFromDefinition || afn->isFromDefinition;
            fn->as.function.ret = TypeComposite(bfn->ret, afn->ret);
            ARRAY_ALLOC(const ASTVariableType*, fn->as.function, param);

            if(afn->paramCount == 0 && !afn->isFromDefinition) {
                for(unsigned int i = 0; i < bfn->paramCount; i++) {
                    ARRAY_PUSH(fn->as.function, param, bfn->params[i]);
                }
            } else if(bfn->paramCount == 0 && !bfn->isFromDefinition) {
                for(unsigned int i = 0; i < afn->paramCount; i++) {
                    ARRAY_PUSH(fn->as.function, param, afn->params[i]);
                }
            } else {
                // param lengths same, as typecompat = true
                for(unsigned int i = 0; i < afn->paramCount; i++) {
                    const ASTVariableType* param = TypeComposite(
                        afn->params[i]->variableType,
                        bfn->params[i]->variableType);
                    ASTDeclarator* decl = ArenaAlloc(sizeof*decl);
                    decl->declarator = afn->params[i]->declarator;
                    decl->declToken = afn->params[i]->declToken;
                    decl->redeclared = false;
                    decl->variableType = param;
                    ARRAY_PUSH(fn->as.function, param, decl);
                }
            }

            return fn;
        }; break;
    }

    // unreachable
    exit(0);
}

static void AnalyseExpression(ASTExpression* ast, ctx* ctx);

static void AnalyseAssignExpression(ASTExpression* ast, ctx* ctx) {
    ASTAssignExpression* assign = &ast->as.assign;
    if(!assign->target->isLvalue) {
        errorAt(ctx->parser, &assign->operator, "Operand must be an lvalue");
    }

    AnalyseExpression(assign->target, ctx);
    AnalyseExpression(assign->value, ctx);

    if(!TypeCompat(assign->target->exprType, assign->value->exprType)) {
        errorAt(ctx->parser, &assign->operator, "Cannot assign value to target of different type");
    }

    if(assign->operator.type != TOKEN_EQUAL_EQUAL && !TypeCompat(assign->value->exprType, &defaultInt)) {
        errorAt(ctx->parser, &assign->operator, "Cannot do arithmetic assignment with non arithmetic type");
    }

    ast->exprType = assign->target->exprType;
}

static void AnalyseBinaryExpression(ASTExpression* ast, ctx* ctx) {
    ASTBinaryExpression* bin = &ast->as.binary;
    AnalyseExpression(bin->left, ctx);
    AnalyseExpression(bin->right, ctx);

    // TODO - pointer arithmetic, integer conversions, ...
    if(!TypeCompat(bin->left->exprType, bin->right->exprType)) {
        errorAt(ctx->parser, &bin->operator, "Binary operator types must be equal");
    }

    if(!TypeCompat(bin->left->exprType, &defaultInt)) {
        errorAt(ctx->parser, &bin->operator, "Cannot use operator on non arithmetic type");
    }

    ast->exprType = bin->left->exprType;
}

static void AnalyseCallExpression(ASTExpression* ast, ctx* ctx) {
    // indirect call check
    //TODO - analyse functions, check multiple definition compatability,
    // possibly needs parser changes to record everything
    // check number/type of arguments
    // difference between int a() and int a(void)
    // get return value type
    // return type check

    ASTCallExpression* call = &ast->as.call;

    AnalyseExpression(call->target, ctx);
    for(unsigned int i = 0; i < call->paramCount; i++) {
        AnalyseExpression(call->params[i], ctx);
    }

    if(call->target->exprType->type != AST_VARIABLE_TYPE_FUNCTION) {
        errorAt(ctx->parser, &call->indirectErrorLoc, "Cannot call non function");
        return;
    }

    ast->exprType = call->target->exprType->as.function.ret;
}

static void AnalyseConstantExpression(ASTExpression* ast) {
    ASTConstantExpression* expr = &ast->as.constant;
    switch(expr->type) {
        case AST_CONSTANT_EXPRESSION_INTEGER:
            ast->exprType = &defaultInt;
            break;
        case AST_CONSTANT_EXPRESSION_LOCAL:
            ast->exprType = expr->local->type;
            break;
    }
}

static void AnalysePostfixExpression(ASTExpression* ast, ctx* ctx) {
    ASTPostfixExpression* post = &ast->as.postfix;
    if(!post->operand->isLvalue) {
        errorAt(ctx->parser, &post->operator, "Operand must be an lvalue");
    }

    AnalyseExpression(post->operand, ctx);

    // only postfix implemented are ++ and --
    if(!TypeCompat(post->operand->exprType, &defaultInt)) {
        errorAt(ctx->parser, &post->operator, "Cannot increment/decrement non arithmetic type");
    }

    ast->exprType = &defaultInt;
}

static void AnalyseTernaryExpression(ASTExpression* ast, ctx* ctx) {
    ASTTernaryExpression* op = &ast->as.ternary;

    AnalyseExpression(op->operand1, ctx);
    AnalyseExpression(op->operand2, ctx);
    AnalyseExpression(op->operand3, ctx);

    if(!TypeCompat(op->operand1->exprType, &defaultInt)) {
        errorAt(ctx->parser, &op->operator, "Condition must have scalar type");
    }

    if(!TypeCompat(op->operand2->exprType, op->operand3->exprType)) {
        errorAt(ctx->parser, &op->secondOperator, "condition values must have same type");
    }

    ast->exprType = op->operand2->exprType;
}

static void AnalyseUnaryExpression(ASTExpression* ast, ctx* ctx) {
    ASTUnaryExpression* unary = &ast->as.unary;

    AnalyseExpression(unary->operand, ctx);

    if(unary->operator.type == TOKEN_AND) {
        // elide &*var
        if(unary->operand->type == AST_EXPRESSION_UNARY &&
        unary->operand->as.unary.operator.type == TOKEN_STAR &&
        !unary->operand->as.unary.elide) {
            unary->elide = true;
            unary->operand->as.unary.elide = true;
        } else if(unary->operand->type != AST_EXPRESSION_CONSTANT ||
           unary->operand->as.constant.type != AST_CONSTANT_EXPRESSION_LOCAL) {
            // disallow &1, &(5+6), etc
            errorAt(ctx->parser, &unary->operator, "Cannot take address of not variable");
        }
    }

    // -a, ~a, &a, *a
    switch(unary->operator.type) {
        case TOKEN_NEGATE:
        case TOKEN_COMPLIMENT:
            if(!TypeCompat(unary->operand->exprType, &defaultInt)) {
                errorAt(ctx->parser, &unary->operator, "Cannot use operator on non arithmetic type");
            }
            ast->exprType = unary->operand->exprType;
            break;
        case TOKEN_AND: {
            ASTVariableType* addr = ArenaAlloc(sizeof(*addr));
            addr->type = AST_VARIABLE_TYPE_POINTER;
            addr->as.pointer = unary->operand->exprType;
            ast->exprType = addr;
        } break;
        case TOKEN_STAR:
            if(unary->operand->exprType->type != AST_VARIABLE_TYPE_POINTER) {
                errorAt(ctx->parser, &unary->operator, "Cannot dereference non pointer");
                return;
            }
            ast->exprType = unary->operand->exprType->as.pointer;
            break;
        default:
            printf("unreachable unary analysis");
            exit(0);
    }
}

static void AnalyseExpression(ASTExpression* ast, ctx* ctx) {
    if(ast == NULL) return;
    switch(ast->type) {
        case AST_EXPRESSION_ASSIGN:
            AnalyseAssignExpression(ast, ctx);
            break;
        case AST_EXPRESSION_BINARY:
            AnalyseBinaryExpression(ast, ctx);
            break;
        case AST_EXPRESSION_CALL:
            AnalyseCallExpression(ast, ctx);
            break;
        case AST_EXPRESSION_CONSTANT:
            AnalyseConstantExpression(ast);
            break;
        case AST_EXPRESSION_POSTFIX:
            AnalysePostfixExpression(ast, ctx);
            break;
        case AST_EXPRESSION_TERNARY:
            AnalyseTernaryExpression(ast, ctx);
            break;
        case AST_EXPRESSION_UNARY:
            AnalyseUnaryExpression(ast, ctx);
            break;
    }
}

static void AnalyseStatement(ASTStatement* ast, ctx* ctx);
static void AnalyseDeclaration(ASTDeclaration* ast, ctx* ctx);
static void AnalyseIterationStatement(ASTIterationStatement* ast, ctx* ctx) {
    bool oldLoop = ctx->inLoop;
    ctx->inLoop = true;

    if(ast->type == AST_ITERATION_STATEMENT_FOR_DECL) {
        AnalyseDeclaration(ast->preDecl, ctx);
    } else if(ast->type == AST_ITERATION_STATEMENT_FOR_EXPR) {
        AnalyseExpression(ast->preExpr, ctx);
    }

    AnalyseExpression(ast->control, ctx);
    if(!TypeCompat(ast->control->exprType, &defaultInt)) {
        errorAt(ctx->parser, &ast->keyword, "Loop condition must be of arithmetic type");
    }

    if(ast->type == AST_ITERATION_STATEMENT_FOR_DECL ||
       ast->type == AST_ITERATION_STATEMENT_FOR_EXPR) {
        AnalyseExpression(ast->post, ctx);
    }

    AnalyseStatement(ast->body, ctx);
    ctx->inLoop = oldLoop;
}

static void AnalyseSelectionStatement(ASTSelectionStatement* ast, ctx* ctx) {
    AnalyseExpression(ast->condition, ctx);
    if(!TypeCompat(ast->condition->exprType, &defaultInt)) {
        errorAt(ctx->parser, &ast->keyword, "Condition must have scalar type");
    }

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
            if(!TypeCompat(ast->expr->exprType, ctx->currentFn->as.function.ret)) {
                errorAt(ctx->parser, &ast->statement, "Cannot return wrong type");
            }
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
    for(unsigned int i = 0; i < ast->declaratorCount; i++) {
        ASTInitDeclarator* decl = ast->declarators[i];
        const ASTVariableType* decltype = decl->declarator->variableType;

        if(decl->declarator->anonymous) {
            errorAt(ctx->parser, &decl->declarator->declToken,
                "Cannot have anonymous declaration, expected identifier");
        }

        if(decl->type == AST_INIT_DECLARATOR_FUNCTION) {
            if(i != 0) {
                errorAt(ctx->parser, &decl->initializerStart,
                    "Cannot initialise function and variable at the same time");
            }

            if(decl->declarator->declarator->scopeDepth != 0) {
                errorAt(ctx->parser, &decl->initializerStart,
                    "Function definition not allowed in inner scope");
            }

            // whether function parameters are anonymous or not is not checked
            // although it is invalid c11, it is likley to become valid in c2x
            // and i dont really want to add more code to prevent it.
            // clang --std=c2x accepts the syntax

            for(unsigned int j = 0; j < decltype->as.function.paramCount; j++) {
                decltype->as.function.params[j]->declarator->type =
                    decltype->as.function.params[j]->variableType;
            }

            if(decl->declarator->declarator->type == NULL) {
                decl->declarator->declarator->type = decltype;
            } else {
                decl->declarator->declarator->type = TypeComposite(decl->declarator->declarator->type, decltype);
            }

            const ASTVariableType* old = ctx->currentFn;
            ctx->currentFn = decltype;
            AnalyseFnCompoundStatement(decl->fn, ctx);
            ctx->currentFn = old;
            continue;
        }

        decl->declarator->declarator->type = decltype;

        if(decl->declarator->declarator->scopeDepth == 0) {
            errorAt(ctx->parser, &decl->declarator->declToken,
                "Global variables not implemented yet");
        }
        if(decl->declarator->redeclared) {
            errorAt(ctx->parser, &decl->declarator->declToken,
                "Cannot redeclare variable with same linkage");
        }

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