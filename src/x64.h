#ifndef X64_H
#define X64_H

#include "ast.h"

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

void x64ASTGen(ASTTranslationUnit* ast);

#endif