#ifndef LEX_H
#define LEX_H

#include <stdint.h>
#include <stdbool.h>

typedef struct TranslationContext {
    char* source;
    size_t sourceLength;
    size_t consumed;

    size_t saveConsumed;

    char phase2Previous;
    struct Phase3Context* phase3;

    bool trigraphs;
} TranslationContext;

void TranslationContextInit(TranslationContext* ctx, const char* fileName);

void runPhase1(TranslationContext* ctx);
void runPhase2(TranslationContext* ctx);
void runPhase3(TranslationContext* ctx);

#endif