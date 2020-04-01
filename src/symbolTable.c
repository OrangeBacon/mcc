#include "symbolTable.h"

static uint32_t stringHash(const char* str, unsigned int length) {
    uint32_t hash = 2166126261u;

    for(size_t i = 0; i < length; i++) {
        hash ^= str[i];
        hash *= 16777619;
    }

    return hash;
}

void SymbolTableInit(SymbolTable* table) {
    ARRAY_ALLOC(SymbolLocal*, *table, local);
}


// Add a new local variable to the current scope
// output only valid while no more locals added to the table as this function
// could change the location of the symbol table in memory when it is extended
SymbolLocal* SymbolTableAddLocal(SymbolTable* table, const char* name, unsigned int length) {

    // check for repeat definitions
    SymbolLocal* local = SymbolTableGetLocal(table, name, length);
    if(local != NULL && local->scopeDepth == table->currentDepth) {
        return NULL;
    }

    SymbolLocal* ret = ArenaAlloc(sizeof(*local));
    ARRAY_PUSH(*table, local, ret);
    ret->scopeDepth = table->currentDepth;
    ret->hash = stringHash(name, length);
    ret->name = name;
    ret->length = length;

    return ret;
}

SymbolLocal* SymbolTableGetLocal(SymbolTable* table, const char* name, unsigned int length) {
    uint32_t hash = stringHash(name, length);

    for(int i = table->localCount - 1; i >= 0; i--) {
        if(table->locals[i]->length == length && table->locals[i]->hash == hash) {
            return table->locals[i];
        }
    }

    return NULL;
}

void SymbolTableEnter(SymbolTable* table) {
    table->currentDepth++;
}

SymbolExitList* SymbolTableExit(SymbolTable* table) {
    SymbolExitList* ret = ArenaAlloc(sizeof(*ret));
    ARRAY_ALLOC(SymbolLocal*, *ret, local);

    table->currentDepth--;
    while(table->localCount > 0 &&
          table->locals[table->localCount - 1]->scopeDepth > table->currentDepth) {
        ARRAY_PUSH(*ret, local, table->locals[table->localCount - 1]);
        table->localCount--;
    }

    return ret;
}