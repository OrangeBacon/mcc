#ifndef IR_H
#define IR_H

#include "symbolTable.h"
#include "memory.h"
#include "x64Encode.h"

// This file describes an SSA IR used by this compiler.  It is strictly
// statically typed, each virtual register has a datatype, unlike LLVMIR.  This
// means that cast nodes are required to convert types explicitly; however
// allows types to not be required at every usage of a register.

// Example:
// the code:
// int a = 0;
// int add(int a, int b) {
//     int ret = a + b;
//     if(ret > 5) {
//         return a;
//     }
//     return ret;
// }
//
// could be represented in the ir as:
//
// global a $0 : i32 = 0
//
// function add $1(i32, i32) : i32 {
//   | @0:
// 0 |   %0 = parameter 0
// 1 |   %1 = parameter 1
// 2 |   %2 = add %2 %3
// 3 |   %3 = compare greater %2 5
// 4 |   jump if %3 @1 @2
//   | @1:
// 0 |   return $0
//   | @2:
// 0 |   return %2
// }
//
// which is significantly clearer and easier to work with than the equivalent
// ast, which at the time of writing is prints as 56 lines long.

// the type of an ir node
typedef struct IrType {

    // what sort of type is being represented
    enum {
        IR_TYPE_NONE,
        IR_TYPE_INTEGER,
        IR_TYPE_FUNCTION,
    } kind;

    // this is a pointer to the type
    unsigned int pointerDepth;

    union {
        // the size of the integer type, eg int = i32
        // i0 means any convinient integer size
        unsigned int integer;

        struct {
            struct IrParameter* retType;
            struct IrParameter* parameters;
            size_t parameterCount;
        } function;
    } as;
} IrType;

// a constant value that can be used at the global scope
// todo: possibly add top level instruction calls, eg allowing
// `int a = 5 + 6` to be represented
typedef struct IrConstant {
    int32_t value;
    bool undefined;

    // the type of the constant
    IrType type;
} IrConstant;

typedef enum IrUsageType {
    IR_USAGE_PHI,
    IR_USAGE_INSTRUCTION,
    IR_USAGE_PREDECESSOR,
} IrUsageType;

typedef struct IrUsageData {
    struct IrUsageData* prev;
    void* usageLocation;

    IrUsageType sourceType;
    void* source;

} IrUsageData;

// a virtual regiser
typedef struct IrVirtualRegister {
    size_t ID;

    // the instruction that created the register
    bool isPhi;
    union {
        struct IrInstruction* inst;
        struct IrPhi* phi;
    } loc;
    struct IrBasicBlock* block;

    unsigned int useCount;
    IrUsageData* users;

    bool hasType : 1;

    // the type of the value in the register
    IrType type;
} IrVirtualRegister;

// a parameter to an instruction in the ir
typedef struct IrParameter {

    // what sort of value is stored
    enum {
        // type, eg in cast instructions
        IR_PARAMETER_TYPE,

        // a virtual register
        IR_PARAMETER_VREG,

        // a basic block reference for jump instructions
        IR_PARAMETER_BLOCK,

        // a constant numerical value
        IR_PARAMETER_CONSTANT,

        IR_PARAMETER_TOP_LEVEL,
    } kind;

    // the value stored
    union {
        IrType type;
        IrVirtualRegister* virtualRegister;
        struct IrBasicBlock* block;
        IrConstant constant;
        struct IrTopLevel* topLevel;
    } as;
} IrParameter;

// possible comparisons used in instructions
// 3 bit, bit 0 = greater, bit 1 = equal, bit 2 = less than
// eg. 0b001 = greater, 0b110 = less than or equal to
typedef enum IrComparison {
    IR_COMPARE_GREATER = 0x1,
    IR_COMPARE_EQUAL = 0x2,
    IR_COMPARE_GREATER_EQUAL = 0x3,
    IR_COMAPRE_LESS = 0x4,
    IR_COMPARE_NOT_EQUAL = 0x5,
    IR_COMPARE_LESS_EQUAL = 0x6,
    IR_COMPARE_MAX, // largest comparison number
} IrComparison;

typedef enum IrOpcode {
    IR_INS_PARAMETER,
    IR_INS_ADD,
    IR_INS_COMPARE,
    IR_INS_JUMP_IF,
    IR_INS_RETURN,
    IR_INS_NEGATE, // 2's compliment
    IR_INS_NOT, // 1's compliment
    IR_INS_SUB,
    IR_INS_SMUL,
    IR_INS_SDIV,
    IR_INS_SREM,
    IR_INS_OR,
    IR_INS_AND,
    IR_INS_XOR,
    IR_INS_SHL,
    IR_INS_ASR,
    IR_INS_JUMP,
    IR_INS_ALLOCA,
    IR_INS_LOAD,
    IR_INS_STORE,
    IR_INS_GET_ELEMENT_POINTER,
    IR_INS_CAST,
    IR_INS_CALL,
    IR_INS_SIZEOF,
    IR_INS_MAX, // largest opcode number;
} IrOpcode;

typedef struct SSAInstruction {
    // flags
    IrComparison comparison : 3;
    bool hasReturn : 1;
    bool hasCondition : 1;
    bool returnTypeSet : 1;

    // what the operation is (todo: more operations, this is just an example)
    IrOpcode opcode;

    // Parameters to the instruction. If required, the previous parameter
    // is the return value
    IrParameter* params;

    // number of parameters in the list
    unsigned int parameterCount;
} SSAInstruction;

typedef enum IrInstructionType {
    IR_INSTRUCTION_SSA,
    IR_INSTRUCTION_X64,
} IrInstructionType;

// each instruction inside a basic block
typedef struct IrInstruction {
    size_t ID;

    // the instruction after this one
    struct IrInstruction* next;
    struct IrInstruction* prev;

    // where it is allocated
    struct IrBasicBlock* block;

    IrInstructionType kind;

    union {
        SSAInstruction ssa;
        x64Instruction x64;
    } as;
} IrInstruction;

typedef struct IrPhiParameter {
    bool ignore;

    IrParameter param;
    struct IrBasicBlock* block;
} IrPhiParameter;

typedef struct IrPhi {
    IrParameter result;

    ARRAY_DEFINE(IrPhiParameter, param);

    bool incomplete: 1;
    bool used : 1;
    bool returnTypeSet : 1;
    bool tryRemoveProcessing : 1;

    SymbolLocal* var;

    struct IrPhi* next;

    struct IrBasicBlock* block;
} IrPhi;

typedef struct IrBasicBlock {
    struct IrBasicBlock* next;
    size_t ID;

    // first instruction in the block
    IrInstruction* firstInstruction;
    size_t instructionCount;
    size_t maxInstructionCount;

    IrInstruction* lastInstruction;

    unsigned int useCount;
    IrUsageData* users;

    struct IrFunction* fn;

    unsigned int predCount;
    IrUsageData* predecessors;
    IrUsageData* lastPred;

    struct IrPhi* firstPhi;
    size_t phiCount;
    struct IrPhi* lastPhi;

    bool sealed : 1;

} IrBasicBlock;

// a function definition
typedef struct IrFunction {
    // return type, must equal types of everything returned from all
    // return instructions in the function
    IrParameter* returnType;

    // the types passed into the function index into parameters array,
    // access the values with a parameter instruction, continuous array
    IrParameter* parameters;
    size_t parameterCount;

    // first basic block
    IrBasicBlock* firstBlock;
    size_t blockCount;

    IrBasicBlock* lastBlock;
    IrVirtualRegister* lastVReg;

    // context used in creating this function
    struct IrContext* ctx;

    PairTable variableTable;

} IrFunction;

// any top level declaration
// todo: add struct definitions, etc
typedef struct IrTopLevel {
    size_t ID;

    enum {
        IR_TOP_LEVEL_GLOBAL,
        IR_TOP_LEVEL_FUNCTION,
    } kind;

    // the identifier to be use
    // could be exported and/or used in a call
    const char* name;
    unsigned int nameLength;

    IrType type;

    union {
        IrConstant global;
        IrFunction function;
    } as;
} IrTopLevel;

typedef struct IrContext {
    // all top level elements in the translation unit
    MemoryArray topLevel;

    // all basic blocks allocated
    MemoryArray basicBlocks;

    // all instructions
    MemoryArray instructions;

    // all parameters to instructions
    MemoryArray instParams;

    MemoryArray vReg;

    MemoryArray usageData;

    MemoryArray phi;
} IrContext;

extern char* IrInstructionNames[IR_INS_MAX];
extern char* IrConditionNames[IR_COMPARE_MAX];
void IrContextCreate(IrContext* ctx, MemoryPool* pool);
IrTopLevel* IrFunctionCreate(IrContext* ctx, const char* name, unsigned int nameLength, IrParameter* returnType, IrParameter* inType, size_t parameterCount);
IrTopLevel* IrGlobalPrototypeCreate(IrContext* ctx, const char* name, unsigned int nameLength);
void IrGlobalInitialize(IrTopLevel* global, size_t value, size_t size);
IrBasicBlock* IrBasicBlockCreate(IrFunction* fn);
IrParameter* IrParameterCreate(IrContext* ctx);
IrParameter* IrParametersCreate(IrContext* ctx, size_t count);
void IrParameterConstant(IrParameter* param, int value, int dataSize);
void IrParameterUndefined(IrParameter* param);
void IrParameterIntegerType(IrParameter* param, int size);
void IrParameterNewVReg(IrFunction* fn, IrParameter* param);
void IrParameterBlock(IrParameter* param, IrBasicBlock* block);
void IrParameterTopLevel(IrParameter* param, IrTopLevel* top);
void IrParameterReference(IrParameter* param, IrParameter* src);
IrType* IrParameterGetType(IrParameter* param);
IrInstruction* IrInstructionSetCreate(IrContext* ctx, IrBasicBlock* block, IrOpcode opcode, IrParameter* params, size_t paramCount);
IrInstruction* IrInstructionVoidCreate(IrContext* ctx, IrBasicBlock* block, IrOpcode opcode, IrParameter* params, size_t paramCount);
void IrInstructionCondition(IrInstruction* inst, IrComparison cmp);
void IrInvertCondition(IrInstruction* inst);
IrPhi* IrPhiCreate(IrContext* ctx, IrBasicBlock* block, SymbolLocal* var);
void IrPhiAddOperand(IrContext* ctx, IrPhi* phi, IrBasicBlock* block, IrParameter* operand);

void IrContextPrint(IrContext* ctx);

bool IrTypeEqual(IrType* a, IrType* b);

void IrWriteVariable(IrFunction* fn, SymbolLocal* var, IrBasicBlock* block, IrParameter* value);
IrParameter* IrReadVariable(IrFunction* fn, SymbolLocal* var, IrBasicBlock* block);
void IrSealBlock(IrFunction* fn, IrBasicBlock* block);
void IrTryRemoveTrivialBlocks(IrFunction* fn);

#endif