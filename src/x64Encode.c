#include "x64Encode.h"

#include <stdlib.h>
#include <assert.h>
#include "ir.h"

// ------------------ //
// x64 Pretty Printer //
// ------------------ //

char* x64OpcodeNames[x64_MAX_OPCODE] = {
    [x64_PUSH] = "push",
    [x64_POP] = "pop",
    [x64_SHL] = "shl",
    [x64_NEG] = "neg",
    [x64_NOT] = "not",
    [x64_ADD] = "add",
    [x64_SUB] = "sub",
    [x64_IMUL] = "imul",
    [x64_IDIV] = "idiv",
    [x64_OR] = "or",
    [x64_AND] = "and",
    [x64_XOR] = "xor",
    [x64_SAL] = "sal",
    [x64_SAR] = "sar",
    [x64_CQO] = "cqo",
    [x64_CMP] = "cmp",
    [x64_INC] = "inc",
    [x64_DEC] = "dec",
    [x64_SETCC] = "set",
    [x64_MOV] = "mov",
    [x64_LEA] = "lea",
    [x64_JMP] = "jmp",
    [x64_JCC] = "j",
    [x64_RET] = "ret",
    [x64_CALL] = "call",
};

char* x64ConditionCodeNames[x64_MAX_CC] = {
    [x64_OVERFLOW] = "O",
    [x64_NO_OVERFLOW] = "NO",
    [x64_BELOW] = "B",
    [x64_NOT_BELOW] = "NB",
    [x64_EQUAL] = "E",
    [x64_NOT_EQUAL] = "NE",
    [x64_BELOW_EQUAL] = "BE",
    [x64_NOT_BELOW_EQUAL] = "NBE",
    [x64_SIGN] = "S",
    [x64_NO_SIGN] = "NS",
    [x64_PARITY_EVEN] = "PE",
    [x64_PARITY_ODD] = "PO",
    [x64_LESS] = "L",
    [x64_GREATER_EQUAL] = "GE",
    [x64_LESS_EQUAL] = "LE",
    [x64_GREATER] = "G",
};

void x64InstructionPrint(unsigned int idx, x64Instruction* inst, unsigned int gutterSize) {
    (void)idx;
    (void)inst;
    (void)gutterSize;
}
/*
void push(IrContext* ctx,x64Operand op) {
    memoryArrayPush(ctx->x64);
}

void test(IrContext* ctx, IrParameter* param) {
    replace();
    inst = insert_after();
    push(inst, reg(x64_AX));
    inst = insert_after(inst);
    setcc(inst, x64_LESS, regr(x64_BX, NULL), regw(param));
}
*/

// --------------------- //
// x64 Utility Functions //
// --------------------- //

// register argument that is read by the instruction
x64Operand regr(IrParameter* vreg) {
    return (x64Operand) {
        .type = x64_REGISTER,
        .as.reg = {
            .mod = x64_DIRECT_R,
            .base.vreg = vreg->as.virtualRegister,
        },
    };
}

// register argument that is writen by the instruction
x64Operand regw(IrParameter* vreg) {
    return (x64Operand) {
        .type = x64_REGISTER,
        .as.reg = {
            .mod = x64_DIRECT_W,
            .base.vreg = vreg->as.virtualRegister,
        },
    };
}

// register argument that is read and written by the instruction
x64Operand regrw(IrParameter* vregr, IrParameter* vregw) {
    return (x64Operand) {
        .type = x64_REGISTER,
        .as.reg = {
            .mod = x64_DIRECT_RW,
            .base.vreg = vregr->as.virtualRegister,
            .index.vreg = vregw->as.virtualRegister,
        },
    };
}

// argument dereferencing memory, possibly including SIB byte
x64Operand memaddr(IrParameter* vregb, IrParameter* vregi,
    uint8_t scale, uint32_t disp) {
    return (x64Operand) {
        .type = x64_REGISTER,
        .as.reg = {
            .mod = x64_INDIRECT,
            .base.vreg = vregb->as.virtualRegister,
            .index.vreg = vregi->as.virtualRegister,
            .scale = scale,
            .ripRelative = false,
            .usesSymbol = false,
            .disp.disp32 = disp,
        },
    };
}

// RIP relative addressing to symbol
x64Operand riprel(IrTopLevel* top) {
    return (x64Operand) {
        .type = x64_REGISTER,
        .as.reg = {
            .mod = x64_INDIRECT,
            .ripRelative = true,
            .disp.topLevel = top,
        },
    };
}

// relative addressing to block address
x64Operand blockrel(IrBasicBlock* block) {
    return (x64Operand) {
        .type = x64_IMMEDIATE,
        .as.imm = {
            .size = x64_IMM_SYMBOL,
            .as.block = block,
        },
    };
}

// 8 bit constant value
x64Operand imm8(uint8_t imm) {
    return (x64Operand) {
        .type = x64_IMMEDIATE,
        .as.imm = {
            .size = x64_IMM8,
            .as.imm8 = imm,
        },
    };
}

// 16 bit constant value
x64Operand imm16(uint16_t imm) {
    return (x64Operand) {
        .type = x64_IMMEDIATE,
        .as.imm = {
            .size = x64_IMM16,
            .as.imm16 = imm,
        },
    };
}

// 32 bit constant value
x64Operand imm32(uint32_t imm) {
    return (x64Operand) {
        .type = x64_IMMEDIATE,
        .as.imm = {
            .size = x64_IMM32,
            .as.imm32 = imm,
        },
    };
}

// condition code argument
x64Operand cc(x64ConditionCode code) {
    return (x64Operand) {
        .type = x64_CONDITION_CODE,
        .as.code = code,
    };
}

// reference to parameter, read only
x64Operand refr(IrParameter* param) {
    switch(param->kind) {
        case IR_PARAMETER_BLOCK: return blockrel(param->as.block);
        case IR_PARAMETER_CONSTANT: {
            int32_t c = param->as.constant.value;
            if(c < INT8_MAX && c > INT8_MIN) {
                return imm8(c);
            } else if(c < INT16_MAX && c > INT16_MIN) {
                return imm16(c);
            } else {
                return imm32(c);
            }
        }
        case IR_PARAMETER_PHI: return regr(&param->as.phi->result);
        case IR_PARAMETER_TOP_LEVEL: return riprel(param->as.topLevel);
        case IR_PARAMETER_VREG: return regr(param);
        case IR_PARAMETER_TYPE:
            printf("Cannot lower type to assembly");
            exit(1);
    }

    printf("unreachable"); exit(1);
}

// ------------------ //
// x64 Opcode Helpers //
// ------------------ //

#define x64Inst(instruction, oc, count) \
    instruction->kind = IR_INSTRUCTION_X64; \
    x64Instruction* inst = &(instruction)->as.x64; \
    inst->opcode = oc; \
    inst->operandCount = count; \
    inst->operands = (count) == 1 \
        ? memoryArrayPush(&ctx->operands) \
        : memoryArrayPushN(&ctx->operands, (count))

#define argReg(arg) ((arg).type == x64_REGISTER && (arg).as.reg.mod != x64_INDIRECT)
#define argRM(arg) ((arg).type == x64_REGISTER)
#define argImm(arg) ((arg).type == x64_IMMEDIATE)
#define argR(arg) ((arg).as.reg.mod == x64_INDIRECT || (arg).as.reg.mod == x64_DIRECT_R)
#define argW(arg) ((arg).as.reg.mod == x64_INDIRECT || (arg).as.reg.mod == x64_DIRECT_W)
#define argRW(arg) ((arg).as.reg.mod == x64_INDIRECT || (arg).as.reg.mod == x64_DIRECT_RW)
#define argNo(arg) ((arg).type == x64_INVALID_OPERAND)

void push(x64Context* ctx, IrInstruction* i, x64Operand arg1) {
    assert(argReg(arg1) || argImm(arg1));
    if(arg1.type == x64_REGISTER) {
        assert(argR(arg1));
    }

    x64Inst(i, x64_PUSH, 1);
    *inst->operands = arg1;
}

void pop(x64Context* ctx, IrInstruction* i, x64Operand arg1) {
    assert(argRM(arg1) && argW(arg1));

    x64Inst(i, x64_POP, 1);
    inst->operands[0] = arg1;
}

void shl(x64Context* ctx, IrInstruction* i, x64Operand arg1, x64Operand arg2) {
    assert(argReg(arg1) && argRW(arg1));
    assert((argImm(arg2) && arg2.as.imm.size == x64_IMM8) || argR(arg2));

    x64Inst(i, x64_SHL, 2);

    // otherwise not representable in assembly
    if(arg2.type == x64_REGISTER) {
        arg2.as.reg.base.reg = x64_CX;
    }

    inst->operands[0] = arg1;
    inst->operands[1] = arg2;
}

void neg(x64Context* ctx, IrInstruction* i, x64Operand arg1) {
    assert(argRW(arg1));

    x64Inst(i, x64_NEG, 1);
    inst->operands[0] = arg1;
}

void not(x64Context* ctx, IrInstruction* i, x64Operand arg1) {
    assert(argRW(arg1));

    x64Inst(i, x64_NOT, 1);
    inst->operands[0] = arg1;
}

void add(x64Context* ctx, IrInstruction* i, x64Operand arg1, x64Operand arg2) {
    assert((argReg(arg1) && argRW(arg1) && argR(arg2))
        || (argRM(arg1) && argRW(arg1) && argReg(arg2) && argR(arg2))
        || (argRW(arg1) && argImm(arg2))
        || (argImm(arg1) && argRW(arg2)));

    // immediate must come second
    if(arg1.type == x64_IMMEDIATE) {
        x64Operand tmp = arg1;
        arg1 = arg2;
        arg2 = tmp;
    }

    x64Inst(i, x64_ADD, 2);
    inst->operands[0] = arg1;
    inst->operands[1] = arg2;
}

void sub(x64Context* ctx, IrInstruction* i, x64Operand arg1, x64Operand arg2) {
    assert((argReg(arg1) && argRW(arg1) && argR(arg2))
        || (argRM(arg1) && argRW(arg1) && argReg(arg2) && argR(arg2))
        || (argRW(arg1) && argImm(arg2)));

    x64Inst(i, x64_ADD, 2);
    inst->operands[0] = arg1;
    inst->operands[1] = arg2;
}

void imul(x64Context* ctx, IrInstruction* i, x64Operand arg1, x64Operand arg2, x64Operand arg3) {
    assert((argRW(arg1) && argNo(arg2) && argNo(arg3))
        || (argReg(arg1) && argRW(arg1) && argR(arg2) && argNo(arg3))
        || (argReg(arg1) && argRW(arg1) && argR(arg2) && argImm(arg3)));

    int argCount = 3 - argNo(arg2) - argNo(arg3);
    x64Inst(i, x64_IMUL, argCount);
    inst->operands[0] = arg1;
    if(argCount > 1) inst->operands[1] = arg2;
    if(argCount > 2) inst->operands[2] = arg3;
}

#undef x64Inst
#undef argReg
#undef argRM
#undef argIMM
#undef argR
#undef argW
#undef argRW

// --------------- //
// x64 IR Lowering //
// --------------- //

void x64LowerIr(IrContext* ctx) {
    (void) ctx;
}