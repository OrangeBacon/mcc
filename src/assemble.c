#define __USE_MINGW_ANSI_STDIO 1
#include "assemble.h"

static char* registerNames[] = {
    "%rax", "%rcx", "%rdx", "%r8", "%r9", "%rsp", "%rbp",
};

static char* reg8Names[] = {
    "%al", "%cl", "%dl", "%r8b", "%r9b", "%spl", "%bpl",
};

static char* conditionNames[] = {
    "e", "ne", "l", "le", "g", "ge",
};

void asmPush(x64Ctx* ctx, Register reg) {
    fprintf(ctx->f, "\tpush %s\n", registerNames[reg]);
    ctx->stackIndex -= 8;
}

void asmPop(x64Ctx* ctx, Register reg) {
    fprintf(ctx->f, "\tpop %s\n", registerNames[reg]);
    ctx->stackIndex += 8;
}

void asmShiftLeftInt(x64Ctx* ctx, Register reg, int shift) {
    fprintf(ctx->f, "\tshl $%d, %s\n", shift, registerNames[reg]);
}

void asmNeg(x64Ctx* ctx, Register reg) {
    fprintf(ctx->f, "\tneg %s\n", registerNames[reg]);
}

void asmNot(x64Ctx* ctx, Register reg) {
    fprintf(ctx->f, "\tnot %s\n", registerNames[reg]);
}

void asmAdd(x64Ctx* ctx, Register src, Register dst) {
    fprintf(ctx->f, "\tadd %s, %s\n", registerNames[src], registerNames[dst]);
}

void asmSub(x64Ctx* ctx, Register src, Register dst) {
    fprintf(ctx->f, "\tsub %s, %s\n", registerNames[src], registerNames[dst]);
}

void asmIMul(x64Ctx* ctx, Register src, Register dst) {
    fprintf(ctx->f, "\timul %s, %s\n", registerNames[src], registerNames[dst]);
}

void asmIDiv(x64Ctx* ctx, Register src) {
    fprintf(ctx->f, "\tidiv %s\n", registerNames[src]);
}

void asmOr(x64Ctx* ctx, Register src, Register dst) {
    fprintf(ctx->f, "\tor %s, %s\n", registerNames[src], registerNames[dst]);
}

void asmAnd(x64Ctx* ctx, Register src, Register dst) {
    fprintf(ctx->f, "\tand %s, %s\n", registerNames[src], registerNames[dst]);
}

void asmXor(x64Ctx* ctx, Register src, Register dst){
    fprintf(ctx->f, "\txor %s, %s\n", registerNames[src], registerNames[dst]);
}

void asmShiftLeft(x64Ctx* ctx, Register src, Register dst){
    fprintf(ctx->f, "\tsal %s, %s\n", reg8Names[src], registerNames[dst]);
}

void asmShiftRight(x64Ctx* ctx, Register src, Register dst) {
    fprintf(ctx->f, "\tsar %s, %s\n", reg8Names[src], registerNames[dst]);
}

void asmRAXExtend(x64Ctx* ctx) {
    fprintf(ctx->f, "\tcqo\n");
}

void asmAddI(x64Ctx* ctx, Register reg, int shift) {
    fprintf(ctx->f, "\tadd $%d, %s\n", shift, registerNames[reg]);
    if(reg == RSP) {
        ctx->stackIndex += shift;
    }
}
void asmSubI(x64Ctx* ctx, Register reg, int shift) {
    fprintf(ctx->f, "\tsub $%d, %s\n", shift, registerNames[reg]);
    if(reg == RSP) {
        ctx->stackIndex -= shift;
    }
}

void asmAddDeref(x64Ctx* ctx, Register src, Register dst){
    fprintf(ctx->f, "\tadd (%s), %s\n", registerNames[src], registerNames[dst]);
}

void asmIMulDeref(x64Ctx* ctx, Register src, Register dst){
    fprintf(ctx->f, "\timul (%s), %s\n", registerNames[src], registerNames[dst]);
}

void asmOrDeref(x64Ctx* ctx, Register src, Register dst){
    fprintf(ctx->f, "\tor (%s), %s\n", registerNames[src], registerNames[dst]);
}

void asmAndDeref(x64Ctx* ctx, Register src, Register dst){
    fprintf(ctx->f, "\tand (%s), %s\n", registerNames[src], registerNames[dst]);
}

void asmXorDeref(x64Ctx* ctx, Register src, Register dst){
    fprintf(ctx->f, "\txor (%s), %s\n", registerNames[src], registerNames[dst]);
}


void asmCmp(x64Ctx* ctx, Register a, Register b) {
    fprintf(ctx->f, "\tcmp %s, %s\n", registerNames[a], registerNames[b]);
}

void asmAddIStoreRef(x64Ctx* ctx, int src, Register dst) {
    fprintf(ctx->f, "\tadd $%d, (%s)\n", src, registerNames[dst]);
}

void asmSubIStoreRef(x64Ctx* ctx, int src, Register dst) {
    fprintf(ctx->f, "\tsub $%d, (%s)\n", src, registerNames[dst]);
}

void asmIncDeref(x64Ctx* ctx, Register src) {
    fprintf(ctx->f, "\tincq (%s)\n", registerNames[src]);
}

void asmDecDeref(x64Ctx* ctx, Register src) {
    fprintf(ctx->f, "\tdecq (%s)\n", registerNames[src]);
}

void asmICmp(x64Ctx* ctx, Register a, int b) {
    fprintf(ctx->f, "\tcmp $%d, %s\n", b, registerNames[a]);
}

void asmSetcc(x64Ctx* ctx, ConditionCode code, Register reg) {
    fprintf(ctx->f, "\tset%s %s\n", conditionNames[code], reg8Names[reg]);
}

void asmRegSet(x64Ctx* ctx, Register dst, long long int value) {
    fprintf(ctx->f, "\tmov $%#llx, %s\n", value, registerNames[dst]);
}

void asmRegMov(x64Ctx* ctx, Register src, Register dst) {
    fprintf(ctx->f, "\tmov %s, %s\n", registerNames[src], registerNames[dst]);
}

void asmRegMovAddr(x64Ctx* ctx, Register src, Register dst) {
    fprintf(ctx->f, "\tmov %s, (%s)\n", registerNames[src], registerNames[dst]);
}

void asmDeref(x64Ctx* ctx, Register src, Register dst){
    fprintf(ctx->f, "\tmov (%s), %s\n", registerNames[src], registerNames[dst]);
}

void asmDerefOffset(x64Ctx* ctx, Register src, int off, Register dst) {
    fprintf(ctx->f, "\tmov %d(%s), %s\n", off, registerNames[src], registerNames[dst]);
}

void asmLeaDerefOffset(x64Ctx* ctx, Register src, int off, Register dst) {
    fprintf(ctx->f, "\tlea %d(%s), %s\n", off, registerNames[src], registerNames[dst]);
}

void asmJump(x64Ctx* ctx, int target) {
    fprintf(ctx->f, "\tjmp _%d\n", target);
}

void asmJumpCC(x64Ctx* ctx, ConditionCode code, int target) {
    fprintf(ctx->f, "\tj%s _%d\n", conditionNames[code], target);
}

void asmJumpTarget(x64Ctx* ctx, int target) {
    fprintf(ctx->f, "_%d:\n", target);
}

void asmRet(x64Ctx* ctx) {
    fprintf(ctx->f, "\tret\n");
}

void asmCallIndir(x64Ctx* ctx, Register reg) {
    fprintf(ctx->f, "\tcall *%s\n", registerNames[reg]);
}

void asmGlobl(x64Ctx* ctx, int len, const char* name) {
    fprintf(ctx->f, ".globl %.*s\n", len, name);
}

void asmFnName(x64Ctx* ctx, int len, const char* name) {
    fprintf(ctx->f, "%.*s:\n", len, name);
}

void asmSection(x64Ctx* ctx, const char* section) {
    fprintf(ctx->f, "\t.%s\n", section);
}

void asmAlign(x64Ctx* ctx, int bytes) {
    fprintf(ctx->f, "\t.balign %d\n", bytes);
}

void asmLong(x64Ctx* ctx, int value) {
    fprintf(ctx->f, "\t.long %d\n", value);
}

void asmLoadName(x64Ctx* ctx, int len, const char* name, Register dst) {
    fprintf(ctx->f, "\tlea %.*s(%%rip), %s\n", len, name, registerNames[dst]);
}

void asmComm(x64Ctx* ctx, int len, const char* name, int size) {
    fprintf(ctx->f, "\t.comm %.*s,%d\n", len, name, size);
}