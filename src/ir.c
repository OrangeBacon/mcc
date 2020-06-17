#define __USE_MINGW_ANSI_STDIO 1
#include "ir.h"
#include <math.h>
#include <assert.h>

// --------- //
// UTILITIES //
// --------- //

#define ITER_BLOCKS(fn, i, block, body) \
    { \
        IrBasicBlock* block = (fn)->firstBlock; \
        for(unsigned int i = 0; i < (fn)->blockCount; i++, block = block->next) body \
    }

#define ITER_INSTRUCTIONS(block, i, instruction, body) \
    { \
        IrInstruction* instruction = (block)->firstInstruction; \
        for(unsigned int i = 0; i < (block)->instructionCount; i++, instruction = instruction->next) body \
    }

#define ITER_PHIS(block, i, phi, body) \
    { \
        IrPhi* phi = (block)->firstPhi; \
        for(unsigned int i = 0; i < (block)->phiCount; i++, phi = phi->next) body \
    }

// ------- //
// BUILDER //
// ------- //

void IrContextCreate(IrContext* ctx, MemoryPool* pool) {
    memoryArrayAlloc(&ctx->topLevel, pool, 16*MiB, sizeof(IrTopLevel));
    memoryArrayAlloc(&ctx->basicBlocks, pool, 64*MiB, sizeof(IrBasicBlock));
    memoryArrayAlloc(&ctx->instructions, pool, 512*MiB, sizeof(IrInstruction));
    memoryArrayAlloc(&ctx->instParams, pool, 512*MiB, sizeof(IrParameter));
    memoryArrayAlloc(&ctx->vReg, pool, 256*MiB, sizeof(IrVirtualRegister));
    memoryArrayAlloc(&ctx->phi, pool, 128*MiB, sizeof(IrPhi));
}

IrFunction* IrFunctionCreate(IrContext* ctx, const char* name, unsigned int nameLength, IrParameter* returnType, IrParameter* inType, size_t parameterCount) {
    IrTopLevel* top = memoryArrayPush(&ctx->topLevel);
    top->name = name;
    top->nameLength = nameLength;
    top->kind = IR_TOP_LEVEL_FUNCTION;
    top->as.function.ID = ctx->topLevel.itemCount - 1;
    top->as.function.blockCount = 0;
    top->as.function.parameterCount = 0;
    top->as.function.returnType = returnType;
    top->as.function.ctx = ctx;
    top->as.function.lastVReg = NULL;
    top->as.function.lastBlock = NULL;
    top->as.function.parameterCount = parameterCount;
    top->as.function.parameters = inType;
    return &top->as.function;
}

IrGlobal* IrGlobalCreate(IrContext* ctx, char* name, unsigned int nameLength, size_t value) {
    IrTopLevel* top = memoryArrayPush(&ctx->topLevel);
    top->name = name;
    top->nameLength = nameLength;
    top->kind = IR_TOP_LEVEL_GLOBAL;
    top->as.global.ID = ctx->topLevel.itemCount - 1;
    top->as.global.value.value = value;
    top->as.global.value.type.kind = IR_TYPE_INTEGER;
    top->as.global.value.type.as.integer = 0;

    return &top->as.global;
}

IrBasicBlock* IrBasicBlockCreate(IrFunction* fn) {
    IrBasicBlock* block = memoryArrayPush(&fn->ctx->basicBlocks);
    block->instructionCount = 0;
    block->next = NULL;
    block->fn = fn;
    block->lastInstruction = NULL;
    block->instructionCount = 0;
    block->predecessors = NULL;
    block->predecessorCount = 0;
    block->firstPhi = NULL;
    block->phiCount = 0;
    block->lastPhi = NULL;

    if(fn->blockCount == 0) {
        fn->firstBlock = fn->lastBlock = block;
        block->ID = 0;
    } else {
        fn->lastBlock->next = block;
        block->ID = fn->lastBlock->ID + 1;
        fn->lastBlock = block;
    }

    fn->blockCount++;

    return block;
}

IrPhi* IrPhiCreate(IrContext* ctx, IrBasicBlock* block, size_t params) {
    IrPhi* phi = memoryArrayPush(&ctx->phi);
    IrParameterNewVReg(block->fn, &phi->result);
    phi->next = NULL;
    phi->parameterCount = params;
    phi->params = memoryArrayPushN(&ctx->instParams, params);
    phi->blocks = memoryArrayPushN(&ctx->instParams, params);

    if(block->phiCount == 0) {
        block->firstPhi = block->lastPhi = phi;
    } else {
        block->lastPhi->next = phi;
        block->lastPhi = phi;
    }
    block->phiCount++;

    return phi;
}

IrParameter* IrBlockSetPredecessors(IrBasicBlock* block, size_t count) {
    block->predecessorCount = count;
    return block->predecessors = memoryArrayPushN(&block->fn->ctx->instParams, count);
}

IrVirtualRegister* IrVirtualRegisterCreate(IrFunction* fn) {
    IrVirtualRegister* reg = memoryArrayPush(&fn->ctx->vReg);

    if(fn->lastVReg == NULL) {
        reg->ID = 0;
    } else {
        reg->ID = fn->lastVReg->ID + 1;
    }
    fn->lastVReg = reg;

    return reg;
}

IrParameter* IrParameterCreate(IrContext* ctx) {
    return memoryArrayPush(&ctx->instParams);
}
IrParameter* IrParametersCreate(IrContext* ctx, size_t count) {
    return memoryArrayPushN(&ctx->instParams, count);
}

void IrParameterConstant(IrParameter* param, int value) {
    param->kind = IR_PARAMETER_CONSTANT;
    param->as.constant.value = value;
    param->as.constant.undefined = false;
    param->as.constant.type.kind = IR_TYPE_INTEGER;
    param->as.constant.type.as.integer = 0;
}

void IrParameterUndefined(IrParameter* param) {
    IrParameterConstant(param, 0);
    param->as.constant.undefined = true;
}

void IrParameterIntegerType(IrParameter* param, int size) {
    param->kind = IR_PARAMETER_TYPE;
    param->as.type.kind = IR_TYPE_INTEGER;
    param->as.type.as.integer = size;
}

void IrParameterNewVReg(IrFunction* fn, IrParameter* param) {
    IrVirtualRegister* reg = IrVirtualRegisterCreate(fn);
    param->kind = IR_PARAMETER_REGISTER;
    param->as.virtualRegister = reg;
}

void IrParameterVRegRef(IrParameter* param, IrParameter* vreg) {
    param->kind = IR_PARAMETER_REGISTER;
    param->as.virtualRegister = vreg->as.virtualRegister;
}

void IrParameterBlock(IrParameter* param, IrBasicBlock* block) {
    param->kind = IR_PARAMETER_BLOCK;
    param->as.block = block;
}

void IrParameterGlobal(IrParameter* param, IrGlobal* global) {
    param->kind = IR_PARAMETER_GLOBAL;
    param->as.global = global;
}

void IrParameterReference(IrParameter* param, IrParameter* src) {
    switch(src->kind) {
        case IR_PARAMETER_BLOCK:
            IrParameterBlock(param, src->as.block); break;
        case IR_PARAMETER_CONSTANT:
            *param = *src; break;
        case IR_PARAMETER_GLOBAL:
            IrParameterGlobal(param, src->as.global); break;
        case IR_PARAMETER_REGISTER:
            IrParameterVRegRef(param, src); break;
        case IR_PARAMETER_TYPE:
            printf("IrBuilder error\n");exit(1);
    }
}


static IrInstruction* addIns(IrContext* ctx, IrBasicBlock* block, IrOpcode opcode) {
    IrInstruction* inst = memoryArrayPush(&ctx->instructions);

    if(block->lastInstruction == NULL) {
        inst->ID = 0;
        block->firstInstruction = inst;
    } else {
        block->lastInstruction->next = inst;
        inst->ID = block->lastInstruction->ID + 1;
    }
    block->instructionCount++;
    block->lastInstruction = inst;
    inst->hasCondition = false;
    inst->opcode = opcode;

    return inst;
}

IrInstruction* IrInstructionSetCreate(IrContext* ctx, IrBasicBlock* block, IrOpcode opcode, IrParameter* params, size_t paramCount) {
    IrInstruction* inst = addIns(ctx, block, opcode);

    inst->hasReturn = true;
    inst->params = params + 1;
    inst->parameterCount = paramCount - 1;
    params[0].as.virtualRegister->location = inst;
    params[0].as.virtualRegister->block = block;

    return inst;
}

IrInstruction* IrInstructionVoidCreate(IrContext* ctx, IrBasicBlock* block, IrOpcode opcode, IrParameter* params, size_t paramCount) {
    IrInstruction* inst = addIns(ctx, block, opcode);

    inst->hasReturn = false;
    inst->params = params;
    inst->parameterCount = paramCount;

    return inst;
}

void IrInstructionCondition(IrInstruction* inst, IrComparison cmp) {
    inst->hasCondition = true;
    inst->comparison = cmp;
}

// ------- //
// PRINTER //
// ------- //

void IrTypePrint(IrContext* ctx, IrType* ir) {
    switch(ir->kind) {
        case IR_TYPE_INTEGER:
            printf("i%d", ir->as.integer);
            break;
        case IR_TYPE_POINTER:
            for(unsigned int i = 0; i < ir->as.pointer.depth; i++) {
                putchar('*');
            }
            IrTypePrint(ctx, memoryArrayGet(&ctx->instParams, ir->as.pointer.type));
    }
}

void IrGlobalPrint(IrContext* ctx, IrTopLevel* ir) {
    IrGlobal* global = &ir->as.global;
    printf("global %.*s $%lld : ", ir->nameLength, ir->name, global->ID);
    IrTypePrint(ctx, &global->value.type);
    printf(" = %d\n\n", global->value.value);
}

void IrConstantPrint(IrConstant* ir) {
    if(ir->undefined) {
        printf("undefined");
    } else {
        printf("%d", ir->value);
    }
}

void IrParameterPrint(IrContext* ctx, IrParameter* param) {
    switch(param->kind) {
        case IR_PARAMETER_TYPE:
            IrTypePrint(ctx, &param->as.type);
            break;
        case IR_PARAMETER_REGISTER:
            printf("%%%lld", param->as.virtualRegister->ID);
            break;
        case IR_PARAMETER_CONSTANT:
            IrConstantPrint(&param->as.constant);
            break;
        case IR_PARAMETER_BLOCK:
            printf("@%lld", param->as.block->ID);
            break;
        case IR_PARAMETER_GLOBAL:
            printf("$%lld", param->as.global->ID);
            break;
    }
}

char* IrInstructionNames[IR_INS_MAX] = {
    [IR_INS_PARAMETER] = "parameter",
    [IR_INS_ADD] = "add",
    [IR_INS_COMPARE] = "compare",
    [IR_INS_JUMP_IF] = "jump if",
    [IR_INS_RETURN] = "return",
    [IR_INS_NEGATE] = "negate",
    [IR_INS_NOT] = "not",
    [IR_INS_SUB] = "sub",
    [IR_INS_SMUL] = "mul signed",
    [IR_INS_SDIV] = "div signed",
    [IR_INS_SREM] = "rem signed",
    [IR_INS_OR] = "or",
    [IR_INS_AND] = "and",
    [IR_INS_XOR] = "xor",
    [IR_INS_SHL] = "shift left",
    [IR_INS_ASR] = "shift right signed",
    [IR_INS_JUMP] = "jump",
    [IR_INS_ALLOCA] = "alloca",
    [IR_INS_LOAD] = "load",
    [IR_INS_STORE] = "store",
};

char* IrConditionNames[IR_COMPARE_MAX] = {
    [IR_COMPARE_GREATER] = "greater",
    [IR_COMPARE_EQUAL] = "equal",
    [IR_COMPARE_GREATER_EQUAL] = "greater equal",
    [IR_COMAPRE_LESS] = "less",
    [IR_COMPARE_NOT_EQUAL] = "not equal",
    [IR_COMPARE_LESS_EQUAL] = "less equal",
};

void IrInstructionPrint(IrContext* ctx, unsigned int idx, IrInstruction* inst, unsigned int gutterSize) {

    printf("%*d |   ", gutterSize, idx);
    if(inst->hasReturn) {
        IrParameter* param = inst->params - 1;
        IrParameterPrint(ctx, param);
        printf(" = ");
    }
    printf("%s ", IrInstructionNames[inst->opcode]);

    if(inst->hasCondition) {
        printf("%s ", IrConditionNames[inst->comparison]);
    }

    for(unsigned int i = 0; i < inst->parameterCount; i++) {
        IrParameter* param = inst->params + i;
        IrParameterPrint(ctx, param);
        printf(" ");
    }

    printf("\n");
}

void IrBasicBlockPrint(IrContext* ctx, size_t idx, IrBasicBlock* block, unsigned int gutterSize) {
    printf("%*s | @%lld", gutterSize, "", idx);

    if(block->predecessorCount == 0) {
        printf(":\n");
    } else {
        printf("(");
        for(unsigned int i = 0; i < block->predecessorCount; i++) {
            if(i > 0) printf(", ");
            printf("@%lld", block->predecessors[i].as.block->ID);
        }
        printf("):\n");
    }

    ITER_PHIS(block, i, phi, {
        printf("%*s |   %%%lld = phi ", gutterSize, "", phi->result.as.virtualRegister->ID);
        for(unsigned int j = 0; j < phi->parameterCount; j++) {
            printf("[@%lld ", phi->blocks[j].as.block->ID);
            IrParameterPrint(ctx, &phi->params[j]);
            printf("] ");
        }
        printf("\n");
    });

    ITER_INSTRUCTIONS(block, i, inst, {
        IrInstructionPrint(ctx, i, inst, gutterSize);
    });
}

unsigned int intLength(unsigned int num) {
    return num == 0 ? 1 : floor(log10(num)) + 1;
}

void IrFunctionPrint(IrContext* ctx, IrTopLevel* ir) {
    IrFunction* fn = &ir->as.function;
    printf("function %.*s $%lld(", ir->nameLength, ir->name, fn->ID);
    for(unsigned int i = 0; i < fn->parameterCount; i++) {
        if(i != 0) printf(", ");
        IrParameterPrint(ctx, &fn->parameters[i]);
    }
    printf(") : ");
    IrParameterPrint(ctx, fn->returnType);
    printf(" {\n");

    unsigned int instrCount = 0;
    ITER_BLOCKS(fn, i, block, {
        instrCount += block->instructionCount;
    });

    unsigned int gutterSize = intLength(instrCount);

    ITER_BLOCKS(fn, i, block, {
        IrBasicBlockPrint(ctx, i, block, gutterSize);
    });

    printf("}\n\n");
}

void IrTopLevelPrint(IrContext* ctx, size_t idx) {
    IrTopLevel* ir = memoryArrayGet(&ctx->topLevel, idx);
    switch(ir->kind) {
        case IR_TOP_LEVEL_GLOBAL:
            IrGlobalPrint(ctx, ir);
            break;
        case IR_TOP_LEVEL_FUNCTION:
            IrFunctionPrint(ctx, ir);
            break;
    }
}

void IrContextPrint(IrContext* ctx) {
    for(unsigned int i = 0; i < ctx->topLevel.itemCount; i++) {
        IrTopLevelPrint(ctx, i);
    }
}