#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <inttypes.h>
#include "memory.h"

typedef struct SymbolLocal {
    char* name;
    unsigned int length;
    uint32_t hash;
    unsigned int scopeDepth;
    unsigned int stackOffset;
} SymbolLocal;

typedef struct SymbolTable {
    ARRAY_DEFINE(SymbolLocal, local);
    unsigned int currentDepth;
} SymbolTable;

void SymbolTableInit(SymbolTable* table);

SymbolLocal* SymbolTableAddLocal(SymbolTable* table);

SymbolLocal* SymbolTableGetLocal(SymbolTable* table, char* name, unsigned int length);

void SymbolTableEnter(SymbolTable* table);

void SymbolTableExit(SymbolTable* table);

#endif