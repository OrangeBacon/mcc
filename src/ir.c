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
    memoryArrayAlloc(&ctx->vRegUsageData, pool, 128*MiB, sizeof(IrVirtualRegisterUsage));
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
    ARRAY_ALLOC(IrBasicBlock*, *block, predecessor);

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

    phi->result.as.virtualRegister->block = block;
    phi->var = var;
    phi->block = block;
    phi->result.as.virtualRegister->isPhi = true;
    phi->result.as.virtualRegister->loc.phi = phi;

    if(block->phiCount == 0) {
        phi->prev = NULL;
        block->firstPhi = block->lastPhi = phi;
    } else {
        phi->prev = block->lastPhi;
        block->lastPhi->next = phi;
        block->lastPhi = phi;
    }
    block->phiCount++;

    return phi;
}

static void IrInstructionSetReturnType(IrFunction* fn, IrInstruction* instruction);
void IrPhiAddOperand(IrContext* ctx, IrPhi* phi, IrBasicBlock* block, IrParameter* operand) {
    IrVirtualRegister* vreg = phi->result.as.virtualRegister;

    ARRAY_PUSH(*phi, param, ((IrPhiParameter) {0}));
    phi->params[phi->paramCount-1].block = block;
    IrParameterReference(&phi->params[phi->paramCount-1].param, operand);

    if(phi->paramCount == 1) {
        vreg->type = *IrParameterGetType(operand);

        // refresh return types incase it depended upon the type of the phi
        // which was previously null
        IrVirtualRegisterUsage* usage = vreg->users;
        for(unsigned int i = 0; i < vreg->useCount; i++) {
            if(!usage->isPhi) {
                IrInstruction* inst = usage->as.instruction;
                IrInstructionSetReturnType(block->fn, inst);
            }
            usage = usage->prev;
        }
    }

    if(operand->kind == IR_PARAMETER_PHI || operand->kind == IR_PARAMETER_REGISTER) {
        IrVirtualRegisterAddUsage(ctx, &phi->params[phi->paramCount-1].param, phi, true);
    }
}

IrVirtualRegister* IrVirtualRegisterCreate(IrFunction* fn) {
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

void IrVirtualRegisterAddUsage(IrContext* ctx, IrParameter* param, void* source, bool isPhi) {
    IrVirtualRegister* reg = param->as.virtualRegister;
    IrVirtualRegisterUsage* usage = memoryArrayPush(&ctx->vRegUsageData);
    usage->usage = param;
    usage->next = NULL;
    usage->as.phi = source;
    usage->isPhi = isPhi;

    if(reg->users == NULL) {
        usage->prev = NULL;
        reg->users = usage;
    } else {
        reg->users->next = usage;
        usage->prev = reg->users;
        reg->users = usage;
    }

    reg->useCount++;
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

void IrParameterTopLevel(IrParameter* param, IrTopLevel* top) {
    param->kind = IR_PARAMETER_TOP_LEVEL;
    param->as.topLevel = top;
}

void IrParameterPhi(IrParameter* param, IrPhi* phi) {
    param->kind = IR_PARAMETER_PHI;
    param->as.phi = phi;
}

void IrParameterReference(IrParameter* param, IrParameter* src) {
    switch(src->kind) {
        case IR_PARAMETER_BLOCK:
            IrParameterBlock(param, src->as.block); break;
        case IR_PARAMETER_CONSTANT:
            *param = *src; break;
        case IR_PARAMETER_TOP_LEVEL:
            IrParameterTopLevel(param, src->as.topLevel); break;
        case IR_PARAMETER_REGISTER:
            IrParameterVRegRef(param, src); break;
        case IR_PARAMETER_PHI:
            IrParameterPhi(param, src->as.phi); break;
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
        case IR_PARAMETER_REGISTER:
            return &param->as.virtualRegister->type;
        case IR_PARAMETER_TYPE:
            return &param->as.type;
        case IR_PARAMETER_PHI:
            return &param->as.phi->result.as.virtualRegister->type;
    }

    return NULL; // unreachable
}

static void IrInstructionSetReturnType(IrFunction* fn, IrInstruction* instruction) {
    IrType* ret = &instruction->params[-1].as.virtualRegister->type;
    switch(instruction->opcode) {
        case IR_INS_PARAMETER:
            *ret = fn->parameters[instruction->params[0].as.constant.value].as.type;
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
            *ret = *IrParameterGetType(&instruction->params[0]);
            break;
        case IR_INS_COMPARE:
        case IR_INS_NOT:
            ret->kind = IR_TYPE_INTEGER;
            ret->pointerDepth = 0;
            ret->as.integer = 8;
            break;
        case IR_INS_ALLOCA:
            *ret = instruction->params[0].as.type;
            ret->pointerDepth++;
            break;
        case IR_INS_LOAD:
            *ret = *IrParameterGetType(&instruction->params[0]);
            ret->pointerDepth--;
            break;
        case IR_INS_GET_ELEMENT_POINTER: {
            // note: this only works as gep is only used for pointer arithmetic
            // &a + 1, etc
            *ret = *IrParameterGetType(&instruction->params[0]);
        }; break;
        case IR_INS_CAST:
            *ret = instruction->params[0].as.type;
            break;
        case IR_INS_CALL:
            *ret = IrParameterGetType(&instruction->params[0])->as.function.retType->as.type;
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
    inst->returnTypeSet = false;

    return inst;
}

static void instructionVregUsageSet(IrContext* ctx, IrInstruction* inst) {
    unsigned int paramCount = inst->parameterCount;
    IrParameter* params = inst->params;
    for(unsigned int i = 0; i < paramCount; i++) {
        IrParameter* param = &params[i];
        if(param->kind == IR_PARAMETER_REGISTER) {
            IrVirtualRegisterAddUsage(ctx, param, inst, false);
        }
    }
}

IrInstruction* IrInstructionSetCreate(IrContext* ctx, IrBasicBlock* block, IrOpcode opcode, IrParameter* params, size_t paramCount) {
    IrInstruction* inst = addIns(ctx, block, opcode);

    inst->hasReturn = true;
    inst->params = params + 1;
    inst->parameterCount = paramCount - 1;
    params[0].as.virtualRegister->isPhi = false;
    params[0].as.virtualRegister->loc.inst = inst;
    params[0].as.virtualRegister->block = block;

    IrInstructionSetReturnType(block->fn, inst);
    instructionVregUsageSet(ctx, inst);

    return inst;
}

IrInstruction* IrInstructionVoidCreate(IrContext* ctx, IrBasicBlock* block, IrOpcode opcode, IrParameter* params, size_t paramCount) {
    IrInstruction* inst = addIns(ctx, block, opcode);

    inst->hasReturn = false;
    inst->params = params;
    inst->parameterCount = paramCount;

    instructionVregUsageSet(ctx, inst);

    return inst;
}

void IrTypeAddPointer(IrParameter* param) {
    param->as.type.pointerDepth++;
}

void IrInstructionCondition(IrInstruction* inst, IrComparison cmp) {
    inst->hasCondition = true;
    inst->comparison = cmp;
}

// --------------- //
// VARIABLE LOOKUP //
// --------------- //

// See Simple and Efficient Construction of Static SingleAssignment Form
// (https://c9x.me/compile/bib/braun13cc.pdf) for more infomation

void IrWriteVariable(IrFunction* fn, SymbolLocal* var, IrBasicBlock* block, IrParameter* value) {
    PAIRTABLE_SET(fn->variableTable, var, block, value);
}

IrParameter* IrReadVariableRecursive(IrFunction* fn, SymbolLocal* var, IrBasicBlock* block);
IrParameter* IrReadVariable(IrFunction* fn, SymbolLocal* var, IrBasicBlock* block) {
    if(pairPableHas(&fn->variableTable, var, block)) {
        // local value numbering
        return pairTableGet(&fn->variableTable, var, block);
    }

    // global value numbering
    return IrReadVariableRecursive(fn, var, block);
}

IrParameter* IrAddPhiOperands(IrFunction* fn, SymbolLocal* var, IrPhi* phi);
IrParameter* IrReadVariableRecursive(IrFunction* fn, SymbolLocal* var, IrBasicBlock* block) {
    IrParameter* val;

    if(!block->sealed) {
        // incomplete CFG
        IrPhi* phi = IrPhiCreate(fn->ctx, block, var);
        val = &phi->result;
        phi->incomplete = true;
    } else if(block->predecessorCount == 1) {
        // Optimise the common case of one predecessor: No phi needed
        val = IrReadVariable(fn, var, block->predecessors[0]);
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

IrParameter* IrTryRemoveTrivialPhi(IrPhi* phi);
IrParameter* IrAddPhiOperands(IrFunction* fn, SymbolLocal* var, IrPhi* phi) {
    for(unsigned int i = 0; i < phi->block->predecessorCount; i++) {
        IrBasicBlock* pred = phi->block->predecessors[i];
        IrPhiAddOperand(fn->ctx, phi, pred, IrReadVariable(fn, var, pred));
    }

    return IrTryRemoveTrivialPhi(phi);
}

bool IrParameterEqual(IrParameter* a, IrParameter* b) {
    if(a == NULL || b == NULL) return false;
    if(a->kind != b->kind) return false;

    switch(a->kind) {
        case IR_PARAMETER_BLOCK: return a->as.block == b->as.block;
        case IR_PARAMETER_PHI: return a->as.phi == b->as.phi;
        case IR_PARAMETER_REGISTER: return a->as.virtualRegister == b->as.virtualRegister;
        case IR_PARAMETER_TOP_LEVEL: return a->as.topLevel == b->as.topLevel;
        case IR_PARAMETER_TYPE: return IrTypeEqual(&a->as.type, &b->as.type);
        case IR_PARAMETER_CONSTANT: return
            a->as.constant.undefined == b->as.constant.undefined &&
            a->as.constant.value == b->as.constant.undefined &&
            IrTypeEqual(&a->as.constant.type, &b->as.constant.type);
    }

    printf("Unreachable\n"); exit(0);
}

IrParameter* IrTryRemoveTrivialPhi(IrPhi* phi) {
    IrParameter* same = NULL;

    for(unsigned int i = 0; i < phi->paramCount; i++) {
        IrPhiParameter* param = &phi->params[i];
        if (
            IrParameterEqual(&param->param, same) ||
            (param->param.kind == IR_PARAMETER_REGISTER &&
                param->param.as.virtualRegister->isPhi &&
                param->param.as.virtualRegister->loc.phi == phi)
        ) {
            continue; // unique value or self-reference
        }
        if(same != NULL) {
            return &phi->result; // the phi merges at least two values: not trivial
        }
        same = &param->param;
    }

    // remember all users exept the phi itsself
    IrVirtualRegister* reg = phi->result.as.virtualRegister;

    // reverse iterate double linked list
    IrVirtualRegisterUsage* usage = reg->users;
    for(unsigned int i = 0; i < reg->useCount; i++) {
        if(!(usage->isPhi && usage->as.phi == phi)) {
            // reroute all uses of phi to same
            *usage->usage = same != NULL? *same : (IrParameter){0};
            if(usage->isPhi) IrTryRemoveTrivialPhi(usage->as.phi);
        }
        usage = usage->prev;
    }

    // remove phi
    phi->used = false;

    return same;
}

void IrSealBlock(IrFunction* fn, IrBasicBlock* block) {
    if(block->sealed) return;
    ITER_PHIS(block, i, phi, {
        if(phi->incomplete && phi->used) IrAddPhiOperands(fn, phi->var, phi);
    });
    block->sealed = true;
}

// ------- //
// PRINTER //
// ------- //

void IrTypePrint(IrType* ir) {
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

void IrGlobalPrint(IrTopLevel* ir) {
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

void IrConstantPrint(IrConstant* ir) {
    if(ir->undefined) {
        printf("undefined");
    } else {
        printf("%d", ir->value);
    }
}

void IrParameterPrint(IrParameter* param, bool printType) {
    switch(param->kind) {
        case IR_PARAMETER_TYPE:
            IrTypePrint(&param->as.type);
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
        case IR_PARAMETER_TOP_LEVEL:
            printf("$%lld", param->as.topLevel->ID);
            break;
        case IR_PARAMETER_PHI:
            IrParameterPrint(&param->as.phi->result, printType);
            break;
    }

    if(!printType || param->kind == IR_PARAMETER_TYPE) return;

    printf(" : ");

    switch(param->kind) {
        case IR_PARAMETER_REGISTER:
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
        case IR_PARAMETER_PHI:
            IrTypePrint(&param->as.phi->result.as.virtualRegister->type);
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

void IrInstructionPrint(unsigned int idx, IrInstruction* inst, unsigned int gutterSize) {

    printf("%*d |   ", gutterSize, idx);
    if(inst->hasReturn) {
        IrParameter* param = inst->params - 1;
        IrParameterPrint(param, true);
        printf(" = ");
    }
    printf("%s ", IrInstructionNames[inst->opcode]);

    if(inst->hasCondition) {
        printf("%s ", IrConditionNames[inst->comparison]);
    }

    for(unsigned int i = 0; i < inst->parameterCount; i++) {
        IrParameter* param = inst->params + i;
        IrParameterPrint(param, false);
        printf(" ");
    }

    printf("\n");
}

void IrBasicBlockPrint(size_t idx, IrBasicBlock* block, unsigned int gutterSize) {
    printf("%*s | @%lld", gutterSize, "", idx);

    if(block->predecessorCount == 0) {
        printf(":\n");
    } else {
        printf("(");
        for(unsigned int i = 0; i < block->predecessorCount; i++) {
            if(i > 0) printf(", ");
            printf("@%lld", block->predecessors[i]->ID);
        }
        printf("):\n");
    }

    ITER_PHIS(block, i, phi, {
        if(!phi->used) continue;
        printf("%*s |   ", gutterSize, "");
        IrParameterPrint(&phi->result, true);
        printf(" = phi ");
        for(unsigned int j = 0; j < phi->paramCount; j++) {
            printf("[@%lld ", phi->params[j].block->ID);
            IrParameterPrint(&phi->params[j].param, false);
            printf("] ");
        }
        printf("\n");
    });

    ITER_INSTRUCTIONS(block, i, inst, {
        IrInstructionPrint(i, inst, gutterSize);
    });
}

unsigned int intLength(unsigned int num) {
    return num == 0 ? 1 : floor(log10(num)) + 1;
}

void IrFunctionPrint(IrTopLevel* ir) {
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
        IrBasicBlockPrint(i, block, gutterSize);
    });

    printf("}\n\n");
}

void IrTopLevelPrint(IrContext* ctx, size_t idx) {
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