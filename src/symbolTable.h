#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <inttypes.h>
#include "memory.h"

typedef struct SymbolLocal {
    const char* name;
    unsigned int length;
    uint32_t hash;
    unsigned int scopeDepth;

    // used only by the backend, not in creating the ast
    int stackOffset;
} SymbolLocal;

typedef struct SymbolTable {
    ARRAY_DEFINE(SymbolLocal*, local);
    unsigned int currentDepth;
} SymbolTable;

typedef struct SymbolExitList {
    ARRAY_DEFINE(SymbolLocal*, local);
} SymbolExitList;

void SymbolTableInit(SymbolTable* table);

SymbolLocal* SymbolTableAddLocal(SymbolTable* table, const char* name, unsigned int length);

SymbolLocal* SymbolTableGetLocal(SymbolTable* table, const char* name, unsigned int length);

void SymbolTableEnter(SymbolTable* table);

SymbolExitList* SymbolTableExit(SymbolTable* table);

#endif