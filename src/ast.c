#include "ast.h"

#include <stdio.h>

void ASTPrint(ASTTranslationUnit* ast) {
    printf("AST: size = %u\n", ast->declarationCount);
}