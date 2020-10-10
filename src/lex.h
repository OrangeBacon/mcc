#ifndef LEX_H
#define LEX_H

#include <stdint.h>
#include <stdbool.h>
#include "memory.h"
#include "file.h"
#include "symbolTable.h"
#include "lexString.h"

typedef struct SourceLocation {
    const unsigned char* fileName;
    size_t line;
    size_t column;
    size_t length;
    const unsigned char* sourceText;
} SourceLocation;

// What sort of token is it, used for both preprocessor and regular
// tokens, however not all values are valid in each scenario
typedef enum LexerTokenType {
    TOKEN_KW_AUTO,
    TOKEN_KW_BREAK,
    TOKEN_KW_CASE,
    TOKEN_KW_CHAR,
    TOKEN_KW_CONST,
    TOKEN_KW_CONTINUE,
    TOKEN_KW_DEFAULT,
    TOKEN_KW_DO,
    TOKEN_KW_DOUBLE,
    TOKEN_KW_ELSE,
    TOKEN_KW_ENUM,
    TOKEN_KW_EXTERN,
    TOKEN_KW_FLOAT,
    TOKEN_KW_FOR,
    TOKEN_KW_GOTO,
    TOKEN_KW_IF,
    TOKEN_KW_INLINE,
    TOKEN_KW_INT,
    TOKEN_KW_LONG,
    TOKEN_KW_REGISTER,
    TOKEN_KW_RESTRICT,
    TOKEN_KW_RETURN,
    TOKEN_KW_SHORT,
    TOKEN_KW_SIGNED,
    TOKEN_KW_SIZEOF,
    TOKEN_KW_STATIC,
    TOKEN_KW_STRUCT,
    TOKEN_KW_SWITCH,
    TOKEN_KW_TYPEDEF,
    TOKEN_KW_UNION,
    TOKEN_KW_UNSIGNED,
    TOKEN_KW_VOID,
    TOKEN_KW_VOLATILE,
    TOKEN_KW_WHILE,
    TOKEN_KW_ALIGNAS,
    TOKEN_KW_ALIGNOF,
    TOKEN_KW_ATOMIC,
    TOKEN_KW_BOOL,
    TOKEN_KW_COMPLEX,
    TOKEN_KW_GENERIC,
    TOKEN_KW_IMAGINARY,
    TOKEN_KW_NORETURN,
    TOKEN_KW_STATICASSERT,
    TOKEN_KW_THREADLOCAL,
    TOKEN_PUNC_LEFT_SQUARE,
    TOKEN_PUNC_RIGHT_SQUARE,
    TOKEN_PUNC_LEFT_PAREN,
    TOKEN_PUNC_RIGHT_PAREN,
    TOKEN_PUNC_LEFT_BRACE,
    TOKEN_PUNC_RIGHT_BRACE,
    TOKEN_PUNC_DOT,
    TOKEN_PUNC_ARROW,
    TOKEN_PUNC_PLUS_PLUS,
    TOKEN_PUNC_MINUS_MINUS,
    TOKEN_PUNC_AND,
    TOKEN_PUNC_STAR,
    TOKEN_PUNC_PLUS,
    TOKEN_PUNC_MINUS,
    TOKEN_PUNC_TILDE,
    TOKEN_PUNC_BANG,
    TOKEN_PUNC_SLASH,
    TOKEN_PUNC_PERCENT,
    TOKEN_PUNC_LESS_LESS,
    TOKEN_PUNC_GREATER_GREATER,
    TOKEN_PUNC_LESS,
    TOKEN_PUNC_GREATER,
    TOKEN_PUNC_LESS_EQUAL,
    TOKEN_PUNC_GREATER_EQUAL,
    TOKEN_PUNC_EQUAL_EQUAL,
    TOKEN_PUNC_BANG_EQUAL,
    TOKEN_PUNC_CARET,
    TOKEN_PUNC_OR,
    TOKEN_PUNC_AND_AND,
    TOKEN_PUNC_OR_OR,
    TOKEN_PUNC_QUESTION,
    TOKEN_PUNC_COLON,
    TOKEN_PUNC_SEMICOLON,
    TOKEN_PUNC_ELIPSIS,
    TOKEN_PUNC_EQUAL,
    TOKEN_PUNC_STAR_EQUAL,
    TOKEN_PUNC_SLASH_EQUAL,
    TOKEN_PUNC_PERCENT_EQUAL,
    TOKEN_PUNC_PLUS_EQUAL,
    TOKEN_PUNC_MINUS_EQUAL,
    TOKEN_PUNC_LESS_LESS_EQUAL,
    TOKEN_PUNC_GREATER_GREATER_EQUAL,
    TOKEN_PUNC_AND_EQUAL,
    TOKEN_PUNC_CARET_EQUAL,
    TOKEN_PUNC_PIPE_EQUAL,
    TOKEN_PUNC_COMMA,
    TOKEN_PUNC_HASH,
    TOKEN_PUNC_HASH_HASH,
    TOKEN_PUNC_LESS_COLON, // [
    TOKEN_PUNC_COLON_GREATER, // ]
    TOKEN_PUNC_LESS_PERCENT, // {
    TOKEN_PUNC_PERCENT_GREATER, // }
    TOKEN_PUNC_PERCENT_COLON, // #
    TOKEN_PUNC_PERCENT_COLON_PERCENT_COLON, // ##
    TOKEN_HEADER_NAME,
    TOKEN_SYS_HEADER_NAME,
    TOKEN_PP_NUMBER,
    TOKEN_IDENTIFIER_L,
    TOKEN_INTEGER_L,
    TOKEN_FLOATING_L,
    TOKEN_CHARACTER_L,
    TOKEN_STRING_L,
    TOKEN_MACRO_ARG,
    TOKEN_UNKNOWN_L,
    TOKEN_PLACEHOLDER_L,
    TOKEN_ERROR_L,
    TOKEN_EOF_L,
} LexerTokenType;

// the smallest non-character unit of code
typedef struct LexerToken {
    LexerTokenType type;

    // required to prevent extra macro expansion
    bool isStartOfLine;

    // should the token be printed on a new line
    bool renderStartOfLine;

    // required to tell the difference between
    // #define(a)  - function macro
    // #define (a) - value macro
    bool whitespaceBefore;

    // stores the quantity of whitespace before the token on the same
    // line as the token (note whitespaceBefore can be true, while this
    // is equal to 0, so need both of them)
    size_t indent;

    // where the token is in the source file, used for emmitting errors
    // and debugging infomation
    SourceLocation* loc;

    // optional data about the token, what is stored here is dependant
    // upon token type, could be nothing/uninitialised
    union {
        intmax_t integer;
        double floating;
        LexerString string;
        char character;
        struct {
            struct HashNode* node;
            bool attemptExpansion;
        };
    } data;
} LexerToken;

typedef struct Phase1Context {
    unsigned char* source;
    size_t sourceLength;
    size_t consumed;
    SourceLocation location;
    unsigned char ignoreNewLine;
    struct TranslationContext* settings;
} Phase1Context;

typedef struct Phase2Context {
    unsigned char peek;
    SourceLocation peekLoc;
    unsigned char previous;
    SourceLocation currentLoc;
    Phase1Context phase1;
} Phase2Context;

typedef enum Phase3LexMode {
    LEX_MODE_MAYBE_HEADER,
    LEX_MODE_NO_HEADER
} Phase3LexMode;

typedef struct Phase3Context {
    Phase3LexMode mode;
    unsigned char peek;
    SourceLocation peekLoc;
    unsigned char peekNext;
    SourceLocation peekNextLoc;
    SourceLocation* currentLocation;
    bool AtStart;
    void* getterCtx;
    unsigned char (*getter)(void* ctx, SourceLocation* loc);
    Table* hashNodes;
    Phase2Context phase2;
    struct TranslationContext* settings;
} Phase3Context;

typedef enum HashNodeType {
    NODE_MACRO_OBJECT,
    NODE_MACRO_STRING,
    NODE_MACRO_INTEGER,
    NODE_MACRO_FUNCTION,
    NODE_MACRO_LINE,
    NODE_MACRO_FILE,
    NODE_VOID,
} HashNodeType;

typedef struct TokenList {
    ARRAY_DEFINE(LexerToken, item);
} TokenList;

typedef struct FnMacro {
    ARRAY_DEFINE(LexerToken, argument);
    ARRAY_DEFINE(LexerToken, replacement);
    bool isVariadac;
} FnMacro;

typedef struct HashNode {
    LexerToken name;
    HashNodeType type;
    uint32_t hash;

    bool macroExpansionEnabled;

    union {
        TokenList object;
        FnMacro function;
        const char* string;
        intmax_t integer;
    } as;
} HashNode;

typedef struct MacroContext {
    LexerToken* tokens;
    size_t tokenCount;
} MacroContext;

typedef enum Phase4LexMode {
    LEX_MODE_INCLUDE,
    LEX_MODE_NO_INCLUDE,
} Phase4LexMode;

typedef struct Phase4Context {
    Phase4LexMode mode;
    LexerToken peek;
    struct TranslationContext* includeContext;
    struct TranslationContext* parent;
    IncludeSearchState searchState;
    unsigned char depth;
    MacroContext macroCtx;

    // stores the previous token emitted at base context (no macro expansion)
    // used for correct __LINE__ and __FILE__ interpretation
    LexerToken previous;
} Phase4Context;

// random data used by each translation phase that needs to be stored
typedef struct TranslationContext {
    // settings
    bool trigraphs;
    size_t tabSize;
    IncludeSearchPath search;
    MemoryPool* pool;

    // state
    const unsigned char* fileName;

    Phase3Context phase3;
    Phase4Context phase4;

    // memory allocators
    MemoryArray stringArr;
    MemoryArray locations;
} TranslationContext;

void TranslationContextInit(TranslationContext* ctx, MemoryPool* pool, const unsigned char* fileName);

void runPhase1(TranslationContext* ctx);
void runPhase2(TranslationContext* ctx);
void runPhase3(TranslationContext* ctx);
void runPhase4(TranslationContext* ctx);

#endif