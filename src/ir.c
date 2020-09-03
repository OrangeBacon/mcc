#define __USE_MINGW_ANSI_STDIO 1
#include "ir.h"
#include <math.h>
#include <assert.h>

bool optimisePhis = true;
bool removeAfterJump = true;
bool removeUnusedBlocks = true;

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
    memoryArrayAlloc(&ctx->usageData, pool, 128*MiB, sizeof(IrUsageData));
    memoryArrayAlloc(&ctx->phi, pool, 128*MiB, sizeof(IrPhi));
}

IrTopLevel* IrFunctionCreate(IrContext* ctx, const char* name, unsigned int nameLength, IrParameter* returnType, IrParameter* inType, size_t parameterCount) {
    IrTopLevel* top = memoryArrayPush(&ctx->topLevel);
    top->name = name;
    top->nameLength = nameLength;
    top->kind = IR_TOP_LEVEL_FUNCTION;
    top->ID = ctx->topLevel.itemCount - 1;
    top->as.function.blockCount = 0;
    top->as.function.parameterCount = 0;
    top->as.function.returnType = returnType;
    top->as.function.ctx = ctx;
    top->as.function.lastVReg = NULL;
    top->as.function.lastBlock = NULL;
    top->as.function.parameterCount = parameterCount;
    top->as.function.parameters = inType;
    PAIRTABLE_INIT(top->as.function.variableTable, IrParameter*);
    return top;
}

IrTopLevel* IrGlobalPrototypeCreate(IrContext* ctx, const char* name, unsigned int nameLength) {
    IrTopLevel* top = memoryArrayPush(&ctx->topLevel);
    top->name = name;
    top->nameLength = nameLength;
    top->kind = IR_TOP_LEVEL_GLOBAL;
    top->ID = ctx->topLevel.itemCount - 1;
    top->as.global.undefined = true;

    return top;
}

void IrGlobalInitialize(IrTopLevel* top, size_t value, size_t size) {
    top->as.global.value = value;
    top->as.global.undefined = false;
    top->as.global.type.kind = IR_TYPE_INTEGER;
    top->as.global.type.pointerDepth = 0;
    top->as.global.type.as.integer = size;
}

IrBasicBlock* IrBasicBlockCreate(IrFunction* fn) {
    IrBasicBlock* block = memoryArrayPush(&fn->ctx->basicBlocks);
    block->instructionCount = 0;
    block->next = NULL;
    block->fn = fn;
    block->lastInstruction = NULL;
    block->instructionCount = 0;
    block->firstPhi = NULL;
    block->phiCount = 0;
    block->lastPhi = NULL;
    block->sealed = false;
    block->predCount = 0;
    block->predecessors = NULL;
    block->lastPred = NULL;

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

IrPhi* IrPhiCreate(IrContext* ctx, IrBasicBlock* block, SymbolLocal* var) {
    IrPhi* phi = memoryArrayPush(&ctx->phi);
    IrParameterNewVReg(block->fn, &phi->result);
    phi->next = NULL;
    ARRAY_ALLOC(IrPhiParameter, *phi, param);
    phi->incomplete = false;
    phi->used = true;
    phi->tryRemoveProcessing = false;

    phi->result.as.virtualRegister->block = block;
    phi->var = var;
    phi->block = block;
    phi->result.as.virtualRegister->isPhi = true;
    phi->result.as.virtualRegister->loc.phi = phi;

    if(block->phiCount == 0) {
        block->firstPhi = block->lastPhi = phi;
    } else {
        block->lastPhi->next = phi;
        block->lastPhi = phi;
    }
    block->phiCount++;

    return phi;
}

static void IrPhiSetReturnType(IrPhi* phi);
static void IrInstructionSetReturnType(IrFunction* fn, IrInstruction* instruction);

static void IrVirtualRegisterAddUsage(IrContext* ctx, IrParameter* param, void* source, IrUsageType type) {
    IrVirtualRegister* reg = param->as.virtualRegister;
    IrUsageData* usage = memoryArrayPush(&ctx->usageData);
    usage->usageLocation = param;
    usage->source = source;
    usage->sourceType = type;
    usage->prev = reg->users;

    reg->users = usage;
    reg->useCount++;
}

static void IrBasicBlockAddUsage(IrContext* ctx, IrBasicBlock* block, void* loc, void* source, IrUsageType type) {
    IrUsageData* usage = memoryArrayPush(&ctx->usageData);

    usage->usageLocation = loc;
    usage->source = source;
    usage->sourceType = type;
    usage->prev = block->users;

    block->users = usage;
    block->useCount++;
}

static void IrBasicBlockAddPredecessor(IrBasicBlock* block, IrBasicBlock* pred) {
    IrUsageData* data = memoryArrayPush(&block->fn->ctx->usageData);
    data->source = pred;
    data->prev = NULL;

    if(block->predecessors == NULL) {
        block->predecessors = data;
        block->lastPred = data;
    } else {
        block->lastPred->prev = data;
        block->lastPred = data;
    }

    block->predCount++;

    IrBasicBlockAddUsage(block->fn->ctx, pred, data, block, IR_USAGE_PREDECESSOR);
}

void IrPhiAddOperand(IrContext* ctx, IrPhi* phi, IrBasicBlock* block, IrParameter* operand) {
    ARRAY_PUSH(*phi, param, ((IrPhiParameter) {0}));
    IrPhiParameter* param = &phi->params[phi->paramCount-1];

    param->block = block;
    param->ignore = false;
    IrParameterReference(&param->param, operand);

    if(phi->paramCount == 1) {
        IrPhiSetReturnType(phi);
    }

    if(operand->kind == IR_PARAMETER_VREG) {
        IrVirtualRegisterAddUsage(ctx, &param->param, phi, IR_USAGE_PHI);
    }

    IrBasicBlockAddUsage(ctx, block, param, phi, IR_USAGE_PHI);
}

bool IrTypeEqual(IrType* a, IrType* b) {
    if(a->kind != b->kind) return false;
    if(a->pointerDepth != b->pointerDepth) return false;

    switch(a->kind) {
        case IR_TYPE_NONE: return true;
        case IR_TYPE_INTEGER: return a->as.integer == b->as.integer;
        case IR_TYPE_FUNCTION:
            if(a->as.function.parameterCount != b->as.function.parameterCount)
                return false;
            if(!IrTypeEqual(&a->as.function.retType->as.type, &b->as.function.retType->as.type))
                return false;
            for(unsigned int i = 0; i < a->as.function.parameterCount; i++) {
                if(!IrTypeEqual(&a->as.function.parameters[i].as.type,
                                &b->as.function.parameters[i].as.type)) {
                    return false;
                }
            }
            return true;
    }

    printf("unreachable type equality\n"); exit(1);
}

static IrVirtualRegister* IrVirtualRegisterCreate(IrFunction* fn) {
    IrVirtualRegister* reg = memoryArrayPush(&fn->ctx->vReg);

    if(fn->lastVReg == NULL) {
        reg->ID = 0;
    } else {
        reg->ID = fn->lastVReg->ID + 1;
    }
    fn->lastVReg = reg;

    reg->type.kind = IR_TYPE_NONE;
    reg->users = NULL;
    reg->useCount = 0;

    return reg;
}

IrParameter* IrParameterCreate(IrContext* ctx) {
    return memoryArrayPush(&ctx->instParams);
}
IrParameter* IrParametersCreate(IrContext* ctx, size_t count) {
    return memoryArrayPushN(&ctx->instParams, count);
}

void IrParameterConstant(IrParameter* param, int value, int dataSize) {
    param->kind = IR_PARAMETER_CONSTANT;
    param->as.constant.value = value;
    param->as.constant.undefined = false;
    param->as.constant.type.kind = IR_TYPE_INTEGER;
    param->as.constant.type.as.integer = dataSize;
}

void IrParameterUndefined(IrParameter* param) {
    IrParameterConstant(param, 0, 0);
    param->as.constant.undefined = true;
}

void IrParameterIntegerType(IrParameter* param, int size) {
    param->kind = IR_PARAMETER_TYPE;
    param->as.type.kind = IR_TYPE_INTEGER;
    param->as.type.as.integer = size;
}

void IrParameterNewVReg(IrFunction* fn, IrParameter* param) {
    IrVirtualRegister* reg = IrVirtualRegisterCreate(fn);
    param->kind = IR_PARAMETER_VREG;
    param->as.virtualRegister = reg;
}

static void IrParameterVRegRef(IrParameter* param, IrParameter* vreg) {
    param->kind = IR_PARAMETER_VREG;
    param->as.virtualRegister = vreg->as.virtualRegister;
}

void IrParameterBlock(IrParameter* param, IrBasicBlock* block) {
    param->kind = IR_PARAMETER_BLOCK;
    param->as.block = block;
}

void IrParameterTopLevel(IrParameter* param, IrTopLevel* top) {
    param->kind = IR_PARAMETER_TOP_LEVEL;
    param->as.topLevel = top;
}

void IrParameterReference(IrParameter* param, IrParameter* src) {
    switch(src->kind) {
        case IR_PARAMETER_BLOCK:
            IrParameterBlock(param, src->as.block); break;
        case IR_PARAMETER_CONSTANT:
            *param = *src; break;
        case IR_PARAMETER_TOP_LEVEL:
            IrParameterTopLevel(param, src->as.topLevel); break;
        case IR_PARAMETER_VREG:
            IrParameterVRegRef(param, src); break;
        case IR_PARAMETER_TYPE:
            printf("IrBuilder error\n");exit(1);
    }
}

IrType* IrParameterGetType(IrParameter* param) {
    switch(param->kind) {
        case IR_PARAMETER_BLOCK:
            printf("blocks do not have a type"); exit(0);
        case IR_PARAMETER_CONSTANT:
            return &param->as.constant.type;
        case IR_PARAMETER_TOP_LEVEL:
            return &param->as.topLevel->type;
        case IR_PARAMETER_VREG:
            return &param->as.virtualRegister->type;
        case IR_PARAMETER_TYPE:
            return &param->as.type;
    }

    return NULL; // unreachable
}

static void IrPhiSetReturnType(IrPhi* phi) {
    if(phi->returnTypeSet) return;

    IrParameter* param1 = &phi->params[0].param;
    if(param1->kind == IR_PARAMETER_VREG && !param1->as.virtualRegister->hasType) {
        return;
    }

    IrVirtualRegister* vreg = phi->result.as.virtualRegister;
    vreg->type = *IrParameterGetType(&phi->params[0].param);
    vreg->hasType = true;
    phi->returnTypeSet = true;

    IrUsageData* usage = vreg->users;
    for(unsigned int i = 0; i < vreg->useCount; i++) {
        if(usage->sourceType == IR_USAGE_PHI) {
            IrPhiSetReturnType(usage->source);
        } else if(usage->sourceType == IR_USAGE_INSTRUCTION) {
            IrInstruction* inst = usage->source;
            IrInstructionSetReturnType(phi->block->fn, inst);
        }
        usage = usage->prev;
    }
}

static void IrInstructionSetReturnType(IrFunction* fn, IrInstruction* instruction) {
    SSAInstruction* inst = &instruction->as.ssa;
    if(inst->returnTypeSet || !inst->hasReturn) return;

    for(unsigned int i = 0; i < inst->parameterCount; i++) {
        IrParameter* param = &inst->params[i];
        if(param->kind == IR_PARAMETER_VREG && !param->as.virtualRegister->hasType) {
            return;
        }
    }

    IrVirtualRegister* vreg = inst->params[-1].as.virtualRegister;
    IrType* ret = &vreg->type;
    switch(inst->opcode) {
        case IR_INS_PARAMETER:
            *ret = fn->parameters[inst->params[0].as.constant.value].as.type;
            break;
        case IR_INS_ADD:
        case IR_INS_SUB:
        case IR_INS_SMUL:
        case IR_INS_SDIV:
        case IR_INS_SREM:
        case IR_INS_SHL:
        case IR_INS_ASR:
        case IR_INS_OR:
        case IR_INS_AND:
        case IR_INS_XOR:
        case IR_INS_NEGATE:
            *ret = *IrParameterGetType(&inst->params[0]);
            break;
        case IR_INS_COMPARE:
        case IR_INS_NOT:
            ret->kind = IR_TYPE_INTEGER;
            ret->pointerDepth = 0;
            ret->as.integer = 8;
            break;
        case IR_INS_ALLOCA:
            *ret = inst->params[0].as.type;
            ret->pointerDepth++;
            break;
        case IR_INS_LOAD:
            *ret = *IrParameterGetType(&inst->params[0]);
            ret->pointerDepth--;
            break;
        case IR_INS_GET_ELEMENT_POINTER: {
            // note: this only works as gep is only used for pointer arithmetic
            // &a + 1, etc
            *ret = *IrParameterGetType(&inst->params[0]);
        }; break;
        case IR_INS_CAST:
            *ret = inst->params[0].as.type;
            break;
        case IR_INS_CALL:
            *ret = IrParameterGetType(&inst->params[0])->as.function.retType->as.type;
            break;
        case IR_INS_SIZEOF:
            ret->kind = IR_TYPE_INTEGER;
            ret->pointerDepth = 0;
            ret->as.integer = 32;
            break;

        // no return value
        case IR_INS_RETURN:
        case IR_INS_STORE:
        case IR_INS_JUMP:
        case IR_INS_JUMP_IF:
            break;

        // should be unreachable
        case IR_INS_MAX:
            break;
    }

    inst->params[-1].as.virtualRegister->hasType = true;
    inst->returnTypeSet = true;

    IrUsageData* usage = vreg->users;
    for(unsigned int i = 0; i < vreg->useCount; i++) {
        if(usage->sourceType == IR_USAGE_PHI) {
            IrPhiSetReturnType(usage->source);
        } else if(usage->sourceType == IR_USAGE_INSTRUCTION) {
            IrInstruction* inst = usage->source;
            IrInstructionSetReturnType(fn, inst);
        }
        usage = usage->prev;
    }
}

IrInstruction* IrInstructionAppend(IrContext* ctx, IrBasicBlock* block) {
    IrInstruction* inst = memoryArrayPush(&ctx->instructions);

    if(block->lastInstruction == NULL) {
        inst->prev = NULL;
        block->firstInstruction = inst;
    } else {
        inst->prev = block->lastInstruction;
        block->lastInstruction->next = inst;
    }
    inst->ID = block->maxInstructionCount;
    block->instructionCount++;
    block->maxInstructionCount++;
    block->lastInstruction = inst;

    inst->block = block;

    return inst;
}

IrInstruction* IrInstructionInsertAfter(IrContext* ctx, IrInstruction* prev) {
    IrBasicBlock* block = prev->block;
    IrInstruction* next = prev->next;

    IrInstruction* new = memoryArrayPush(&ctx->instructions);
    new->block = block;
    new->prev = prev;
    new->next = next;
    new->ID = block->maxInstructionCount;

    prev->next = new;
    if(next) next->prev = new;
    if(block->lastInstruction == prev) block->lastInstruction = new;

    block->instructionCount++;
    block->maxInstructionCount++;

    return new;
}

IrInstruction* IrInstructionInsertBefore(IrContext* ctx, IrInstruction* next) {
    IrBasicBlock* block = next->block;
    IrInstruction* prev = next->prev;

    IrInstruction* new = memoryArrayPush(&ctx->instructions);
    new->block = block;
    new->prev = prev;
    new->next = next;
    new->ID = block->maxInstructionCount;

    next->prev = new;
    if(prev) prev->next = new;
    if(block->firstInstruction == next) block->firstInstruction = new;

    block->instructionCount++;
    block->maxInstructionCount++;

    return new;
}

void IrInstructionRemove(IrInstruction* inst) {
    IrInstruction* prev = inst->prev;
    IrInstruction* next = inst->next;
    IrBasicBlock* block = inst->block;

    if(prev) prev->next = next;
    if(next) next->prev = prev;

    if(block->firstInstruction == inst) block->firstInstruction = next;
    if(block->lastInstruction == inst)  block->lastInstruction  = prev;

    block->instructionCount--;
}

static void instructionVregUsageSet(IrContext* ctx, IrInstruction* inst) {
    unsigned int paramCount = inst->as.ssa.parameterCount;
    IrParameter* params = inst->as.ssa.params;
    for(unsigned int i = 0; i < paramCount; i++) {
        IrParameter* param = &params[i];
        if(param->kind == IR_PARAMETER_VREG) {
            IrVirtualRegisterAddUsage(ctx, param, inst, IR_USAGE_INSTRUCTION);
        } else if(param->kind == IR_PARAMETER_BLOCK) {
            IrBasicBlockAddUsage(ctx, param->as.block, param, inst, IR_USAGE_INSTRUCTION);
        }
    }
}

static bool instrIsSSAJump(IrInstruction* inst) {
    if(inst == NULL) return false;
    if(inst->kind != IR_INSTRUCTION_SSA) return false;

    IrOpcode op = inst->as.ssa.opcode;
    return op == IR_INS_JUMP || op == IR_INS_JUMP_IF || op == IR_INS_RETURN;
}

IrInstruction* IrInstructionSetCreate(IrContext* ctx, IrBasicBlock* block, IrOpcode opcode, IrParameter* params, size_t paramCount) {
    if(removeAfterJump && instrIsSSAJump(block->lastInstruction)) return NULL;

    IrInstruction* inst = IrInstructionAppend(ctx, block);

    inst->kind = IR_INSTRUCTION_SSA;
    inst->as.ssa.hasCondition = false;
    inst->as.ssa.returnTypeSet = false;
    inst->as.ssa.opcode = opcode;
    inst->as.ssa.hasReturn = true;
    inst->as.ssa.params = params + 1;
    inst->as.ssa.parameterCount = paramCount - 1;
    params[0].as.virtualRegister->isPhi = false;
    params[0].as.virtualRegister->loc.inst = inst;
    params[0].as.virtualRegister->block = block;

    IrInstructionSetReturnType(block->fn, inst);
    instructionVregUsageSet(ctx, inst);

    return inst;
}

IrInstruction* IrInstructionVoidCreate(IrContext* ctx, IrBasicBlock* block, IrOpcode opcode, IrParameter* params, size_t paramCount) {
    if(removeAfterJump && instrIsSSAJump(block->lastInstruction)) return NULL;

    IrInstruction* inst = IrInstructionAppend(ctx, block);

    inst->kind = IR_INSTRUCTION_SSA;
    inst->as.ssa.hasCondition = false;
    inst->as.ssa.returnTypeSet = false;
    inst->as.ssa.opcode = opcode;
    inst->as.ssa.hasReturn = false;
    inst->as.ssa.params = params;
    inst->as.ssa.parameterCount = paramCount;

    instructionVregUsageSet(ctx, inst);

    //if(instrIsSSAJump(block->lastInstruction)) return inst;

    if(opcode == IR_INS_JUMP) {
        IrBasicBlockAddPredecessor(params[0].as.block, block);
    } else if(opcode == IR_INS_JUMP_IF) {
        IrBasicBlockAddPredecessor(params[1].as.block, block);
        IrBasicBlockAddPredecessor(params[2].as.block, block);
    }

    return inst;
}

void IrInstructionCondition(IrInstruction* inst, IrComparison cmp) {
    inst->as.ssa.hasCondition = true;
    inst->as.ssa.comparison = cmp;
}

void IrInvertCondition(IrInstruction* inst) {
    if(!inst->as.ssa.hasCondition) return;

    IrComparison new;
    switch(inst->as.ssa.comparison) {
        case IR_COMAPRE_LESS: new = IR_COMPARE_GREATER_EQUAL; break;
        case IR_COMPARE_EQUAL: new = IR_COMPARE_NOT_EQUAL; break;
        case IR_COMPARE_GREATER: new = IR_COMPARE_LESS_EQUAL; break;
        case IR_COMPARE_GREATER_EQUAL: new = IR_COMAPRE_LESS; break;
        case IR_COMPARE_LESS_EQUAL: new = IR_COMPARE_GREATER; break;
        case IR_COMPARE_NOT_EQUAL: new = IR_COMPARE_EQUAL; break;
        case IR_COMPARE_MAX: new = IR_COMPARE_MAX; break; // invalid hopefully
    }

    inst->as.ssa.comparison = new;
}

// --------------- //
// VARIABLE LOOKUP //
// --------------- //

// See Simple and Efficient Construction of Static SingleAssignment Form
// (https://c9x.me/compile/bib/braun13cc.pdf) for more infomation

void IrWriteVariable(IrFunction* fn, SymbolLocal* var, IrBasicBlock* block, IrParameter* value) {
    PAIRTABLE_SET(fn->variableTable, var, block, value);
}

static IrParameter* IrReadVariableRecursive(IrFunction* fn, SymbolLocal* var, IrBasicBlock* block);
IrParameter* IrReadVariable(IrFunction* fn, SymbolLocal* var, IrBasicBlock* block) {
    if(pairPableHas(&fn->variableTable, var, block)) {
        // local value numbering
        return pairTableGet(&fn->variableTable, var, block);
    }

    // global value numbering
    return IrReadVariableRecursive(fn, var, block);
}

static IrParameter* IrAddPhiOperands(IrFunction* fn, SymbolLocal* var, IrPhi* phi);
static IrParameter* IrReadVariableRecursive(IrFunction* fn, SymbolLocal* var, IrBasicBlock* block) {
    IrParameter* val;

    if(!block->sealed) {
        // incomplete CFG
        IrPhi* phi = IrPhiCreate(fn->ctx, block, var);
        val = &phi->result;
        phi->incomplete = true;
    } else if(block->predCount == 1) {
        // Optimise the common case of one predecessor: No phi needed
        val = IrReadVariable(fn, var, block->predecessors->source);
    } else {
        // Break potential cycles with operandless phis
        IrPhi* phi = IrPhiCreate(fn->ctx, block, var);
        val = &phi->result;
        IrWriteVariable(fn, var, block, val);
        val = IrAddPhiOperands(fn, var, phi);
    }

    IrWriteVariable(fn, var, block, val);

    return val;
}

static IrParameter* IrTryRemoveTrivialPhi(IrPhi* phi);
static IrParameter* IrAddPhiOperands(IrFunction* fn, SymbolLocal* var, IrPhi* phi) {
    IrUsageData* predData = phi->block->predecessors;
    for(unsigned int i = 0; i < phi->block->predCount; i++) {
        IrBasicBlock* pred = predData->source;
        IrParameter* read = IrReadVariable(fn, var, pred);
        IrPhiAddOperand(fn->ctx, phi, pred, read);
        predData = predData->prev;
    }

    return IrTryRemoveTrivialPhi(phi);
}

static bool IrParameterEqual(IrParameter* a, IrParameter* b) {
    if(a == NULL || b == NULL) return false;
    if(a->kind != b->kind) return false;

    switch(a->kind) {
        case IR_PARAMETER_BLOCK: return a->as.block == b->as.block;
        case IR_PARAMETER_VREG: return a->as.virtualRegister == b->as.virtualRegister;
        case IR_PARAMETER_TOP_LEVEL: return a->as.topLevel == b->as.topLevel;
        case IR_PARAMETER_TYPE: return IrTypeEqual(&a->as.type, &b->as.type);
        case IR_PARAMETER_CONSTANT: return
            a->as.constant.undefined == b->as.constant.undefined &&
            a->as.constant.value == b->as.constant.undefined &&
            IrTypeEqual(&a->as.constant.type, &b->as.constant.type);
    }

    printf("Unreachable\n"); exit(0);
}

// replace the usage of one vreg with another parameteer of any type
static void IrParameterReplaceVreg(IrFunction* fn, IrParameter* old, IrParameter* new) {
    // replace register inside the variable table
    for(unsigned int i = 0; i < fn->variableTable.entryCount; i++) {
        PairEntry* entry = &fn->variableTable.entrys[i];
        if(entry->key.key1 == NULL) {
            continue;
        }
        IrParameter* param = entry->value;
        if(IrParameterEqual(param, old)) {
            entry->value = new;
        }
    }

    // remember all users exept the phi itsself
    IrVirtualRegister* reg = old->as.virtualRegister;

    // reverse iterate double linked list
    IrUsageData* usage = reg->users;
    for(unsigned int i = 0; i < reg->useCount; i++) {
        // reroute all uses of phi to new
        IrParameter* param = usage->usageLocation;
        *param = new != NULL? *new : (IrParameter){0};

        // update usages of replacement register
        if(new->kind == IR_PARAMETER_VREG) {
            IrVirtualRegisterAddUsage(fn->ctx, param, usage->source, usage->sourceType);
        }

        // phi elimination
        if(usage->sourceType == IR_USAGE_PHI && ((IrPhi*)usage->source)->used && (!reg->isPhi || usage->source != reg->loc.phi)) {
            IrTryRemoveTrivialPhi(usage->source);
        }
        usage = usage->prev;
    }
}

static IrParameter* IrTryRemoveTrivialPhi(IrPhi* phi) {
    if(!optimisePhis || !phi->used || phi->tryRemoveProcessing) {
        return &phi->result;
    }

    phi->tryRemoveProcessing = true;

    IrParameter* same = NULL;

    for(unsigned int i = 0; i < phi->paramCount; i++) {
        IrPhiParameter* param = &phi->params[i];
        if(param->ignore) continue;

        if (
            IrParameterEqual(&param->param, same) ||
            (param->param.kind == IR_PARAMETER_VREG &&
                param->param.as.virtualRegister->isPhi &&
                param->param.as.virtualRegister->loc.phi == phi)
        ) {
            continue; // unique value or self-reference
        }
        if(same != NULL) {
            phi->tryRemoveProcessing = false;
            return &phi->result; // the phi merges at least two values: not trivial
        }
        same = &param->param;
    }

    IrParameterReplaceVreg(phi->block->fn, &phi->result, same);

    // remove phi
    phi->used = false;

    return same;
}

void IrSealBlock(IrFunction* fn, IrBasicBlock* block) {
    if(block->sealed) return;
    if(block->predCount == 0) return;
    ITER_PHIS(block, i, phi, {
        if(phi->incomplete && phi->used) IrAddPhiOperands(fn, phi->var, phi);
    });
    block->sealed = true;
}

void IrTryRemoveTrivialBlocks(IrFunction* fn) {
    if(!removeUnusedBlocks) return;

    IrBasicBlock* prev = NULL;
    IrBasicBlock* block = fn->firstBlock;
    for(unsigned int i = 0; i < fn->blockCount; i++) {
        // do not eliminate entry block
        if(block->ID == 0) goto nextBlock;

        // check for usages of the block
        IrUsageData* data = block->users;
        for(unsigned int j = 0; j < block->useCount; j++) {
            if(data->sourceType != IR_USAGE_PHI && data->sourceType != IR_USAGE_PREDECESSOR) {
                goto nextBlock;
            }
            data = data->prev;
        }

        IrBasicBlock* next = block->next;
        if(prev) prev->next = next;
        if(fn->firstBlock == block) fn->firstBlock = next;
        if(fn->lastBlock == block) fn->lastBlock = prev;
        fn->blockCount--;

        IrUsageData* last = NULL;
        data = block->users;
        for(unsigned int j = 0; j < block->useCount; j++) {
            if(data->sourceType == IR_USAGE_PHI) {
                ((IrPhiParameter*)data->usageLocation)->ignore = true;
                IrTryRemoveTrivialPhi(data->source);
            } else if(data->sourceType == IR_USAGE_PREDECESSOR) {
                IrBasicBlock* blk = data->source;
                if(last) last->prev = data->prev;
                if(blk->predecessors == data) blk->predecessors = data->prev;
                if(blk->lastPred == data) blk->lastPred = last;
                blk->predCount--;
            }
            last = data;
            data = data->prev;
        }

        nextBlock:
        prev = block;
        block = block->next;
    }
}

// ------- //
// PRINTER //
// ------- //

static void IrTypePrint(IrType* ir) {
    switch(ir->kind) {
        case IR_TYPE_NONE:
            printf("none");
            break;
        case IR_TYPE_INTEGER:
            printf("i%d", ir->as.integer);
            break;
        case IR_TYPE_FUNCTION:
            printf("(");
            for(unsigned int i = 0; i < ir->as.function.parameterCount; i++) {
                if(i != 0) printf(", ");
                IrTypePrint(&ir->as.function.parameters[i].as.type);
            }
            if(ir->as.function.parameterCount > 0) printf(" ");
            printf("-> ");
            IrTypePrint(&ir->as.function.retType->as.type);
            printf(")");
    }
    if(ir->pointerDepth > 10) {
        printf("*?");
    } else {
        for(unsigned int i = 0; i < ir->pointerDepth; i++) {
            putchar('*');
        }
    }
}

static void IrGlobalPrint(IrTopLevel* ir) {
    IrConstant* global = &ir->as.global;
    printf("global %.*s : ", ir->nameLength, ir->name);
    IrType realType = ir->type;
    realType.pointerDepth--;
    IrTypePrint(&realType);

    printf(" -> $%lld : ", ir->ID);
    IrTypePrint(&ir->type);

    if(!global->undefined) {
        printf(" = %d\n\n", global->value);
    } else {
        printf("\n\n");
    }
}

static void IrConstantPrint(IrConstant* ir) {
    if(ir->undefined) {
        printf("undefined");
    } else {
        printf("%d", ir->value);
    }
}

static void IrParameterPrint(IrParameter* param, bool printType) {
    switch(param->kind) {
        case IR_PARAMETER_TYPE:
            IrTypePrint(&param->as.type);
            break;
        case IR_PARAMETER_VREG:
            printf("%%%lld", param->as.virtualRegister->ID);
            break;
        case IR_PARAMETER_CONSTANT:
            IrConstantPrint(&param->as.constant);
            break;
        case IR_PARAMETER_BLOCK:
            printf("@%lld", param->as.block->ID);
            break;
        case IR_PARAMETER_TOP_LEVEL:
            printf("$%lld", param->as.topLevel->ID);
            break;
    }

    if(!printType || param->kind == IR_PARAMETER_TYPE) return;

    printf(" : ");

    switch(param->kind) {
        case IR_PARAMETER_VREG:
            IrTypePrint(&param->as.virtualRegister->type);
            break;
        case IR_PARAMETER_CONSTANT:
            IrTypePrint(&param->as.constant.type);
            break;
        case IR_PARAMETER_BLOCK:
            printf("block");
            break;
        case IR_PARAMETER_TOP_LEVEL:
            IrTypePrint(&param->as.topLevel->type);
            break;

        case IR_PARAMETER_TYPE: break; // unreachable;
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
    [IR_INS_GET_ELEMENT_POINTER] = "get element pointer",
    [IR_INS_CAST] = "cast",
    [IR_INS_CALL] = "call",
    [IR_INS_SIZEOF] = "sizeof"
};

char* IrConditionNames[IR_COMPARE_MAX] = {
    [IR_COMPARE_GREATER] = "greater",
    [IR_COMPARE_EQUAL] = "equal",
    [IR_COMPARE_GREATER_EQUAL] = "greater equal",
    [IR_COMAPRE_LESS] = "less",
    [IR_COMPARE_NOT_EQUAL] = "not equal",
    [IR_COMPARE_LESS_EQUAL] = "less equal",
};

static void SSAInstructionPrint(unsigned int idx, SSAInstruction* inst, unsigned int gutterSize) {
    printf("%*d |   ", gutterSize, idx);
    if(inst->hasReturn) {
        IrParameter* param = inst->params - 1;
        IrParameterPrint(param, true);
        printf(" = ");
    }
    printf("%s", IrInstructionNames[inst->opcode]);

    if(inst->hasCondition) {
        printf(" %s", IrConditionNames[inst->comparison]);
    }

    for(unsigned int i = 0; i < inst->parameterCount; i++) {
        IrParameter* param = inst->params + i;
        printf(" ");
        IrParameterPrint(param, false);
    }

    printf("\n");
}

static void IrInstructionPrint(unsigned int idx, IrInstruction* inst, unsigned int gutterSize) {
    switch(inst->kind) {
        case IR_INSTRUCTION_SSA:
            SSAInstructionPrint(idx, &inst->as.ssa, gutterSize);
            break;
        case IR_INSTRUCTION_X64:
            x64InstructionPrint(idx, &inst->as.x64, gutterSize);
            break;
    }
}

static void IrBasicBlockPrint(IrBasicBlock* block, unsigned int gutterSize) {
    printf("%*s | @%lld", gutterSize, "", block->ID);

    if(block->predCount == 0) {
        printf(":\n");
    } else {
        printf("(");
        IrUsageData* predData = block->predecessors;
        for(unsigned int i = 0; i < block->predCount; i++) {
            if(i > 0) printf(", ");
            printf("@%lld", ((IrBasicBlock*)predData->source)->ID);
            predData = predData->prev;
        }
        printf("):\n");
    }

    ITER_PHIS(block, i, phi, {
        if(!phi->used) continue;
        printf("%*s |   ", gutterSize, "");
        IrParameterPrint(&phi->result, true);
        printf(" = phi");
        for(unsigned int j = 0; j < phi->paramCount; j++) {
            if(phi->params[j].ignore) continue;
            printf(" [@%lld ", phi->params[j].block->ID);
            IrParameterPrint(&phi->params[j].param, false);
            printf("]");
        }
        printf("\n");
    });

    ITER_INSTRUCTIONS(block, i, inst, {
        IrInstructionPrint(i, inst, gutterSize);
    });
}

static unsigned int intLength(unsigned int num) {
    return num == 0 ? 1 : floor(log10(num)) + 1;
}

static void IrFunctionPrint(IrTopLevel* ir) {
    IrFunction* fn = &ir->as.function;
    printf("function %.*s $%lld", ir->nameLength, ir->name, ir->ID);
    IrTypePrint(&ir->type);

    if(fn->blockCount == 0) {
        printf("\n\n");
        return;
    }

    printf(" {\n");

    unsigned int instrCount = 0;
    ITER_BLOCKS(fn, i, block, {
        if(block->instructionCount > 0) {
            // (max function)
            instrCount = instrCount > block->lastInstruction->ID
                ? instrCount : block->lastInstruction->ID;
        }
    });

    unsigned int gutterSize = intLength(instrCount);

    ITER_BLOCKS(fn, i, block, {
        IrBasicBlockPrint(block, gutterSize);
    });

    printf("}\n\n");
}

static void IrTopLevelPrint(IrContext* ctx, size_t idx) {
    IrTopLevel* ir = memoryArrayGet(&ctx->topLevel, idx);
    switch(ir->kind) {
        case IR_TOP_LEVEL_GLOBAL:
            IrGlobalPrint(ir);
            break;
        case IR_TOP_LEVEL_FUNCTION:
            IrFunctionPrint(ir);
            break;
    }
}

void IrContextPrint(IrContext* ctx) {
    for(unsigned int i = 0; i < ctx->topLevel.itemCount; i++) {
        IrTopLevelPrint(ctx, i);
    }
}