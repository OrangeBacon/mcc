#ifndef LEX_H
#define LEX_H

#include <stdint.h>
#include <stdbool.h>
#include "memory.h"

typedef struct SourceLocation {
    const char* fileName;
    size_t line;
    size_t column;
    size_t length;
} SourceLocation;

typedef enum Phase3LexMode {
    LEX_MODE_MAYBE_HEADER,
    LEX_MODE_NO_HEADER
} Phase3LexMode;


typedef struct TranslationContext {
    // file buffer infomation
    char* source;
    size_t sourceLength;
    size_t consumed;

    SourceLocation phase1Location;
    char phase1IgnoreNewLine;


    char phase2Peek;
    SourceLocation phase2PeekLoc;
    char phase2Previous;
    SourceLocation phase2CurrentLoc;

    Phase3LexMode phase3mode;
    char phase3peek;
    SourceLocation phase3peekLoc;
    char phase3peekNext;
    SourceLocation phase3peekNextLoc;
    SourceLocation* phase3currentLocation;

    bool trigraphs;

    MemoryArray sourceArr;
    MemoryArray locations;
} TranslationContext;

void TranslationContextInit(TranslationContext* ctx, MemoryPool* pool, const char* fileName);

void runPhase1(TranslationContext* ctx);
void runPhase2(TranslationContext* ctx);
void runPhase3(TranslationContext* ctx);

#endif