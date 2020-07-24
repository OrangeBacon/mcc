#include "x64Encode.h"

#include <stdlib.h>
#include "ir.h"

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

// register argument that is read by the instruction
x64Operand regr(x64BaseRegister reg, IrParameter* vreg) {
    return (x64Operand) {
        .type = x64_REGISTER,
        .as.reg = {
            .mod = x64_DIRECT_R,
            .base = {.reg = reg, .vreg = vreg->as.virtualRegister},
        },
    };
}

// register argument that is writen by the instruction
x64Operand regw(x64BaseRegister reg, IrParameter* vreg) {
    return (x64Operand) {
        .type = x64_REGISTER,
        .as.reg = {
            .mod = x64_DIRECT_W,
            .base = {.reg = reg, .vreg = vreg->as.virtualRegister},
        },
    };
}

// register argument that is read and written by the instruction
x64Operand regrw(x64BaseRegister regr, IrParameter* vregr,
    x64BaseRegister regw, IrParameter* vregw) {
    return (x64Operand) {
        .type = x64_REGISTER,
        .as.reg = {
            .mod = x64_DIRECT_RW,
            .base = {.reg = regr, .vreg = vregr->as.virtualRegister},
            .index = {.reg = regw, .vreg = vregw->as.virtualRegister},
        },
    };
}

// argument dereferencing memory, possibly including SIB byte
x64Operand memaddr(x64BaseRegister regb, IrParameter* vregb,
    x64BaseRegister regi, IrParameter* vregi,
    uint8_t scale, uint32_t disp) {
    return (x64Operand) {
        .type = x64_REGISTER,
        .as.reg = {
            .mod = x64_INDIRECT,
            .base = {.reg = regb, .vreg = vregb->as.virtualRegister},
            .index = {.reg = regi, .vreg = vregi->as.virtualRegister},
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
        case IR_PARAMETER_PHI: return regr(0, &param->as.phi->result);
        case IR_PARAMETER_TOP_LEVEL: return riprel(param->as.topLevel);
        case IR_PARAMETER_VREG: return regr(0, param);
        case IR_PARAMETER_TYPE:
            printf("Cannot lower type to assembly");
            exit(1);
    }

    printf("unreachable"); exit(1);
}