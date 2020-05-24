#define __USE_MINGW_ANSI_STDIO 1
#include "ir.h"
#include <math.h>

#define KiB (1024)
#define MiB (1024*KiB)
#define GiB (1024*MiB)

void IrContextCreate(IrContext* ctx, MemoryPool* pool) {
    memoryArrayAlloc(&ctx->topLevel, pool, 16*MiB, sizeof(IrTopLevel));
    memoryArrayAlloc(&ctx->basicBlocks, pool, 64*MiB, sizeof(IrBasicBlock));
    memoryArrayAlloc(&ctx->instructions, pool, 512*MiB, sizeof(IrInstruction));
    memoryArrayAlloc(&ctx->instParams, pool, 512*MiB, sizeof(IrParameter));
}

void IrTypePrint(IrType* ir) {
    switch(ir->kind) {
        case IR_TYPE_INTEGER:
            printf("i%d", ir->as.integer);
            break;
        case IR_TYPE_POINTER:
            for(unsigned int i = 0; i < ir->as.pointer.depth; i++) {
                putchar('*');
            }
            IrTypePrint(ir->as.pointer.type);
    }
}

void IrGlobalPrint(IrTopLevel* ir, size_t idx) {
    IrGlobal* global = &ir->as.global;
    printf("global %.*s $%lld : ", ir->nameLength, ir->name, idx);
    IrTypePrint(&global->value.type);
    printf(" = %d\n", global->value.value);
}

void IrParameterPrint(IrParameter* param) {
    switch(param->kind) {
        case IR_PARAMETER_TYPE:
            IrTypePrint(&param->as.type);
            break;
        case IR_PARAMETER_REGISTER:
            printf("%%%d", param->as.virtualRegister.id);
            break;
        case IR_PARAMETER_CONSTANT:
            printf("%d", param->as.constant.value);
            break;
        case IR_PARAMETER_BLOCK:
            printf("@%lld", param->as.block);
            break;
    }
}

static char* IrInstructionNames[] = {
    "alloca",
    "add",
    "sub",
    "store",
    "load",
    "get element pointer",
    "phi",
};

static char* IrConditionNames[] = {
    "greater",
    "equal",
    "greater equal",
    "less",
    "not equal",
    "less equal",
};

void IrInstructionPrint(IrContext* ctx, size_t idx, unsigned int gutterSize) {
    IrInstruction* inst = memoryArrayGet(&ctx->instructions, idx);
    printf("%*lld | ", gutterSize, idx);
    if(inst->hasReturn) {
        IrParameter* param = memoryArrayGet(&ctx->instParams, inst->params - 1);
        IrParameterPrint(param);
        printf(" = ");
    }
    printf("%s ", IrInstructionNames[inst->opcode]);

    if(inst->hasCondition) {
        printf("%s ", IrConditionNames[inst->comparison]);
    }

    for(unsigned int i = 0; i < inst->parameterCount; i++) {
        IrParameter* param = memoryArrayGet(&ctx->instParams, inst->params + i);
        IrParameterPrint(param);
        printf(" ");
    }

    printf("\n");
}

void IrBasicBlockPrint(IrContext* ctx, size_t idx, unsigned int gutterSize) {
    IrBasicBlock* block = memoryArrayGet(&ctx->basicBlocks, idx);
    printf("%*s | @%lld:\n", gutterSize, "", idx);
    for(unsigned int i = 0; i < block->instructionCount; i++) {
        IrInstructionPrint(ctx, block->instrctions + i, gutterSize);
    }
}

unsigned int intLength(unsigned int num) {
    return num == 0 ? 1 : floor(log10(num)) + 1;
}

void IrFunctionPrint(IrContext* ctx, IrTopLevel* ir, size_t idx) {
    IrFunction* fn = &ir->as.function;
    printf("function %.*s $%lld(", ir->nameLength, ir->name, idx);
    for(unsigned int i = 0; i < fn->parameterCount; i++) {
        if(i != 0) printf(", ");
        IrParameter* param = memoryArrayGet(&ctx->instParams, fn->parameters + i);
        IrParameterPrint(param);
    }
    printf(") : ");
    IrTypePrint(&fn->returnType);
    printf(" {\n");

    unsigned int instrCount = 0;
    for(unsigned int i = 0; i < fn->blockCount; i++) {
        IrBasicBlock* block = memoryArrayGet(&ctx->basicBlocks, fn->blocks + i);
        instrCount += block->instructionCount;
    }

    unsigned int gutterSize = intLength(instrCount);

    for(unsigned int i = 0; i < fn->blockCount; i++) {
        IrBasicBlockPrint(ctx, fn->blocks + i, gutterSize);
    }
}

void IrTopLevelPrint(IrContext* ctx, size_t idx) {
    IrTopLevel* ir = memoryArrayGet(&ctx->topLevel, idx);
    switch(ir->kind) {
        case IR_TOP_LEVEL_GLOBAL:
            IrGlobalPrint(ir, idx);
            break;
        case IR_TOP_LEVEL_FUNCTION:
            IrFunctionPrint(ctx, ir, idx);
            break;
    }
}

void IrContextPrint(IrContext* ctx) {
    for(unsigned int i = 0; i < ctx->topLevel.itemCount; i++) {
        IrTopLevelPrint(ctx, i);
    }
}