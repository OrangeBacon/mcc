#include "ast.h"

#include <stdio.h>

void ASTPrint(ASTTranslationUnit* ast) {
    printf("AST: size = %u\n", ast->declarationCount);

    for(unsigned int i = 0; i < ast->declarationCount; i++) {
        ASTFunctionDefinition* dec = ast->declarations[i]->as.functionDefinition;
        printf("function %.*s: size = %u\n", dec->name.length, dec->name.start,
            dec->statementCount);

        for(unsigned int j = 0; j < dec->statementCount; j++) {
            ASTStatement* stmt = dec->statements[j];
            switch(stmt->type) {
                case AST_STATEMENT_RETURN:
                    printf("\treturn\n");
            }
        }
    }
}