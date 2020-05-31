#ifndef AST_LOWER_H
#define AST_LOWER_H

#include "ast.h"
#include "ir.h"

void astLower(ASTTranslationUnit* ast, IrContext* ir);

#endif