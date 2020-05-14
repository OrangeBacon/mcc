#include "analysis.h"
#include <stdlib.h>

typedef struct ctx {
    Parser* parser;
    bool inLoop;
    bool convertFnDesignator;

    const ASTVariableType* currentFn;
    ASTTranslationUnit* translationUnit;
} ctx;

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
                    decl->symbol = afn->params[i]->symbol;
                    decl->declToken = afn->params[i]->declToken;
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

    assign->pointerShift = false;

    if(assign->operator.type == TOKEN_PLUS_EQUAL || assign->operator.type == TOKEN_MINUS_EQUAL) {
        if(assign->target->exprType->type == AST_VARIABLE_TYPE_POINTER) {
            if(!TypeCompat(assign->value->exprType, &defaultInt)) {
                errorAt(ctx->parser, &assign->operator, "Cannot change pointer by non arithmetic ammount");
            }
            assign->pointerShift = true;
        } else {
            goto arithassign;
        }
    } else {
        arithassign:
        if(!TypeCompat(assign->target->exprType, assign->value->exprType)) {
            errorAt(ctx->parser, &assign->operator, "Cannot assign value to target of different type");
        }

        if(assign->operator.type != TOKEN_EQUAL && !TypeCompat(assign->value->exprType, &defaultInt)) {
            errorAt(ctx->parser, &assign->operator, "Cannot do arithmetic assignment with non arithmetic type");
        }
    }

    ast->exprType = assign->target->exprType;
}

static void AnalyseBinaryExpression(ASTExpression* ast, ctx* ctx) {
    ASTBinaryExpression* bin = &ast->as.binary;
    AnalyseExpression(bin->left, ctx);
    AnalyseExpression(bin->right, ctx);

    ast->as.binary.pointerShift = false;

    // TODO - integer conversions, ...
    if(bin->operator.type == TOKEN_PLUS) {
        int ptrCount = 0;
        ptrCount |= (bin->left->exprType->type == AST_VARIABLE_TYPE_POINTER) << 0;
        ptrCount |= (bin->right->exprType->type == AST_VARIABLE_TYPE_POINTER) << 1;

        if(ptrCount == 0) {
            if(!TypeCompat(bin->left->exprType, &defaultInt) || !TypeCompat(bin->right->exprType, &defaultInt)) {
                errorAt(ctx->parser, &bin->operator, "Cannot add non arithmetic type");
            }
            ast->exprType = bin->left->exprType;
        } else if(ptrCount == 1) {
            // left ptr
            if(!TypeCompat(bin->right->exprType, &defaultInt)) {
                errorAt(ctx->parser, &bin->operator, "Cannot add non-arithmetic type to pointer");
            }
            ast->exprType = bin->left->exprType;
            ast->as.binary.pointerShift = true;
        } else if(ptrCount == 2) {
            // right ptr
            if(!TypeCompat(bin->left->exprType, &defaultInt)) {
                errorAt(ctx->parser, &bin->operator, "Cannot add non-arithmetic type to pointer");
            }
            ast->exprType = bin->right->exprType;
            ast->as.binary.pointerShift = true;
        } else {
            errorAt(ctx->parser, &bin->operator, "Cannot add pointers");
            ast->exprType = bin->left->exprType;
        }
    } else if(bin->operator.type == TOKEN_NEGATE) {
        int ptrCount = 0;
        ptrCount |= (bin->left->exprType->type == AST_VARIABLE_TYPE_POINTER) << 0;
        ptrCount |= (bin->right->exprType->type == AST_VARIABLE_TYPE_POINTER) << 1;

        if(ptrCount == 0) {
            if(!TypeCompat(bin->left->exprType, &defaultInt) || !TypeCompat(bin->right->exprType, &defaultInt)) {
                errorAt(ctx->parser, &bin->operator, "Cannot subtract non arithmetic type");
            }
            ast->exprType = bin->left->exprType;
        } else if(ptrCount == 1) {
            // left ptr
            if(!TypeCompat(bin->right->exprType, &defaultInt)) {
                errorAt(ctx->parser, &bin->operator, "Cannot add non-arithmetic type to pointer");
            }
            ast->exprType = bin->left->exprType;
            ast->as.binary.pointerShift = true;
        } else if(ptrCount == 2) {
            // right ptr
            if(!TypeCompat(bin->left->exprType, &defaultInt)) {
                errorAt(ctx->parser, &bin->operator, "Cannot add non-arithmetic type to pointer");
            }
            ast->exprType = bin->right->exprType;
            ast->as.binary.pointerShift = true;
        } else {
            // two pointers
            if(!TypeCompat(bin->left->exprType, bin->right->exprType)) {
                errorAt(ctx->parser, &bin->operator, "Cannot subtract pointers of different type");
            }
            ast->exprType = bin->left->exprType;
        }
    } else if(bin->operator.type == TOKEN_EQUAL_EQUAL || bin->operator.type == TOKEN_NOT_EQUAL || bin->operator.type == TOKEN_LESS || bin->operator.type == TOKEN_LESS_EQUAL || bin->operator.type == TOKEN_GREATER || bin->operator.type == TOKEN_GREATER_EQUAL) {
        if(!((TypeCompat(bin->left->exprType, &defaultInt) && TypeCompat(bin->right->exprType, &defaultInt))||(bin->left->exprType->type == AST_VARIABLE_TYPE_POINTER && TypeCompat(bin->left->exprType, bin->right->exprType)))) {
            errorAt(ctx->parser, &bin->operator, "Cannot check different types");
        }
        ast->exprType = &defaultInt;
    } else if(bin->operator.type == TOKEN_COMMA) {
        ast->exprType = bin->right->exprType;
    } else {
        if(!TypeCompat(bin->left->exprType, &defaultInt) || !TypeCompat(bin->right->exprType, &defaultInt)) {
                errorAt(ctx->parser, &bin->operator, "Cannot use operator on non arithmetic type");
        }
        ast->exprType = bin->left->exprType;
    }
}

static void AnalyseCallExpression(ASTExpression* ast, ctx* ctx) {
    // TODO - indirect call check
    // TODO - difference between int a() and int a(void)

    ASTCallExpression* call = &ast->as.call;

    AnalyseExpression(call->target, ctx);
    for(unsigned int i = 0; i < call->paramCount; i++) {
        AnalyseExpression(call->params[i], ctx);
    }

    if(call->target->exprType->type != AST_VARIABLE_TYPE_POINTER ||
       call->target->exprType->as.pointer->type != AST_VARIABLE_TYPE_FUNCTION) {
        errorAt(ctx->parser, &call->indirectErrorLoc, "Cannot call non pointer to function");
        return;
    }

    ast->exprType = call->target->exprType->as.pointer->as.function.ret;
}

static void AnalyseConstantExpression(ASTExpression* ast, ctx* ctx) {
    ASTConstantExpression* expr = &ast->as.constant;
    switch(expr->type) {
        case AST_CONSTANT_EXPRESSION_INTEGER:
            ast->exprType = &defaultInt;
            break;
        case AST_CONSTANT_EXPRESSION_LOCAL:
            if(expr->local->type->type == AST_VARIABLE_TYPE_FUNCTION && ctx->convertFnDesignator) {
                // according to the c standard (n1570 draft) in section
                // 6.3.2.1(4) A function designator should automatically convert
                // into its address unless in sizeof, address of or align of
                // operators.  Those operators should set convertFnDesignator to
                // false, otherwise it should be left true.

                // This adds the ast node and type node required to take its
                // address, otherwise functions are treated identically to
                // other global variables.

                ASTExpression* newExp = ArenaAlloc(sizeof*newExp);
                *newExp = *ast;
                newExp->exprType = expr->local->type;
                ast->type = AST_EXPRESSION_UNARY;
                ast->as.unary.elide = false;
                ast->as.unary.operator = TokenMake(TOKEN_AND);
                ast->as.unary.operand = newExp;

                ASTVariableType* type = ArenaAlloc(sizeof(*type));
                type->type = AST_VARIABLE_TYPE_POINTER;
                type->token = TokenMake(TOKEN_AND);
                type->as.pointer = newExp->exprType;
                ast->exprType = type;
            } else {
                ast->exprType = expr->local->type;
            }
            break;
    }
}

static void AnalysePostfixExpression(ASTExpression* ast, ctx* ctx) {
    ASTPostfixExpression* post = &ast->as.postfix;
    if(!post->operand->isLvalue) {
        errorAt(ctx->parser, &post->operator, "Operand must be an lvalue");
    }

    AnalyseExpression(post->operand, ctx);

    post->pointerShift = post->operand->exprType->type == AST_VARIABLE_TYPE_POINTER;
    if(!TypeCompat(post->operand->exprType, &defaultInt) && !post->pointerShift) {
        errorAt(ctx->parser, &post->operator, "Cannot increment/decrement non arithmetic or pointer type");
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

    if(unary->operator.type == TOKEN_AND) {
        bool old = ctx->convertFnDesignator;
        ctx->convertFnDesignator = false;
        AnalyseExpression(unary->operand, ctx);

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
        ctx->convertFnDesignator = old;
    } else {
        AnalyseExpression(unary->operand, ctx);
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

static void AnalyseCastExpression(ASTExpression* ast, ctx* ctx) {
    ASTCastExpression* cast = &ast->as.cast;

    if(!cast->type->anonymous) {
        errorAt(ctx->parser, &cast->type->declToken, "Unexpected identifier");
    }

    AnalyseExpression(cast->expression, ctx);
    ast->exprType = cast->type->variableType;
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
            AnalyseConstantExpression(ast, ctx);
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
        case AST_EXPRESSION_CAST:
            AnalyseCastExpression(ast, ctx);
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

static void AnalyseFnDeclaration(ASTInitDeclarator* decl, const ASTVariableType* decltype, ctx* ctx) {
    if(decl->declarator->symbol->scopeDepth != 0) {
        errorAt(ctx->parser, &decl->initializerStart,
            "Function definition not allowed in inner scope");
    }

    // whether function parameters are anonymous or not is not checked
    // although it is invalid c11, it is likley to become valid in c2x
    // and i dont really want to add more code to prevent it.
    // clang --std=c2x accepts the syntax

    for(unsigned int j = 0; j < decltype->as.function.paramCount; j++) {
        decltype->as.function.params[j]->symbol->type =
            decltype->as.function.params[j]->variableType;
    }

    if(decl->declarator->symbol->type == NULL) {
        decl->declarator->symbol->type = decltype;
    } else {
        decl->declarator->symbol->type = TypeComposite(decl->declarator->symbol->type, decltype);
    }

    const ASTVariableType* old = ctx->currentFn;
    ctx->currentFn = decltype;
    AnalyseFnCompoundStatement(decl->fn, ctx);
    ctx->currentFn = old;
}

static void AnalyseDeclaration(ASTDeclaration* ast, ctx* ctx) {
    if(ast == NULL) return;
    for(unsigned int i = 0; i < ast->declaratorCount; i++) {
        ASTInitDeclarator* decl = ast->declarators[i];
        const ASTVariableType* decltype = decl->declarator->variableType;

        if(decl->declarator->anonymous) {
            errorAt(ctx->parser, &decl->declarator->declToken,
                "Cannot have anonymous declaration, expected identifier");
        }

        if(decltype->type == AST_VARIABLE_TYPE_FUNCTION &&
           decl->type == AST_INIT_DECLARATOR_INITIALIZE) {
            errorAt(ctx->parser, &decl->initializerStart,
                "Cannot initialise function with value");
        }

        if(decl->type == AST_INIT_DECLARATOR_FUNCTION) {
            if(i != 0) {
                errorAt(ctx->parser, &decl->initializerStart,
                    "Cannot initialise function and variable at the same time");
            }
            AnalyseFnDeclaration(decl, decltype, ctx);
            continue;
        }

        SymbolLocal* symbol = decl->declarator->symbol;
        bool isGlobal = symbol->scopeDepth == 0;
        bool isInitialising = decl->type != AST_INIT_DECLARATOR_NO_INITIALIZE;
        bool isInitialised = symbol->initialised;

        if((isInitialising || !isGlobal) && isInitialised) {
            errorAt(ctx->parser, &decl->initializerStart,
                "Cannot re-declare identifier with the same linkage");
        }

        symbol->initialised |= isInitialising;
        if(!isGlobal) {
            symbol->initialised = true;
        }

        symbol->type = decltype;

        AnalyseExpression(decl->initializer, ctx);

        // if is global
        if(decl->declarator->symbol->scopeDepth == 0) {
            if(isInitialising &&
               (decl->initializer->type != AST_EXPRESSION_CONSTANT ||
                decl->initializer->as.constant.type != AST_CONSTANT_EXPRESSION_INTEGER)) {
                errorAt(ctx->parser, &decl->initializerStart,
                    "Global cannot have non-constant value");
            }
            if(!isInitialising && !symbol->initialised) {
                TABLE_SET(ctx->translationUnit->undefinedSymbols, symbol->name, symbol->length, symbol);
            } else {
                tableRemove(&ctx->translationUnit->undefinedSymbols, symbol->name, symbol->length);
            }
        }
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
    ctx->translationUnit = ast;
    for(unsigned int i = 0; i < ast->declarationCount; i++) {
        AnalyseDeclaration(ast->declarations[i], ctx);
    }
}

void Analyse(Parser* parser) {
    ctx ctx = {
        .parser = parser,
        .inLoop = false,
        .convertFnDesignator = true,
    };
    TABLE_INIT(parser->ast->undefinedSymbols, SymbolLocal*);
    AnalyseTranslationUnit(parser->ast, &ctx);
}