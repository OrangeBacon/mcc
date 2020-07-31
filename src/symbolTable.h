#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <inttypes.h>
#include "memory.h"

// Hash table implementation

typedef struct Key {
    const char* key;
    unsigned int length;
    uint32_t hash;
} Key;

typedef struct Entry {
    Key key;

    void* value;
} Entry;

typedef struct Table {
    // hash table
    ARRAY_DEFINE(Entry, entry);

    size_t valueSize;
} Table;

void adjustCapacity(Table* table, unsigned int capacity);
void tableSet(Table* table, const char* key, unsigned int length, void* value);
void* tableGet(Table* table, const char* key, unsigned int length);
bool tableHas(Table* table, const char* key, unsigned int length);
void tableRemove(Table* table, const char* key, unsigned int length);

typedef struct SymbolLocal {
    const char* name;
    unsigned int length;
    uint32_t hash;
    unsigned int scopeDepth;

    const struct ASTVariableType* type;

    // used only by the backend, not in creating the ast
    int stackOffset;

    struct IrParameter* vreg;
    int parameterNumber;

    bool memoryRequired : 1;
    bool vregToAlloca : 1;
    bool initialised : 1;
    bool globalSymbolGenDone : 1;
    bool toGenerateParameter : 1;
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


// hash table convinience macros

#define TABLE_MAX_LOAD 0.75

#define TABLE_INIT(table, vType) \
    do { \
        (table).valueSize = sizeof(vType); \
        ARRAY_ZERO((table), entry); \
        adjustCapacity(&(table), 8); \
    } while(0)

#define TABLE_SET(table, key, length, value) \
    do { \
        if(sizeof(value) != (table).valueSize) { \
            printf("Seting table member based on incorrect value size, correct " \
                "is %zu, got %zu\n", (table).valueSize, sizeof(value)); \
        } \
        tableSet(&(table), (char*)(key), (length), (void*)(value)); \
    } while(0)


// SSA variable lookup hash table

typedef struct PairKey {
    struct SymbolLocal* key1;
    struct IrBasicBlock* key2;
    uint32_t hash;
} PairKey;

typedef struct PairEntry {
    PairKey key;

    void* value;
} PairEntry;

typedef struct PairTable {
    // hash table
    ARRAY_DEFINE(PairEntry, entry);

    size_t valueSize;
} PairTable;

void pairAdjustCapacity(PairTable* table, unsigned int capacity);
void pairTableSet(PairTable* table, struct SymbolLocal* key1, struct IrBasicBlock* key2, void* value);
void* pairTableGet(PairTable* table,  struct SymbolLocal* key1, struct IrBasicBlock* key2);
bool pairPableHas(PairTable* table,  struct SymbolLocal* key1, struct IrBasicBlock* key2);

#define PAIRTABLE_INIT(table, vType) \
    do { \
        (table).valueSize = sizeof(vType); \
        ARRAY_ZERO((table), entry); \
        pairAdjustCapacity(&(table), 8); \
    } while(0)

#define PAIRTABLE_SET(table, key1, key2, value) \
    do { \
        if(sizeof(value) != (table).valueSize) { \
            printf("Seting pairtable member based on incorrect value size, correct " \
                "is %zu, got %zu\n", (table).valueSize, sizeof(value)); \
        } \
        pairTableSet(&(table), (key1), (key2), (void*)(value)); \
    } while(0)

#endif