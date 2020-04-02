#include "symbolTable.h"

#include <string.h>

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
    TABLE_INIT(table->globals, SymbolGlobal);
    table->currentDepth = 0;
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

SymbolGlobal* SymbolTableAddGlobal(SymbolTable* table, const char* name, unsigned int length) {
    SymbolGlobal* global = SymbolTableGetGlobal(table, name, length);
    if(global != NULL) {
        return NULL;
    }

    SymbolGlobal* ret = ArenaAlloc(sizeof(*global));
    ret->name = name;
    ret->length = length;
    ret->hash = stringHash(name, length);

    TABLE_SET(table->globals, name, length, ret);

    return ret;
}

SymbolGlobal* SymbolTableGetGlobal(SymbolTable* table, const char* name, unsigned int length) {
    return tableGet(&table->globals, name, length);
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

// find an entry in a list of entries
static Entry* findEntry(Entry* entries, int capacity, Key* key) {

    // location to start searching
    uint32_t index = key->hash % capacity;

    while(true) {
        // value to check
        Entry* entry = &entries[index];

        // if run out of entries to check or the correct entry is found,
        // return current search item
        if(entry->key.key == NULL || (entry->key.length == key->length && strncmp(entry->key.key, key->key, key->length) == 0)) {
            return entry;
        }

        // get next location, wrapping round to index 0
        index = (index + 1) % capacity;
    }
}

// increase the size of a table to the given capacity
void adjustCapacity(Table* table, unsigned int capacity) {

    // create new data section and null initialise
    Entry* entries = ArenaAlloc(sizeof(Entry) * capacity);
    for(unsigned int i = 0; i < capacity; i++) {
        entries[i].key.key = NULL;
        entries[i].value = NULL;
    }

    // copy data from the table to the new data section
    // re-organises table for new memory size not just
    // copying the data across.
    if(table->entryCount > 0) {
        for(unsigned int i = 0; i < table->entryCapacity; i++) {
            Entry* entry = &table->entrys[i];
            if(entry->key.key == NULL) {
                continue;
            }

            Entry* dest = findEntry(entries, capacity, &entry->key);
            dest->key = entry->key;
            dest->value = entry->value;
        }
    }

    // set the table's data to the new data
    table->entrys = entries;
    table->entryCapacity = capacity;
}

void tableSet(Table* table, const char* keyPtr, unsigned int length, void* valuePtr) {
    // create the key to be inserted into the table
    Key key;
    key.key = keyPtr;
    key.length = length;
    key.hash = stringHash(keyPtr, length);

    // make sure the table is big enough
    if(table->entryCount + 1 > table->entryCapacity * TABLE_MAX_LOAD) {
        int capacity = table->entryCapacity;
        capacity = capacity < 8 ? 8 : capacity * 2;
        adjustCapacity(table, capacity);
    }

    // get the entry to be set
    Entry* entry = findEntry(table->entrys, table->entryCapacity, &key);

    // does the entry have any content?
    bool isNewKey = entry->key.key == NULL;
    if(isNewKey) {
        table->entryCount++;
    }

    entry->key = key;
    entry->value = valuePtr;
}

void* tableGet(Table* table, const char* keyPtr, unsigned int length) {
    // if nothing has been set then get will always be false
    if(table->entrys == NULL) {
        return false;
    }

    // create key to search for
    Key key;
    key.key = keyPtr;
    key.length = length;
    key.hash = stringHash(keyPtr, length);

    // find location where the key should be
    Entry* entry = findEntry(table->entrys, table->entryCapacity, &key);
    if(entry->key.key == NULL) {
        return NULL;
    }

    return entry->value;
}

bool tableHas(Table* table, const char* key, unsigned int length) {
    if(table->entrys == NULL) {
        return false;
    }

    Key test;
    test.key = key;
    test.length = length;
    test.hash = stringHash(key, length);

    Entry* entry = findEntry(table->entrys, table->entryCapacity, &test);

    // if the item is in the table, its key will not be null
    return !(entry->key.key == NULL);
}

void tableRemove(Table* table, const char* key, unsigned int length) {
    if(table->entrys == NULL) {
        return;
    }

    Key test;
    test.key = key;
    test.length = length;
    test.hash = stringHash(key, length);

    Entry* entry = findEntry(table->entrys, table->entryCapacity, &test);
    entry->key.key = NULL;
    entry->value = NULL;
}