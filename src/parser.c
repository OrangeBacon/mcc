#include "parser.h"

#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stdlib.h>

void ParserInit(Parser* parser, Scanner* scanner) {
    parser->scanner = scanner;
    parser->panicMode = false;
    parser->hadError = false;
    SymbolTableInit(&parser->locals);
}

static void errorAt(Parser* parser, Token* loc, const char* message) {
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
    ast->as.constant.type = AST_CONSTANT_EXPRESSION_VARIABLE;
    ast->as.constant.tok = parser->previous;

    SymbolLocal* local = SymbolTableGetLocal(&parser->locals,
        parser->previous.start, parser->previous.length);
    if(local == NULL) {
        error(parser, "Variable name not declared");
        return ast;
    }
    ast->as.constant.stackDepth = local->stackOffset;

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
    return ast;
}

static ASTExpression* PreIncDec(Parser* parser) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_UNARY;
    ast->as.unary.operator = parser->previous;

    ASTExpression* exp = parsePrecidence(parser, PREC_UNARY);
    if(exp->type != AST_EXPRESSION_CONSTANT) {
        error(parser, "Expected constant before assignement");
        return NULL;
    }
    if(exp->as.constant.type != AST_CONSTANT_EXPRESSION_VARIABLE) {
        error(parser, "Cannot assign to non variable expression");
        return NULL;
    }

    SymbolLocal* local = SymbolTableGetLocal(&parser->locals,
        exp->as.constant.tok.start, exp->as.constant.tok.length);
    if(local == NULL) {
        error(parser, "Variable name not declared");
        return ast;
    }

    ast->as.unary.operand = exp;
    ast->as.unary.stackOffset = local->stackOffset;

    return ast;
}

static ASTExpression* Binary(Parser* parser, ASTExpression* prev) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_BINARY;
    ast->as.binary.operator = parser->previous;
    ast->as.binary.left = prev;

    ParseRule* rule = getRule(ast->as.binary.operator.type);
    ast->as.binary.right = parsePrecidence(parser, (Precidence)(rule->precidence + 1));

    return ast;
}

static ASTExpression* Assign(Parser* parser, ASTExpression* prev) {
    if(prev->type != AST_EXPRESSION_CONSTANT) {
        error(parser, "Expected constant before assignement");
        return NULL;
    }
    if(prev->as.constant.type != AST_CONSTANT_EXPRESSION_VARIABLE) {
        error(parser, "Cannot assign to non variable expression");
        return NULL;
    }

    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_ASSIGN;
    ast->as.assign.operator = parser->previous;
    ast->as.assign.value = parsePrecidence(parser, PREC_ASSIGN);

    SymbolLocal* local = SymbolTableGetLocal(&parser->locals,
        prev->as.constant.tok.start, prev->as.constant.tok.length);
    if(local == NULL) {
        error(parser, "Variable name not declared");
        return ast;
    }

    ast->as.assign.stackOffset = local->stackOffset;

    return ast;
}

static ASTExpression* PostIncDec(Parser* parser, ASTExpression* prev) {
    if(prev->type != AST_EXPRESSION_CONSTANT) {
        error(parser, "Expected constant before post inc/dec operator");
        return NULL;
    }
    if(prev->as.constant.type != AST_CONSTANT_EXPRESSION_VARIABLE) {
        error(parser, "Cannot post inc/dec non variable expression");
        return NULL;
    }

    SymbolLocal* local = SymbolTableGetLocal(&parser->locals,
        prev->as.constant.tok.start, prev->as.constant.tok.length);
    if(local == NULL) {
        error(parser, "Variable name not declared");
        return NULL;
    }

    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_POSTFIX;
    ast->as.postfix.operator = parser->previous;
    ast->as.postfix.operand = prev;
    ast->as.postfix.stackOffset = local->stackOffset;

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
    [TOKEN_LEFT_PAREN] =        { Grouping,  NULL,       PREC_NONE           },
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
    [TOKEN_STAR] =              { NULL,      Binary,     PREC_MULTIPLICITIVE },
    [TOKEN_SLASH] =             { NULL,      Binary,     PREC_MULTIPLICITIVE },
    [TOKEN_AND_AND] =           { NULL,      Binary,     PREC_LOGICAND       },
    [TOKEN_OR_OR] =             { NULL,      Binary,     PREC_LOGICOR        },
    [TOKEN_EQUAL_EQUAL] =       { NULL,      Binary,     PREC_EQUALITY       },
    [TOKEN_NOT_EQUAL] =         { NULL,      Binary,     PREC_EQUALITY       },
    [TOKEN_LESS] =              { NULL,      Binary,     PREC_RELATION       },
    [TOKEN_LESS_EQUAL] =        { NULL,      Binary,     PREC_RELATION       },
    [TOKEN_GREATER] =           { NULL,      Binary,     PREC_RELATION       },
    [TOKEN_GREATER_EQUAL] =     { NULL,      Binary,     PREC_RELATION       },
    [TOKEN_AND] =               { NULL,      Binary,     PREC_BITAND         },
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

ASTFN(InitDeclarator)
    consume(parser, TOKEN_IDENTIFIER, "Expected variable name");
    ast->declarator = parser->previous;
    SymbolLocal* local = SymbolTableAddLocal(&parser->locals,
        ast->declarator.start, ast->declarator.length);
    if(local == NULL) {
        error(parser, "Cannot re-declare variable in same scope");
        return NULL;
    }
    local->stackOffset = parser->stackIndex;
    parser->stackIndex -= 8;

    if(match(parser, TOKEN_EQUAL)) {
        ast->type = AST_INIT_DECLARATOR_INITIALIZE;
        ast->initializer = parsePrecidence(parser, PREC_ASSIGN);
    } else {
        ast->type = AST_INIT_DECLARATOR_NO_INITIALIZE;
    }
ASTFN_END()

ASTFN(Declaration)
    ARRAY_ALLOC(ASTInitDeclarator*, ast->declarators, declarator);

    while(!match(parser, TOKEN_EOF)) {
        if(!check(parser, TOKEN_IDENTIFIER)) break;
        ARRAY_PUSH(ast->declarators, declarator, InitDeclarator(parser));
        if(!match(parser, TOKEN_COMMA)) break;
    }

    consume(parser, TOKEN_SEMICOLON, "Expected ';'");
ASTFN_END()

static ASTStatement* Statement(Parser*);

ASTFN(SelectionStatement)
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
        ARRAY_PUSH(*ast, item, BlockItem(parser));
        if(parser->current.type == TOKEN_RIGHT_BRACE) break;
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}'");
    ast->popCount = SymbolTableExit(&parser->locals);
    parser->stackIndex += 8 * ast->popCount;
ASTFN_END()

ASTIterationStatement* While(Parser* parser) {
    ASTIterationStatement* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_ITERATION_STATEMENT_WHILE;

    consume(parser, TOKEN_LEFT_PAREN, "Expected '('");
    ast->control = Expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')'");
    ast->body = Statement(parser);

    return ast;
}

ASTIterationStatement* For(Parser* parser) {
    ASTIterationStatement* ast = ArenaAlloc(sizeof(*ast));

    SymbolTableEnter(&parser->locals);

    consume(parser, TOKEN_LEFT_PAREN, "Expected '(");
    if(match(parser, TOKEN_INT)) {
        ast->type = AST_ITERATION_STATEMENT_FOR_DECL;
        ast->preDecl = Declaration(parser);
    } else if(match(parser, TOKEN_SEMICOLON)) {
        ast->type = AST_ITERATION_STATEMENT_FOR_EXPR;
        ast->preExpr = NULL;
    } else {
        ast->type = AST_ITERATION_STATEMENT_FOR_EXPR;
        ast->preExpr = Expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expected ';'");
    }

    if(match(parser, TOKEN_SEMICOLON)) {
        ast->control = NULL;
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
    parser->stackIndex += 8 * ast->freeCount;

    return ast;
}

ASTIterationStatement* DoWhile(Parser* parser) {
    ASTIterationStatement* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_ITERATION_STATEMENT_DO;

    ast->body = Statement(parser);

    consume(parser, TOKEN_WHILE, "Expected 'while'");
    consume(parser, TOKEN_LEFT_PAREN, "Expected '(");
    ast->control = Expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expectedd ')'");
    consume(parser, TOKEN_SEMICOLON, "Expected ';'");

    return ast;
}

ASTFN(Statement)
    if(match(parser, TOKEN_RETURN)) {
        ast->type = AST_STATEMENT_RETURN;
        ast->as.return_ = Expression(parser);
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

ASTFN(FnCompoundStatement)
    SymbolTableEnter(&parser->locals);
    consume(parser, TOKEN_LEFT_BRACE, "Expected '{'");

    ARRAY_ALLOC(ASTBlockItem*, *ast, item);
    while(!match(parser, TOKEN_EOF)) {
        ARRAY_PUSH(*ast, item, BlockItem(parser));
        if(parser->current.type == TOKEN_RIGHT_BRACE) break;
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}'");
    SymbolTableExit(&parser->locals);
ASTFN_END()

ASTFN(FunctionDefinition)
    consume(parser, TOKEN_IDENTIFIER, "Expected function name");
    ast->name = parser->previous;
    consume(parser, TOKEN_LEFT_PAREN, "Expected '('");
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')'");
    parser->stackIndex = -8;
    ast->statement = FnCompoundStatement(parser);
ASTFN_END()

ASTFN(ExternalDeclaration)
    ast->type = AST_EXTERNAL_DECLARATION_FUNCTION_DEFINITION;
    consume(parser, TOKEN_INT, "Expected 'int'");

    ast->as.functionDefinition = FunctionDefinition(parser);

ASTFN_END()

ASTFN(TranslationUnit)
    ARRAY_ALLOC(ASTExternalDeclaration*, *ast, declaration);
    while(!match(parser, TOKEN_EOF)) {
        ARRAY_PUSH(*ast, declaration, ExternalDeclaration(parser));
    }
ASTFN_END()

bool ParserRun(Parser* parser) {
    advance(parser);

    parser->ast = TranslationUnit(parser);

    return !parser->hadError;
}