#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#include <stdio.h>
#include "x64.h"

typedef enum Register {
    RAX, RCX, RDX, R8, R9, RSP, RBP
} Register;

typedef enum ConditionCode {
    EQUAL, NOT_EQUAL, LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
} ConditionCode;

// stack ops
void asmPush(x64Ctx* ctx, Register reg);
void asmPop(x64Ctx* ctx, Register reg);

// arithmetic ops
void asmShiftLeftInt(x64Ctx* ctx, Register reg, int shift);
void asmNeg(x64Ctx* ctx, Register reg);
void asmNot(x64Ctx* ctx, Register reg);
void asmAdd(x64Ctx* ctx, Register src, Register dst);
void asmSub(x64Ctx* ctx, Register src, Register dst);
void asmIMul(x64Ctx* ctx, Register src, Register dst);
void asmIDiv(x64Ctx* ctx, Register src);
void asmOr(x64Ctx* ctx, Register src, Register dst);
void asmAnd(x64Ctx* ctx, Register src, Register dst);
void asmXor(x64Ctx* ctx, Register src, Register dst);
void asmShiftLeft(x64Ctx* ctx, Register src, Register dst);
void asmShiftRight(x64Ctx* ctx, Register src, Register dst);
void asmRAXExtend(x64Ctx* ctx);

void asmAddI(x64Ctx* ctx, Register reg, int shift);
void asmSubI(x64Ctx* ctx, Register reg, int shift);

void asmAddDeref(x64Ctx* ctx, Register src, Register dst);
void asmIMulDeref(x64Ctx* ctx, Register src, Register dst);
void asmOrDeref(x64Ctx* ctx, Register src, Register dst);
void asmAndDeref(x64Ctx* ctx, Register src, Register dst);
void asmXorDeref(x64Ctx* ctx, Register src, Register dst);
void asmAddIStoreRef(x64Ctx* ctx, int src, Register dst);
void asmSubIStoreRef(x64Ctx* ctx, int src, Register dst);
void asmIncDeref(x64Ctx* ctx, Register src);
void asmDecDeref(x64Ctx* ctx, Register src);

void asmCmp(x64Ctx* ctx, Register a, Register b);
void asmICmp(x64Ctx* ctx, Register a, int b);
void asmSetcc(x64Ctx* ctx, ConditionCode code, Register reg);
void asmRegSet(x64Ctx* ctx, Register dst, long long int value);
void asmRegMov(x64Ctx* ctx, Register src, Register dst);
void asmRegMovAddr(x64Ctx* ctx, Register src, Register dst);
void asmDeref(x64Ctx* ctx, Register src, Register dst);
void asmDerefOffset(x64Ctx* ctx, Register src, int off, Register dst);
void asmLeaDerefOffset(x64Ctx* ctx, Register src, int off, Register dst);

void asmJump(x64Ctx* ctx, int target);
void asmJumpCC(x64Ctx* ctx, ConditionCode code, int target);
void asmJumpTarget(x64Ctx* ctx, int target);
void asmRet(x64Ctx* ctx);
void asmCallIndir(x64Ctx* ctx, Register reg);

void asmGlobl(x64Ctx* ctx, int len, const char* name);
void asmFnName(x64Ctx* ctx, int len, const char* name);
void asmSection(x64Ctx* ctx, const char* section);
void asmAlign(x64Ctx* ctx, int bytes);
void asmLong(x64Ctx* ctx, int value);
void asmLoadName(x64Ctx* ctx, int len, const char* name, Register dst);
void asmComm(x64Ctx* ctx, int len, const char* name, int size);

#endif