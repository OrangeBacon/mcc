#include "parser.h"

#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void ParserInit(Parser* parser, char* fileName) {
    Scanner* scanner = ArenaAlloc(sizeof(*scanner));
    ScannerInit(scanner, fileName);
    parser->scanner = scanner;
    parser->panicMode = false;
    parser->hadError = false;
    SymbolTableInit(&parser->locals);
}

void errorAt(Parser* parser, Token* loc, const char* message) {
    if(parser->panicMode) return;
    parser->panicMode = true;

    fprintf(stderr, "[%d:%d] Error", loc->line, loc->column);

    if (loc->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (loc->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", loc->length, loc->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;

    // prevent infinite loops
    // todo - error syncronisation + recovery
    printf("PANIC\n");
    exit(1);
}

static void error(Parser* parser, const char* message) {
    errorAt(parser, &parser->previous, message);
}

static void errorAtCurrent(Parser* parser, const char* message) {
    errorAt(parser, &parser->current, message);
}

static void advance(Parser* parser) {
    parser->previous = parser->current;

    while(true) {
        ScannerNext(parser->scanner, &parser->current);

        if(parser->current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser, parser->current.start);
    }
}

static void consume(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }

    errorAtCurrent(parser, message);
}

static bool check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

static bool checks(Parser* parser, int count, ...) {
    va_list args;
    va_start(args, count);

    bool ret = false;
    while(count-- && !ret) ret |= check(parser, va_arg(args, TokenType));

    va_end(args);
    return ret;
}

static bool match(Parser* parser, TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

typedef enum Precidence {
    PREC_NONE,
    PREC_COMMA,
    PREC_ASSIGN,
    PREC_CONDITIONAL,
    PREC_LOGICOR,
    PREC_LOGICAND,
    PREC_BITOR,
    PREC_BITXOR,
    PREC_BITAND,
    PREC_EQUALITY,
    PREC_RELATION,
    PREC_SHIFT,
    PREC_ADDITIVE,
    PREC_MULTIPLICITIVE,
    PREC_CAST,
    PREC_UNARY,
    PREC_POSTFIX,
    PREC_PRIMARY,
} Precidence;

typedef ASTExpression* (*PrefixFn)(Parser*);
typedef ASTExpression* (*InfixFn)(Parser*, ASTExpression*);

typedef struct ParseRule {
    PrefixFn prefix;
    InfixFn infix;
    Precidence precidence;
} ParseRule;


static ParseRule* getRule(TokenType type);

static ASTExpression* parsePrecidence(Parser* parser, Precidence precidence) {
    advance(parser);
    PrefixFn prefixRule = getRule(parser->previous.type)->prefix;
    if(prefixRule == NULL) {
        error(parser, "Expected expression");
        return NULL;
    }

    ASTExpression* exp = prefixRule(parser);

    // will probably crash if a bad prefix expression was parsed and null
    // passed into an infix rule
    if(exp == NULL) {
        return NULL;
    }

    while(precidence <= getRule(parser->current.type)->precidence) {
        advance(parser);
        InfixFn infixRule = getRule(parser->previous.type)->infix;
        exp = infixRule(parser, exp);
    }

    return exp;
}

static ASTExpression* Expression(Parser* parser) {
    return parsePrecidence(parser, PREC_COMMA);
}

static ASTExpression* Variable(Parser* parser) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_CONSTANT;
    ast->as.constant.tok = parser->previous;

    SymbolLocal* local = SymbolTableGetLocal(&parser->locals,
            parser->previous.start, parser->previous.length);
    if(local == NULL) {
        error(parser, "Variable name not declared");
        return ast;
    } else {
        ast->as.constant.type = AST_CONSTANT_EXPRESSION_LOCAL;
        ast->as.constant.local = local;
        ast->isLvalue = true;
    }

    return ast;
}

static ASTExpression* Grouping(Parser* parser) {
    ASTExpression* ast = Expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')'");
    return ast;
}

static ASTExpression* Constant(Parser* parser) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_CONSTANT;
    ast->isLvalue = false;
    ast->as.constant.type = AST_CONSTANT_EXPRESSION_INTEGER;
    ast->as.constant.tok = parser->previous;
    ast->as.constant.tok.numberValue = strtod(parser->previous.start, NULL);
    return ast;
}

static ASTExpression* Unary(Parser* parser) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_UNARY;
    ast->as.unary.operator = parser->previous;
    ast->as.unary.operand = parsePrecidence(parser, PREC_UNARY);
    ast->as.unary.elide = false;

    if(ast->as.unary.operator.type == TOKEN_STAR) {
        ast->isLvalue = true;
    } else {
        ast->isLvalue = false;
    }
    return ast;
}

static ASTExpression* PreIncDec(Parser* parser) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_ASSIGN;
    ast->isLvalue = false;
    if(parser->previous.type == TOKEN_PLUS_PLUS) {
        ast->as.assign.operator = TokenMake(TOKEN_PLUS_EQUAL);
    } else {
        ast->as.assign.operator = TokenMake(TOKEN_MINUS_EQUAL);
    }

    ASTExpression* one = ArenaAlloc(sizeof(*one));
    one->type = AST_EXPRESSION_CONSTANT;
    one->isLvalue = false;
    one->as.constant.tok = TokenMake(TOKEN_INTEGER);
    one->as.constant.tok.numberValue = 1;
    one->as.constant.type = AST_CONSTANT_EXPRESSION_INTEGER;

    ast->as.assign.value = one;

    ASTExpression* exp = parsePrecidence(parser, PREC_UNARY);
    ast->as.assign.target = exp;

    return ast;
}

static ASTExpression* Call(Parser* parser, ASTExpression* prev) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_CALL;
    ast->isLvalue = false;
    ast->as.call.indirectErrorLoc = parser->previous;

    ARRAY_ZERO(ast->as.call, param);
    ast->as.call.target = prev;
    if(match(parser, TOKEN_RIGHT_PAREN)) return ast;

    ARRAY_ALLOC(ASTExpression*, ast->as.call, param);
    while(!match(parser, TOKEN_EOF)) {
        ARRAY_PUSH(ast->as.call, param, parsePrecidence(parser, PREC_ASSIGN));
        if(match(parser, TOKEN_RIGHT_PAREN)) break;
        consume(parser, TOKEN_COMMA, "Expected ','");
    }

    return ast;
}

static ASTExpression* Binary(Parser* parser, ASTExpression* prev) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_BINARY;
    ast->isLvalue = false;
    ast->as.binary.operator = parser->previous;
    ast->as.binary.left = prev;

    ParseRule* rule = getRule(ast->as.binary.operator.type);
    ast->as.binary.right = parsePrecidence(parser, (Precidence)(rule->precidence + 1));

    return ast;
}

static ASTExpression* Assign(Parser* parser, ASTExpression* prev) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_ASSIGN;
    ast->isLvalue = false;
    ast->as.assign.operator = parser->previous;
    ast->as.assign.value = parsePrecidence(parser, PREC_ASSIGN);
    ast->as.assign.target = prev;

    return ast;
}

static ASTExpression* PostIncDec(Parser* parser, ASTExpression* prev) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_POSTFIX;
    ast->isLvalue = false;
    ast->as.postfix.operator = parser->previous;
    ast->as.postfix.operand = prev;

    return ast;
}

static ASTExpression* Condition(Parser* parser, ASTExpression* prev) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_TERNARY;
    ast->as.ternary.operator = parser->previous;
    ast->as.ternary.operand1 = prev;
    ast->as.ternary.operand2 = Expression(parser);
    consume(parser, TOKEN_COLON, "Expected ':' in conditional expression");
    ast->as.ternary.secondOperator = parser->previous;
    ast->as.ternary.operand3 = parsePrecidence(parser, PREC_CONDITIONAL);

    return ast;
}

ParseRule rules[] = {
    [TOKEN_IDENTIFIER] =        { Variable,  NULL,       PREC_NONE           },
    [TOKEN_LEFT_PAREN] =        { Grouping,  Call,       PREC_POSTFIX        },
    [TOKEN_RIGHT_PAREN] =       { NULL,      NULL,       PREC_NONE           },
    [TOKEN_LEFT_BRACE] =        { NULL,      NULL,       PREC_NONE           },
    [TOKEN_RIGHT_BRACE] =       { NULL,      NULL,       PREC_NONE           },
    [TOKEN_RETURN] =            { NULL,      NULL,       PREC_NONE           },
    [TOKEN_INTEGER] =           { Constant,  NULL,       PREC_NONE           },
    [TOKEN_SEMICOLON] =         { NULL,      NULL,       PREC_NONE           },
    [TOKEN_INT] =               { NULL,      NULL,       PREC_NONE           },
    [TOKEN_NEGATE] =            { Unary,     Binary,     PREC_ADDITIVE       },
    [TOKEN_COMPLIMENT] =        { Unary,     NULL,       PREC_NONE           },
    [TOKEN_NOT] =               { Unary,     NULL,       PREC_NONE           },
    [TOKEN_PLUS] =              { NULL,      Binary,     PREC_ADDITIVE       },
    [TOKEN_STAR] =              { Unary,     Binary,     PREC_MULTIPLICITIVE },
    [TOKEN_SLASH] =             { NULL,      Binary,     PREC_MULTIPLICITIVE },
    [TOKEN_AND_AND] =           { NULL,      Binary,     PREC_LOGICAND       },
    [TOKEN_OR_OR] =             { NULL,      Binary,     PREC_LOGICOR        },
    [TOKEN_EQUAL_EQUAL] =       { NULL,      Binary,     PREC_EQUALITY       },
    [TOKEN_NOT_EQUAL] =         { NULL,      Binary,     PREC_EQUALITY       },
    [TOKEN_LESS] =              { NULL,      Binary,     PREC_RELATION       },
    [TOKEN_LESS_EQUAL] =        { NULL,      Binary,     PREC_RELATION       },
    [TOKEN_GREATER] =           { NULL,      Binary,     PREC_RELATION       },
    [TOKEN_GREATER_EQUAL] =     { NULL,      Binary,     PREC_RELATION       },
    [TOKEN_AND] =               { Unary,     Binary,     PREC_BITAND         },
    [TOKEN_OR] =                { NULL,      Binary,     PREC_BITOR          },
    [TOKEN_EQUAL] =             { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_PERCENT] =           { NULL,      Binary,     PREC_MULTIPLICITIVE },
    [TOKEN_SHIFT_LEFT] =        { NULL,      Binary,     PREC_SHIFT          },
    [TOKEN_SHIFT_RIGHT] =       { NULL,      Binary,     PREC_SHIFT          },
    [TOKEN_XOR] =               { NULL,      Binary,     PREC_BITXOR         },
    [TOKEN_COMMA] =             { NULL,      Binary,     PREC_COMMA          },
    [TOKEN_MINUS_MINUS] =       { PreIncDec, PostIncDec, PREC_POSTFIX        },
    [TOKEN_PLUS_PLUS] =         { PreIncDec, PostIncDec, PREC_POSTFIX        },
    [TOKEN_PLUS_EQUAL] =        { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_MINUS_EQUAL] =       { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_SLASH_EQUAL] =       { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_STAR_EQUAL] =        { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_PERCENT_EQUAL] =     { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_LEFT_SHIFT_EQUAL] =  { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_RIGHT_SHIFT_EQUAL] = { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_AND_EQUAL] =         { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_OR_EQUAL] =          { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_XOR_EQUAL] =         { NULL,      Assign,     PREC_ASSIGN         },
    [TOKEN_IF] =                { NULL,      NULL,       PREC_NONE           },
    [TOKEN_ELSE] =              { NULL,      NULL,       PREC_NONE           },
    [TOKEN_COLON] =             { NULL,      NULL,       PREC_NONE           },
    [TOKEN_QUESTION] =          { NULL,      Condition,  PREC_CONDITIONAL    },
    [TOKEN_FOR] =               { NULL,      NULL,       PREC_NONE           },
    [TOKEN_WHILE] =             { NULL,      NULL,       PREC_NONE           },
    [TOKEN_DO] =                { NULL,      NULL,       PREC_NONE           },
    [TOKEN_CONTINUE] =          { NULL,      NULL,       PREC_NONE           },
    [TOKEN_BREAK] =             { NULL,      NULL,       PREC_NONE           },
    [TOKEN_ERROR] =             { NULL,      NULL,       PREC_NONE           },
    [TOKEN_EOF] =               { NULL,      NULL,       PREC_NONE           },
};

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

#define ASTFN(type) \
    static AST##type* type(Parser* parser) { \
    AST##type* ast = ArenaAlloc(sizeof(*ast));
#define ASTFN_END() \
    return ast; }

typedef struct TokenStack {
    ARRAY_DEFINE(Token, token);
} TokenStack;

ASTFN(Declarator)
    // create store for tokens to deal with later
    TokenStack stack;
    ARRAY_ALLOC(Token, stack, token);

    // store all valid tokens before an identifier (will add const, etc)
    // nesting depth used so when parsing a function prototype the last
    // argument does not consume the closing ')'
    int nestingDepth = 0;
    while(match(parser, TOKEN_LEFT_PAREN) || match(parser, TOKEN_STAR)) {
        if(parser->previous.type == TOKEN_LEFT_PAREN) {
            nestingDepth++;
        }
        ARRAY_PUSH(stack, token, parser->previous);
    }

    // variable name, here is where anonymous type definitions would apear
    // e.g. in casts and prototypes - still todo
    if(!match(parser, TOKEN_IDENTIFIER)) {
        error(parser, "Expected variable name");
    }

    SymbolLocal* local = SymbolTableAddLocal(&parser->locals,
        parser->previous.start, parser->previous.length);

    // work around for top level prototype redeclaration
    // multiple definitions will be caught by analysis
    if(local == NULL) {
        local = SymbolTableGetLocal(&parser->locals,
            parser->previous.start, parser->previous.length);
        ast->redeclared = true;
    } else {
        ast->redeclared = false;
    }
    ast->declarator = local;
    ast->declToken = parser->previous;

    // the variable has this type
    const ASTVariableType* type = NULL;

    // where changes are applied
    const ASTVariableType** hole = &type;

    // if seekforward, check new tokens after the name, otherwise check the
    // stack of tokens already read.
    bool seekForward = true;

    // true when the next new token is something that can come after an
    // init declaration
    bool reachedForwardEnd = false;

    // check all tokens on the stack
    while(stack.tokenCount > 0 || parser->current.type == TOKEN_LEFT_PAREN) {

        if(seekForward && nestingDepth > 0 && match(parser, TOKEN_RIGHT_PAREN)) {
            // if next new token is a right paren and one is needed in the type
            // start checking the stack
            seekForward = false;
            nestingDepth--;
        } else if(seekForward && match(parser, TOKEN_LEFT_PAREN)) {
            ASTVariableType* fn = ArenaAlloc(sizeof(*fn));
            fn->token = parser->previous;
            fn->type = AST_VARIABLE_TYPE_FUNCTION;
            fn->as.function.isFromDefinition = false;
            ARRAY_ALLOC(ASTVariableType*, fn->as.function, param);

            // for symbol table management - increase depth, then record it
            // at the end of the arguments, remove all new levels of depth
            // other than one, which will be removed in initdecl
            // allows functions to refer to the correct symbols
            SymbolTableEnter(&parser->locals);

            unsigned int tableCount = parser->locals.currentDepth;

            if(!check(parser, TOKEN_RIGHT_PAREN))
            while(!match(parser, TOKEN_EOF)) {
                consume(parser, TOKEN_INT, "Expected int");
                ARRAY_PUSH(fn->as.function, param, Declarator(parser));
                if(!match(parser, TOKEN_COMMA)) break;
            }

            consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after function type");

            // remove scope levels
            while(parser->locals.currentDepth > tableCount) {
                SymbolTableExit(&parser->locals);
            }

            *hole = fn;
            hole = &fn->as.function.ret;
        } else if(seekForward) {

            // not a right paren or right paren not required
            // here would be where arrays would be
            // added to the parser
            reachedForwardEnd = true;
            seekForward = false;
        } else {
            // not going forwards so check stack - take newest token off
            // the top and return it
            Token next = ARRAY_POP(stack, token);

            if(next.type == TOKEN_LEFT_PAREN) {
                // cannot use more from the stack, resume with new tokens
                seekForward = true;
                if(reachedForwardEnd) {
                    error(parser, "Unexpected end of type definition");
                    break;
                }

            } else if(next.type == TOKEN_STAR) {

                // add a level of pointer to the type
                ASTVariableType* ptr = ArenaAlloc(sizeof*type);
                ptr->type = AST_VARIABLE_TYPE_POINTER;
                ptr->token = next;
                *hole = ptr;
                hole = &ptr->as.pointer;
            } else {
                // something else unknown - const, volatile, _Atomic, restrict
                // are all technically valid, but unsupported here
                errorAt(parser, &next, "Expected '(' or '*' in type");
                break;
            }
        }
    }

    ASTVariableType* base = ArenaAlloc(sizeof(ASTVariableType));
    base->type = AST_VARIABLE_TYPE_INT;
    *hole = base;

    ast->variableType = type;
ASTFN_END()

static ASTBlockItem* BlockItem(Parser*);
ASTFN(FnCompoundStatement)
    ARRAY_ALLOC(ASTBlockItem*, *ast, item);
    while(!match(parser, TOKEN_EOF)) {
        if(parser->current.type == TOKEN_RIGHT_BRACE) break;
        ARRAY_PUSH(*ast, item, BlockItem(parser));
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}'");
ASTFN_END()

static ASTInitDeclarator* InitDeclarator(Parser* parser, bool* foundFnDef) {
    ASTInitDeclarator* ast = ArenaAlloc(sizeof(*ast));

    unsigned int tableCount = parser->locals.currentDepth;

    ast->declarator = Declarator(parser);

    *foundFnDef = false;

    if(match(parser, TOKEN_EQUAL)) {
        ast->type = AST_INIT_DECLARATOR_INITIALIZE;
        ast->initializerStart = parser->previous;
        ast->initializer = parsePrecidence(parser, PREC_ASSIGN);
        ast->fn = NULL;
    } else if(match(parser, TOKEN_LEFT_BRACE)) {
        ast->type = AST_INIT_DECLARATOR_FUNCTION;
        ast->initializerStart = parser->previous;

        if(ast->declarator->variableType->type != AST_VARIABLE_TYPE_FUNCTION) {
            errorAt(parser, &parser->previous, "Cannot define function after non function type");
        } else {
            ((ASTVariableType*)ast->declarator->variableType)->as.function.isFromDefinition = true;
        }

        ast->fn = FnCompoundStatement(parser);
        ast->initializer = NULL;

        *foundFnDef = true;
    } else {

        // function prototypes different from variables
        if(ast->declarator->variableType->type == AST_VARIABLE_TYPE_FUNCTION) {
            ast->type = AST_INIT_DECLARATOR_FUNCTION;
        } else {
            ast->type = AST_INIT_DECLARATOR_NO_INITIALIZE;
        }

        // if not set to null, ast print occasionaly crashed
        ast->initializer = NULL;
        ast->fn = NULL;
        ast->initializer = NULL;
    }

    // unknown scope exit
    while(parser->locals.currentDepth > tableCount) {
        SymbolTableExit(&parser->locals);
    }

    return ast;
}

ASTFN(Declaration)
    ARRAY_ALLOC(ASTInitDeclarator*, *ast, declarator);

    bool foundFnDef;

    while(!match(parser, TOKEN_EOF)) {
        // parse an init declarator if any of the starting tokens
        // for an init declarator come next, otherwise exit
        if(!checks(parser, 3, TOKEN_IDENTIFIER, TOKEN_STAR, TOKEN_LEFT_PAREN)) break;

        // cannot have any more initdeclarators after function, do not accept
        // semicolon after either.
        ARRAY_PUSH(*ast, declarator, InitDeclarator(parser, &foundFnDef));
        if(!match(parser, TOKEN_COMMA)) break;
        if(foundFnDef) break;
    }

    if(!foundFnDef) {
        consume(parser, TOKEN_SEMICOLON, "Expected ';'");
    }
ASTFN_END()

static ASTStatement* Statement(Parser*);

ASTFN(SelectionStatement)
    ast->keyword = parser->previous;
    consume(parser, TOKEN_LEFT_PAREN, "Expected '('");
    ast->condition = Expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')'");
    ast->block = Statement(parser);
    if(match(parser, TOKEN_ELSE)) {
        ast->type = AST_SELECTION_STATEMENT_IFELSE;
        ast->elseBlock = Statement(parser);
    } else {
        ast->type = AST_SELECTION_STATEMENT_IF;
    }
ASTFN_END()

static ASTBlockItem* BlockItem(Parser*);

ASTFN(CompoundStatement)
    SymbolTableEnter(&parser->locals);

    ARRAY_ALLOC(ASTBlockItem*, *ast, item);
    while(!match(parser, TOKEN_EOF)) {
        if(parser->current.type == TOKEN_RIGHT_BRACE) break;
        ARRAY_PUSH(*ast, item, BlockItem(parser));
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}'");
    ast->popCount = SymbolTableExit(&parser->locals);
ASTFN_END()

ASTIterationStatement* While(Parser* parser) {
    ASTIterationStatement* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_ITERATION_STATEMENT_WHILE;
    ast->keyword = parser->previous;
    consume(parser, TOKEN_LEFT_PAREN, "Expected '('");
    ast->control = Expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')'");
    ast->body = Statement(parser);

    // maybe i wrote analysis badly?
    ast->freeCount = NULL;
    ast->post = NULL;
    ast->preDecl = NULL;
    ast->preExpr = NULL;

    return ast;
}

ASTIterationStatement* For(Parser* parser) {
    ASTIterationStatement* ast = ArenaAlloc(sizeof(*ast));
    ast->keyword = parser->previous;

    SymbolTableEnter(&parser->locals);

    consume(parser, TOKEN_LEFT_PAREN, "Expected '(");
    if(match(parser, TOKEN_INT)) {
        ast->type = AST_ITERATION_STATEMENT_FOR_DECL;
        ast->preDecl = Declaration(parser);
        ast->preExpr = NULL;
    } else if(match(parser, TOKEN_SEMICOLON)) {
        ast->type = AST_ITERATION_STATEMENT_FOR_EXPR;
        ast->preExpr = NULL;
        ast->preDecl = NULL;
    } else {
        ast->type = AST_ITERATION_STATEMENT_FOR_EXPR;
        ast->preExpr = Expression(parser);
        ast->preDecl = NULL;
        consume(parser, TOKEN_SEMICOLON, "Expected ';'");
    }

    if(match(parser, TOKEN_SEMICOLON)) {
        ASTExpression* one = ArenaAlloc(sizeof*one);
        one->type = AST_EXPRESSION_CONSTANT;
        one->as.constant.tok = TokenMake(TOKEN_INTEGER);
        one->as.constant.tok.numberValue = 1;
        one->as.constant.type = AST_CONSTANT_EXPRESSION_INTEGER;
        ast->control = one;
    } else {
        ast->control = Expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expected ';'");
    }

    if(match(parser, TOKEN_RIGHT_PAREN)) {
        ast->post = NULL;
    } else {
        ast->post = Expression(parser);
        consume(parser, TOKEN_RIGHT_PAREN, "Expected ')'");
    }

    ast->body = Statement(parser);

    ast->freeCount = SymbolTableExit(&parser->locals);

    return ast;
}

ASTIterationStatement* DoWhile(Parser* parser) {
    ASTIterationStatement* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_ITERATION_STATEMENT_DO;

    ast->body = Statement(parser);

    consume(parser, TOKEN_WHILE, "Expected 'while'");
    ast->keyword = parser->previous;
    consume(parser, TOKEN_LEFT_PAREN, "Expected '(");
    ast->control = Expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')'");
    consume(parser, TOKEN_SEMICOLON, "Expected ';'");

    // windows is fine without this, gdb shows segfault?!
    ast->freeCount = NULL;
    ast->post = NULL;
    ast->preDecl = NULL;
    ast->preExpr = NULL;

    return ast;
}

ASTJumpStatement* Break(Parser* parser) {
    ASTJumpStatement* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_JUMP_STATEMENT_BREAK;
    ast->statement = parser->previous;
    consume(parser, TOKEN_SEMICOLON, "Expected ';'");
    return ast;
}

ASTJumpStatement* Continue(Parser* parser) {
    ASTJumpStatement* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_JUMP_STATEMENT_CONTINUE;
    ast->statement = parser->previous;
    consume(parser, TOKEN_SEMICOLON, "Expected ';'");
    return ast;
}

ASTFN(Statement)
    if(match(parser, TOKEN_RETURN)) {
        ast->type = AST_STATEMENT_JUMP;
        ast->as.jump = ArenaAlloc(sizeof(*ast->as.jump));
        ast->as.jump->type = AST_JUMP_STATEMENT_RETURN;
        ast->as.jump->statement = parser->previous;
        ast->as.jump->expr = Expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expected ';'");
    } else if(match(parser, TOKEN_IF)) {
        ast->type = AST_STATEMENT_SELECTION;
        ast->as.selection = SelectionStatement(parser);
    } else if(match(parser, TOKEN_SEMICOLON)) {
        ast->type = AST_STATEMENT_NULL;
    } else if(match(parser, TOKEN_LEFT_BRACE)) {
        ast->type = AST_STATEMENT_COMPOUND;
        ast->as.compound = CompoundStatement(parser);
    } else if(match(parser, TOKEN_WHILE)) {
        ast->type = AST_STATEMENT_ITERATION;
        ast->as.iteration = While(parser);
    } else if(match(parser, TOKEN_FOR)) {
        ast->type = AST_STATEMENT_ITERATION;
        ast->as.iteration = For(parser);
    } else if(match(parser, TOKEN_DO)) {
        ast->type = AST_STATEMENT_ITERATION;
        ast->as.iteration = DoWhile(parser);
    } else if(match(parser, TOKEN_BREAK)) {
        ast->type = AST_STATEMENT_JUMP;
        ast->as.jump = Break(parser);
    } else if(match(parser, TOKEN_CONTINUE)) {
        ast->type = AST_STATEMENT_JUMP;
        ast->as.jump = Continue(parser);
    } else {
        ast->type = AST_STATEMENT_EXPRESSION;
        ast->as.expression = Expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expected ';'");
    }
ASTFN_END()

ASTFN(BlockItem)
    if(match(parser, TOKEN_INT)) {
        ast->type = AST_BLOCK_ITEM_DECLARATION;
        ast->as.declaration = Declaration(parser);
    } else {
        ast->type = AST_BLOCK_ITEM_STATEMENT;
        ast->as.statement = Statement(parser);
    }
ASTFN_END()

ASTFN(TranslationUnit)
    ARRAY_ALLOC(ASTDeclaration*, *ast, declaration);
    while(!match(parser, TOKEN_EOF)) {
        consume(parser, TOKEN_INT, "Expected 'int'");
        ARRAY_PUSH(*ast, declaration, Declaration(parser));
    }
ASTFN_END()

bool ParserRun(Parser* parser) {
    advance(parser);

    parser->ast = TranslationUnit(parser);

    return !parser->hadError;
}