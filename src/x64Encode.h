#ifndef X64_ENCODE_H
#define X64_ENCODE_H

#include <stdint.h>
#include <stdbool.h>
#include "memory.h"

// See Intel® 64 and IA-32 Architectures Software Developer’s Manual ->
//   Volume 1: Basic Architecture
//   Volume 2: Instruction Set Reference
// for more infomation

typedef struct x64Context {
    MemoryArray operands;
} x64Context;

typedef enum x64BaseRegister {
    x64_REG_UNDEFINED,
    x64_AX,
    x64_CX,
    x64_DX,
    x64_BX,
    x64_SP,
    x64_BP,
    x64_SI,
    x64_DI,
    x64_R8,
    x64_R9,
    x64_R10,
    x64_R11,
    x64_R12,
    x64_R13,
    x64_R14,
    x64_R15,
} x64BaseRegister;

typedef enum x64Opcode {
    x64_PUSH,
    x64_POP,
    x64_SHL,
    x64_NEG,
    x64_NOT,
    x64_ADD,
    x64_SUB,
    x64_IMUL,
    x64_IDIV,
    x64_OR,
    x64_AND,
    x64_XOR,
    x64_SAL,
    x64_SAR,
    x64_CQO,
    x64_CMP,
    x64_INC,
    x64_DEC,
    x64_SETCC,
    x64_MOV,
    x64_LEA,
    x64_JMP,
    x64_JCC,
    x64_RET,
    x64_CALL,
    x64_MAX_OPCODE,
} x64Opcode;

typedef enum x64ConditionCode {
    x64_OVERFLOW = 0,
    x64_NO_OVERFLOW = 1,
    x64_BELOW = 2,
    x64_NOT_ABOVE_EQUAL = 2,
    x64_NOT_BELOW = 3,
    x64_ABOVE_EQUAL = 3,
    x64_EQUAL = 4,
    x64_ZERO = 4,
    x64_NOT_EQUAL = 5,
    x64_NOT_ZERO = 5,
    x64_BELOW_EQUAL = 6,
    x64_NOT_ABOVE = 6,
    x64_NOT_BELOW_EQUAL = 7,
    x64_ABOVE = 7,
    x64_SIGN = 8,
    x64_NO_SIGN = 9,
    x64_PARITY = 10,
    x64_PARITY_EVEN = 10,
    x64_NO_PARITY = 11,
    x64_PARITY_ODD = 11,
    x64_LESS = 12,
    x64_NOT_GREATER_EQUAL = 12,
    x64_NOT_LESS = 13,
    x64_GREATER_EQUAL = 13,
    x64_LESS_EQUAL = 14,
    x64_NOT_GREATER = 14,
    x64_NOT_LESS_EQUAL = 15,
    x64_GREATER = 15,
    x64_MAX_CC,
} x64ConditionCode;

typedef struct x64Register {
    struct IrVirtualRegister* vreg;
    x64BaseRegister reg;
} x64Register;

typedef enum x64ReducedModField {
    x64_INDIRECT,
    x64_DIRECT_W,
    x64_DIRECT_R,
    x64_DIRECT_RW,
    x64_MAX_REDUCED_MOD,
} x64ReducedModField;

typedef enum x64Scale {
    x64_SCALE_1,
    x64_SCALE_2,
    x64_SCALE_4,
    x64_SCALE_8,
} x64Scale;

typedef struct x64RegisterOperand {
    _Static_assert(x64_MAX_REDUCED_MOD >= 4, "x64ReduceMod member count too large");
    uint8_t mod : 2; // enum x64ReducedMod
    uint8_t scale : 2; // 1/2/4/8
    bool ripRelative : 1;
    bool usesSymbol: 1;

    // sib base, modr/m.r/m, direct read reg
    x64Register base;

    // sib index, direct write reg
    x64Register index;
    union {
        uint32_t disp32;
        struct IrTopLevel* topLevel;
    } disp;
} x64RegisterOperand;

typedef enum x64ImmediateSize {
    x64_IMM8,
    x64_IMM16,
    x64_IMM32,
    x64_IMM_SYMBOL,
} x64ImmediateSize;

typedef struct x64ImmediateOperand {
    x64ImmediateSize size;
    union {
        uint8_t imm8;
        uint16_t imm16;
        uint32_t imm32;
        struct IrBasicBlock* block;
    } as;
} x64ImmediateOperand;

typedef enum x64OperandType {
    x64_INVALID_OPERAND = 0,
    x64_REGISTER,
    x64_IMMEDIATE,
    x64_CONDITION_CODE,
} x64OperandType;

// operand to x64 assembly instruction
// undefined if condition code used beyond argument 1
typedef struct x64Operand {
    enum x64OperandType type;

    union {
        x64RegisterOperand reg;
        x64ImmediateOperand imm;
        x64ConditionCode code;
    } as;
} x64Operand;

typedef struct x64Instruction {
    x64Opcode opcode;
    uint8_t operandCount;
    x64Operand* operands;
} x64Instruction;

void x64InstructionPrint(unsigned int idx, x64Instruction* inst, unsigned int gutterSize);

/*
op %0
op rax
op %0=rax
op ->%0
op ->rdx
op ->%0=rax
op %0->%1
op %0->rax
op %0=rax->rax=%1 // equiv: op %0=rax=>%1
op rax->%1
op rax->rax // equiv: op *rax
op [%0+%1*2+3]
op [rip+main]
op 8'5
op 16'257
op 32'784563
*/

// more complex examples?
// cqo %0=rax, ->rdx=%1
//   reads from %0, which must be in rax, writes %1 in rdx
// idiv %0=rax->rax=%3, %1=rdx->rdx=%4, %2=rcx
//   reads from %0 in rax, %1 in rdx, %2 in rcx, writes %2 in rax, %3 in rdx
// == cqoidiv(%3, %4, %0, %2)
// cqoidiv(divRes, remRes, dividend, divisor)

/*

.mod {
    00 => [reg]
    01 => [reg+disp8]
    10 => [reg+disp32]
    11 => reg
}

when .mod != 11 && .rm == 100 {
    uses sib
}

modrm = {
    .reg = {opcode extension | (REX.R + this) = register}: 3
    .rm  = {direct/indirect register (REX.B + this) = register}: 3
}

sib = {
    .scale = {0=>1, 1=>2, 2=>4, 3=>8} : 2
    .index = REX.X register : 3
    .base = REX.B register : 3
}

opcode = 1,2,3 bytes

displacement = 1,2,4,8 bytes
    if == 8, then no immediate

immediate = 1,2,4,8 bytes

registers {
                                AH , CH , DH , BH , // without REX
    [08] = {AL , CL , DL , BL , SPL, BPL, SIL, DIL, R8L, R9L, R10L, R11L, R12L, R13L, R14L, R15L}
    [16] = {AX , CX , DX , BX , SP , BP , SI , DI , R8W, R9W, R10W, R11W, R12W, R13W, R14W, R15W}
    [32] = {EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI, R8D, R9D, R10D, R11D, R12D, R13D, R14D, R15D}
    [64] = {RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8 , R9 , R10 , R11 , R12 , R13 , R14 , R15 }
}
*/

struct IrContext;
void x64LowerIr(struct IrContext* ctx);

#endif