#ifndef MEMORY_H
#define MEMORY_H

#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

// section of memory
typedef struct Area {
    size_t bytesLeft;
    void* end;
} Area;

// contains all memory in the program
typedef struct Arena {
    Area* areas;
    size_t arenaCount;
    size_t pageSize;
    size_t align;
} Arena;

// initialise the arena
void ArenaInit();

// allocate memory in arena with default alignment
void* ArenaAlloc(size_t size);

// allocate memory in arena
// align(bytes) bust be a power of 2
void* ArenaAllocAlign(size_t size, size_t align);

// resize a pointer alloced in the arena
void* ArenaReAlloc(void* old_ptr, size_t old_size, size_t new_size);

// declare a new array in the current scope
#define ARRAY_DEFINE(type, name) \
    type* name##s; \
    unsigned int name##Count; \
    unsigned int name##Capacity; \
    unsigned int name##ElementSize

// initialise an array with 0 capacity
#define ARRAY_ZERO(container, name) \
    do { \
        (container).name##s = NULL; \
        (container).name##Count = 0; \
        (container).name##Capacity = 0; \
        (container).name##ElementSize = 0; \
    } while(0)

// initialise the array
#define ARRAY_ALLOC(type, container, name) \
    do { \
        (container).name##Count = 0; \
        (container).name##Capacity = 8; \
        (container).name##s = ArenaAlloc(sizeof(type) * (container).name##Capacity); \
        (container).name##ElementSize = sizeof(type); \
    } while(0)

#define MEMQUOTE(x) #x

#define ARRAY_PUSH(container, name, value) \
    do { \
        if(sizeof(value) != (container).name##ElementSize) { \
            printf("Push to array with incorrect item size (%zu), array item " \
                "size is %u at %s:%d\n", sizeof(value), \
            (container).name##ElementSize, __FILE__, __LINE__); \
        } \
        if((container).name##Count == (container).name##Capacity) { \
            (container).name##s = ArenaReAlloc((container).name##s, \
                (container).name##ElementSize * (container).name##Capacity, \
                (container).name##ElementSize * (container).name##Capacity * 2);\
            (container).name##Capacity *= 2; \
        } \
        (container).name##s[(container).name##Count] = (value); \
        (container).name##Count++; \
    } while(0)

// return and remove the last item in an array
#define ARRAY_POP(container, name) \
    ((container).name##Count--,(container).name##s[(container).name##Count])

// format a string into a newly allocated buffer valid for the life of
// the compiler - avoids storing va_list
char* aprintf(const char* format, ...);

char* vaprintf(const char* format, va_list args);

// container to hold virtual, non-committed memory areas
typedef struct MemoryPool {

    // pointer to reserved, unusable virtual memory
    void* memory;

    // how many bytes have been given out to be allocated
    size_t bytesUsed;

    // size of the allocated memory
    size_t pageSize;

    // minimum allocation size
    size_t allocGranularity;
} MemoryPool;

// array holding single size objects, allocated out of a memory pool
typedef struct MemoryArray {

    // pointer to start of usable memory
    void* memory;

    // how much of the buffer has been used
    size_t bytesUsed;

    // how much of the buffer has been committed
    size_t bytesCommitted;

    // how big each item is
    size_t itemSize;

    // maximum size of the array
    size_t pageSize;

    // minimum allocation size
    size_t allocGranularity;

    // incase more virtual memory is needed
    MemoryPool* pool;

    size_t itemsPerPage;
} MemoryArray;

// create a new memory pool
void memoryPoolAlloc(MemoryPool* pool, size_t pageSize);

// create a new memory array from a pool
void memoryArrayAlloc(MemoryArray* arr, MemoryPool* pool, size_t pageSize, size_t itemSize);

// get a pointer to a new item at the end of the array
void* memoryArrayPush(MemoryArray* arr);

// get item by index
void* memoryArrayGet(MemoryArray* arr, size_t idx);

#endif