#include "x64Encode.h"
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
    insert_after();
    insert_before();
    remove();
    push(ctx, reg(x64_AX));
    setcc(ctx, x64_LESS, reg(ctx, x64_BX), vreg(ctx, param));
}
*/
