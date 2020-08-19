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

// increase value to the next multiple of align,
// assumes align is a power of two
// returns the new value
static size_t align(size_t value, size_t align) {
    return (value + (align - 1)) & ~(align - 1);
}

// array memory overhead, used as lookup table
#define MEMORY_ARRAY_INDEX_SIZE (512 * sizeof(void*))

// technically should be taken from windows system call, practically
// will never change
#define ALLOCATION_GRANULARITY 65536

// predict less likley to return truthy value
#define unlikely(x) __builtin_expect(!!(x), 0)

void memoryPoolAlloc(MemoryPool* pool, size_t pageSize) {
    pageSize = align(pageSize, ALLOCATION_GRANULARITY);

    pool->pageSize = pageSize;
    pool->memory = VirtualAlloc(NULL, pageSize, MEM_RESERVE, PAGE_NOACCESS);
    pool->bytesUsed = 0;
}

// how memory array works:
// It allocates a memory block (No. 1) of size pageSize at start.  The first
// MEMORY_ARRAY_INDEX_SIZE bytes of that block act as a lookup table.  The first
// item is allocated directly after that table, until the end of the memory
// block.  When a new block is required, (block n), its address is added to the
// lookup table in block 1, then it is used for allocating items.  The lookup
// table allows the get function to find the location an item is stored in,
// allowing the memory blocks to not have to be continuous.  This allows array
// expansion in constant time, without copying the rest of the array.  It does
// mean that the array can only be expanded (currently 512) times, however the
// pages could be large (1GiB+) and are only partially added to RAM, unless
// used.  Therefore unless 0.5TiB+ RAM per array is needed (unlikely, as will
// have multiple arrays), then it will be large enough.

// assumes itemSize < ALLOCATION_GRANULARITY (65536)
void memoryArrayAlloc(MemoryArray* arr, MemoryPool* pool, size_t pageSize, size_t itemSize) {
    pageSize = align(pageSize, ALLOCATION_GRANULARITY);

    if(pool->bytesUsed + pageSize > pool->pageSize) {
        // todo: error handling, increasing virtual memory amount?
        printf("Out of virtual memory - pool\n");
        exit(0);
    }

    arr->bytesUsed = 0;
    arr->pagesUsed = 0;
    arr->bytesCommitted = 0;
    arr->itemSize = itemSize;
    arr->pageSize = pageSize;
    arr->memory = (char*)pool->memory + pool->bytesUsed;
    arr->index = arr->memory;
    arr->pool = pool;
    arr->itemCount = 0;
    pool->bytesUsed += pageSize;
}

static void* memReserve(MemoryArray* arr, size_t byteCount) {
    // todo: item alignment? it should already be 4k aligned?

    if(unlikely(byteCount > ALLOCATION_GRANULARITY)) {
        printf("Large reservations not implemented\n");
        exit(0);
    }

    if(unlikely(arr->bytesUsed == 0)) {
        // new array, create index, memory is not allocated if array is not used

        VirtualAlloc(arr->index, ALLOCATION_GRANULARITY, MEM_COMMIT, PAGE_READWRITE);
        // allocate index
        arr->bytesUsed += MEMORY_ARRAY_INDEX_SIZE;
        arr->bytesCommitted += ALLOCATION_GRANULARITY;
        arr->pagesUsed = 1;
    }

    // bytes used + item size > avaliable memory
    // = make more memory avaliable
    if(arr->bytesUsed + byteCount > arr->bytesCommitted) {
        if(arr->bytesUsed + ALLOCATION_GRANULARITY > arr->pageSize) {
            if(arr->pool->bytesUsed + arr->pageSize > arr->pool->pageSize) {
                // todo: error handling, increasing virtual memory ammount
                printf("Out of virtual memory - array\n");
                exit(0);
            }

            // virtual memory location
            arr->memory = (char*)arr->pool->memory + arr->pool->bytesUsed;
            arr->pool->bytesUsed += arr->pageSize;
            arr->bytesUsed = 0;
            arr->bytesCommitted = 0;

            // set address in index
            ((void**)arr->index)[arr->pagesUsed - 1] = arr->memory;

            arr->pagesUsed++;
        }

        VirtualAlloc((char*)arr->memory + arr->bytesCommitted, ALLOCATION_GRANULARITY, MEM_COMMIT, PAGE_READWRITE);
        arr->bytesCommitted += ALLOCATION_GRANULARITY;
    }

    void* ptr = (char*)arr->memory + arr->bytesUsed;
    arr->bytesUsed += byteCount;
    arr->itemCount++;

    return ptr;
}

void* memoryArrayPush(MemoryArray* arr) {
    return memReserve(arr, arr->itemSize);
}

void* memoryArrayPushN(MemoryArray* arr, size_t n) {
    return memReserve(arr, arr->itemSize * n);
}

void* memoryArrayGet(MemoryArray* arr, size_t idx) {
#ifndef NDEBUG
    if(idx >= arr->itemCount) {
        printf("Array access out of bounds\n");
    }
#endif

    size_t firstPageCount = (arr->pageSize - MEMORY_ARRAY_INDEX_SIZE) / arr->itemSize;
    size_t itemsPerPage = arr->pageSize / arr->itemSize;

    if(arr->pagesUsed == 1) {
        return (char*)arr->index + MEMORY_ARRAY_INDEX_SIZE + idx * arr->itemSize;
    }

    idx -= firstPageCount;

    int pageNo = (idx - 1) / itemsPerPage + 1; // ceiling of integer division
    int itemNo = idx % itemsPerPage;

    // get page address from index
    uintptr_t pageAddr = (uintptr_t)(((void**)arr->index)[pageNo - 1]);

    // get item address
    pageAddr += itemNo * arr->itemSize;

    return (void*) pageAddr;
}
