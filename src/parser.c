#include "parser.h"

#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stdlib.h>

void ParserInit(Parser* parser, Scanner* scanner) {
    parser->scanner = scanner;
    parser->panicMode = false;
    parser->hadError = false;
}

static void errorAt(Parser* parser, Token* loc, const char* message) {
    if(parser->panicMode) return;
    parser->panicMode = true;

    fprintf(stderr, "[%d:%d] Error: ", loc->line, loc->column);

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
    PREC_PRIMARY,
    PREC_POSTFIX,
    PREC_UNARY,
    PREC_CAST,
    PREC_MULTIPLICITIVE,
    PREC_ADDITIVE,
    PREC_SHIFT,
    PREC_RELATION,
    PREC_EQUALITY,
    PREC_BITAND,
    PREC_BITXOR,
    PREC_BITOR,
    PREC_LOGICAND,
    PREC_LOGICOR,
    PREC_CONDITIONAL,
    PREC_ASSIGN,
    PREC_COMMA,
    PREC_NONE,
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

static ASTExpression* ConstantExpression(Parser* parser) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_CONSTANT;
    ast->as.constant.integer = parser->previous;
    ast->as.constant.integer.numberValue = strtod(parser->previous.start, NULL);
    return ast;
}

static ASTExpression* UnaryExpression(Parser* parser) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_UANRY;
    ast->as.unary.operator = parser->previous;
    ast->as.unary.operand = parsePrecidence(parser, PREC_UNARY);
    return ast;
}

static ASTExpression* BinaryExpression(Parser* parser, ASTExpression* prev) {
    ASTExpression* ast = ArenaAlloc(sizeof(*ast));
    ast->type = AST_EXPRESSION_BINARY;
    ast->as.binary.operator = parser->previous;
    ast->as.binary.left = prev;

    ParseRule* rule = getRule(ast->as.binary.operator.type);
    ast->as.binary.right = parsePrecidence(parser, (Precidence)(rule->precidence + 1));

    return ast;
}

ParseRule rules[] = {
    [TOKEN_IDENTIFIER] =    {NULL,                  NULL,   PREC_NONE},
    [TOKEN_LEFT_PAREN] =    {NULL,                  NULL,   PREC_NONE},
    [TOKEN_RIGHT_PAREN] =   {NULL,                  NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE] =    {NULL,                  NULL,   PREC_NONE},
    [TOKEN_RIGHT_BRACE] =   {NULL,                  NULL,   PREC_NONE},
    [TOKEN_RETURN] =        {NULL,                  NULL,   PREC_NONE},
    [TOKEN_INTEGER] =       {ConstantExpression,    NULL,   PREC_NONE},
    [TOKEN_SEMICOLON] =     {NULL,                  NULL,   PREC_NONE},
    [TOKEN_INT] =           {NULL,                  NULL,   PREC_NONE},
    [TOKEN_ERROR] =         {NULL,                  NULL,   PREC_NONE},
    [TOKEN_EOF] =           {NULL,                  NULL,   PREC_NONE},
};

static ParseRule* getRule(TokenType type) {
    (void)BinaryExpression;
    (void)UnaryExpression;
    return &rules[type];
}

static ASTExpression* Expression(Parser* parser) {
    return parsePrecidence(parser, PREC_COMMA);
}

#define ASTFN(type) \
    static AST##type* type(Parser* parser) { \
    AST##type* ast = ArenaAlloc(sizeof(*ast));
#define ASTFN_END() \
    return ast; }

ASTFN(Statement)
    if(match(parser, TOKEN_RETURN)) {
        ast->type = AST_STATEMENT_RETURN;
        ast->as.return_ = Expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expected ';'");
    }
ASTFN_END()

ASTFN(BlockItem)
    ast->type = AST_BLOCK_ITEM_STATEMENT;
    ast->as.statement = Statement(parser);
ASTFN_END()

ASTFN(CompoundStatement)
    consume(parser, TOKEN_LEFT_BRACE, "Expected '{'");

    ARRAY_ALLOC(ASTBlockItem*, *ast, item);
    while(!match(parser, TOKEN_EOF)) {
        //ARRAY_PUSH(*ast, item, BlockItem(parser));
        do {
            if(sizeof(BlockItem(parser)) != (*ast).itemElementSize) {
                printf("Push to array with incorrect item size (%zu), array item " "size is %u at %s:%d\n", sizeof(BlockItem(parser)), (*ast).itemElementSize, "C:\\Users\\Will\\Documents\\repos\\mcc\\src\\parser.c", 97);
            }
            if((*ast).itemCount == (*ast).itemCapacity) {
                (*ast).items = ArenaReAlloc((*ast).items, (*ast).itemElementSize * (*ast).itemCapacity, (*ast).itemElementSize * (*ast).itemCapacity * 2);
                (*ast).itemCapacity *= 2;
            }
            ASTBlockItem* b = BlockItem(parser);
            ast->items[0] = b;
            (*ast).itemCount++;
        } while(0);

        if(parser->current.type == TOKEN_RIGHT_BRACE) break;
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}");
ASTFN_END()

ASTFN(FunctionDefinition)
    consume(parser, TOKEN_IDENTIFIER, "Expected function name");
    ast->name = parser->previous;
    consume(parser, TOKEN_LEFT_PAREN, "Expected '('");
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')'");
    ast->statement = CompoundStatement(parser);
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