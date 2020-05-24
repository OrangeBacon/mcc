#ifndef IR_H
#define IR_H

#include "memory.h"

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
// 0 |   %2 = parameter 0
// 1 |   %3 = parameter 1
// 2 |   %4 = add %2 %3
// 3 |   %5 = compare greater %4 5
// 4 |   jump_if %5 @1 @2
//   | @1:
// 5 |   return $0
//   | @2:
// 6 |   return %4
// }
//
// which is significantly clearer and easier to work with than the equivalent
// ast, which at the time of writing is prints as 56 lines long.

// the type of an ir node
typedef struct IrType {

    // what sort of type is being represented
    enum {
        IR_TYPE_INTEGER,
        IR_TYPE_POINTER
    } kind;

    union {
        // the size of the integer type, eg int = i32
        // i0 means any convinient integer size
        unsigned int integer;

        // pointer type infomation
        struct {

            // how many levels of indirection, eg int*** => 3
            // hopefully reduces allocations of IrType structs
            unsigned int depth;

            // the type being pointed to
            struct IrType* type;
        } pointer;
    } as;
} IrType;

// a constant value that can be used at the global scope
// todo: possibly add top level instruction calls, eg allowing
// `int a = 5 + 6` to be represented
typedef struct IrConstant {
    int value;

    // the type of the constant
    IrType type;
} IrConstant;

// identifier for a virtual regiser
typedef struct IrRegisterID {
    // the unique to the function id
    unsigned int id;

    // the instruction that created the register
    struct IrInstruction* location;

    // the type of the value in the register
    IrType type;
} IrRegisterID;

// a global variable
typedef struct IrGlobal {

    // the value stored in the global
    IrConstant value;
} IrGlobal;

// a parameter to an instruction in the ir
typedef struct IrParameter {

    // what sort of value is stored
    enum {
        // type, eg in cast instructions
        IR_PARAMETER_TYPE,

        // a virtual register
        IR_PARAMETER_REGISTER,

        // a basic block reference for jump instructions
        IR_PARAMETER_BLOCK,

        // a constant numerical value
        IR_PARAMETER_CONSTANT,
    } kind;

    // the value stored
    union {
        IrType type;
        IrRegisterID virtualRegister;
        size_t block;
        IrConstant constant;
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
} IrComparison;

// each instruction inside a basic block
typedef struct IrInstruction {
    // the instruction after this one
    struct IrInstruction* next;

    // the instruction before this one
    struct IrInstruction* prev;

    // flags
    IrComparison comparison : 3;
    bool hasReturn : 1;
    bool hasCondition : 1;

    // what the operation is (todo: more operations, this is just an example)
    enum {
        IR_INS_ALLOCA,
        IR_INS_ADD,
        IR_INS_SUB,
        IR_INS_STORE,
        IR_INS_LOAD,
        IR_INS_GET_ELEMENT_POINTER,
        IR_INS_PHI,
    } opcode;

    // instruction parameters index.  If required, the previous parameter
    // is the return value
    size_t params;

    // number of parameters in the list
    unsigned int parameterCount;
} IrInstruction;

typedef struct IrBasicBlock {

    // first instruction in the block
    size_t instrctions;
    size_t instructionCount;

} IrBasicBlock;

// a function definition
typedef struct IrFunction {

    // return type, must equal types of everything returned from all
    // return instructions in the function
    IrType returnType;

    // the types passed into the function index into parameters array,
    // access the values with a parameter instruction
    size_t parameters;
    size_t parameterCount;

    // first basic block
    size_t blocks;
    size_t blockCount;

} IrFunction;

// any top level declaration
// todo: add struct definitions, etc
typedef struct IrTopLevel {
    enum {
        IR_TOP_LEVEL_GLOBAL,
        IR_TOP_LEVEL_FUNCTION,
    } kind;

    // the identifier to be use
    // could be exported and/or used in a call
    const char* name;
    unsigned int nameLength;

    union {
        IrGlobal global;
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
} IrContext;

void IrContextCreate(IrContext* ctx, MemoryPool* pool);

#endif