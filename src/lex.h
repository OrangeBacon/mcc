#ifndef LEX_H
#define LEX_H

#include <stdint.h>
#include <stdbool.h>
#include "memory.h"

typedef struct SourceLocation {
    const unsigned char* fileName;
    size_t line;
    size_t column;
    size_t length;
} SourceLocation;

typedef enum Phase3LexMode {
    LEX_MODE_MAYBE_HEADER,
    LEX_MODE_NO_HEADER
} Phase3LexMode;

struct TranslationContext;
typedef struct Phase3Mode {
    Phase3LexMode mode;
    unsigned char peek;
    SourceLocation peekLoc;
    unsigned char peekNext;
    SourceLocation peekNextLoc;
    SourceLocation* currentLocation;
    bool AtStart;
    unsigned char (*getter)(struct TranslationContext*, SourceLocation* loc);
} Phase3Mode;

// random data used by each translation phase that needs to be stored
typedef struct TranslationContext {
    // settings
    bool trigraphs;
    size_t tabSize;
    bool debugPrint;

    // state
    bool tokenPrinterAtStart;

    unsigned char* phase1source;
    size_t phase1sourceLength;
    size_t phase1consumed;
    SourceLocation phase1Location;
    unsigned char phase1IgnoreNewLine;

    unsigned char phase2Peek;
    SourceLocation phase2PeekLoc;
    unsigned char phase2Previous;
    SourceLocation phase2CurrentLoc;

    Phase3Mode phase3;

    // memory allocators
    MemoryArray stringArr;
    MemoryArray locations;
} TranslationContext;

void TranslationContextInit(TranslationContext* ctx, MemoryPool* pool, const unsigned char* fileName);

void runPhase1(TranslationContext* ctx);
void runPhase2(TranslationContext* ctx);
void runPhase3(TranslationContext* ctx);

#endif