#define __USE_MINGW_ANSI_STDIO 1

#include "memory.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

// global instance of arena,
// only place memory is stored in the program
// is not freed, assumes enough memory for all
// alocations to exist
static Arena arena;

// increments pointer until it is aligned to align
// align (bytes) must be a power of 2
// returns the amount the pointer was incremented by
// modifies the pointer
static int AlignForward(void* real_ptr, size_t align) {
    uintptr_t ptr = (uintptr_t)real_ptr;
    uintptr_t align_ptr = align;

    uintptr_t modulo = ptr % align_ptr;

    return modulo==0?0:align_ptr - modulo;
}

void ArenaInit() {
    arena.pageSize = 4096 * 16;

    // add an initial page
    arena.align = 2 * sizeof(void*);
    arena.areas = malloc(sizeof(Arena));
    arena.areas[0].end = malloc(arena.pageSize);
    arena.arenaCount = 1;
    if(arena.areas == NULL || arena.areas[0].end == NULL) {
        printf("Could not allocate memory for arena");
        exit(1);
    }
    arena.areas[0].bytesLeft = arena.pageSize;
}

void* ArenaAlloc(size_t size) {
    return ArenaAllocAlign(size, arena.align);
}

void* ArenaAllocAlign(size_t size, size_t align) {
    void* ptr = NULL;
    size_t i;

    // find area with enough remaining memory
    for(i = 0; i < arena.arenaCount; i++){
        // ensure there is enough space incase the pointer needs re-aligning
        if(arena.areas[i].bytesLeft > size + align){
            ptr = (void*)arena.areas[i].end;
            break;
        }
    }

    // no large enough area found so add new area that is large enough
    if(ptr == NULL) {
        // ensure that new area will be large enough
        if(arena.pageSize < size) {
            arena.pageSize = size;
        }

        // expand the arena base pointer array
        arena.arenaCount++;
        arena.areas = realloc(arena.areas, sizeof(Arena)*arena.arenaCount);
        if(arena.areas == NULL) {
            printf("Could not expand arena area list from %zu to %zu",
                arena.arenaCount-1, arena.arenaCount);
            exit(1);
        }

        // allocate the area
        arena.areas[arena.arenaCount-1].end = malloc(arena.pageSize);
        if(arena.areas[arena.arenaCount-1].end == NULL) {
            printf("Could not create new area");
            exit(1);
        }
        ptr = (void*)arena.areas[arena.arenaCount-1].end;
        arena.areas[arena.arenaCount-1].bytesLeft = arena.pageSize;
    }

    // align end pointer and ensure area's pointers are updated
    int alignOffset = AlignForward((void*)((uintptr_t)arena.areas[i].end + size), align);
    arena.areas[i].end = (void*)((uintptr_t)arena.areas[i].end + size + alignOffset);
    arena.areas[i].bytesLeft -= size + alignOffset;

    return ptr;
}

void* ArenaReAlloc(void* old_ptr, size_t old_size, size_t new_size) {
    void* new_ptr = ArenaAlloc(new_size);
    memcpy(new_ptr, old_ptr, old_size);
    return new_ptr;
}

char* aprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char* buf = vaprintf(format, args);
    va_end(args);
    return buf;
}

char* vaprintf(const char* format, va_list args) {

    va_list lengthArgs;
    va_copy(lengthArgs, args);
    size_t len = vsnprintf(NULL, 0, format, lengthArgs) + 1;
    va_end(lengthArgs);

    char* buf = ArenaAlloc(len * sizeof(char));
    vsprintf(buf, format, args);

    return buf;
}

static size_t align(size_t value, size_t align) {
    return (value + (align - 1)) & ~(align - 1);
}

void memoryPoolAlloc(MemoryPool* pool, size_t pageSize) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    pageSize = align(pageSize, info.dwAllocationGranularity);

    pool->allocGranularity = info.dwAllocationGranularity;
    pool->pageSize = pageSize;
    pool->memory = VirtualAlloc(NULL, pageSize, MEM_RESERVE, PAGE_NOACCESS);
    pool->bytesUsed = 0;
}

// assumes itemSize < dwAllocationGranularity
void memoryArrayAlloc(MemoryArray* arr, MemoryPool* pool, size_t pageSize, size_t itemSize) {
    pageSize = align(pageSize, pool->allocGranularity);
    if(pool->bytesUsed + pageSize > pool->pageSize) {
        // todo: error handling, increasing virtual memory ammount
        printf("Out of virtual memory\n");
        exit(0);
    }

    arr->bytesUsed = 0;
    arr->bytesCommitted = 0;
    arr->itemSize = itemSize;
    arr->pageSize = pageSize;
    arr->memory = (char*)pool->memory + pool->bytesUsed;
    arr->allocGranularity = pool->allocGranularity;
    pool->bytesUsed += pageSize;
}

void* memoryArrayPush(MemoryArray* arr) {
    // todo: item alignment? it should already be 4k aligned
    if(arr->bytesUsed + arr->itemSize > arr->pageSize) {
        // todo: request another virtual memory page
        printf("Array out of memory\n");
        exit(0);
    }

    if(arr->bytesUsed + arr->itemSize > arr->bytesCommitted) {
        VirtualAlloc((char*)arr->memory + arr->bytesCommitted, arr->allocGranularity, MEM_COMMIT, PAGE_READWRITE);
        arr->bytesCommitted += arr->allocGranularity;
    }

    void* ptr = (char*)arr->memory + arr->bytesUsed;
    arr->bytesUsed += arr->itemSize;

    return ptr;
}
