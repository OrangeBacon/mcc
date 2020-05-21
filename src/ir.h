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
// global $0 : i32 = 0
//
// function add(i32, i32) : i32 {
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

// identifier for a global variable
typedef struct IrGlobalID {
    // the identifier's globally unique (at the top level) id
    unsigned int id;

    // the global it refers to
    struct IrGlobal* location;
} IrGlobalID;

// identifier for a virtual regiser
typedef struct IrRegisterID {
    // the unique to the function id
    unsigned int id;

    // the instruction that created the register
    struct IrInstruction* location;

    // the type of the value in the register
    IrType type;
} IrRegisterID;

// indentifier for a basic block within a function
typedef struct IrBlockID {

    // unique lable within a function, in a different namespace to virtual
    // registers, they could collide.
    unsigned int id;

    // the basic block the id refers to
    struct IrBasicBlock* location;
} IrBlockID;

// a global variable
typedef struct IrGlobal {

    // the id of the global
    IrGlobalID id;

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
        IrBlockID block;
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
    // the instruction after this one (singly linked list, avoids having to
    // allocate an array which could reduce copying of pointers inside a
    // basic block)
    struct IrInstruction* next;
    struct IrInstruction* prev;

    IrComparison comparison : 3;

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

    // list of parameters.  If required, parameter 0 is the return value
    IrParameter* params;

    // number of parameters in the list
    unsigned int parameterCount;
} IrInstruction;

typedef struct IrBasicBlock {
    // this block's id
    IrBlockID id;

    // first instruction in the block
    IrInstruction* start;

    // last instruction, where to add new instructions to
    IrInstruction* end;
} IrBasicBlock;

// a function definition
typedef struct IrFunction {
    struct IrModule* module;

    // the identifier to be use
    // could be exported and/or used in a call
    const char* name;
    unsigned int nameLength;

    // return type, must equal types of everything returned from all
    // return instructions in the function
    IrType returnType;

    // the types passed into thee function, access the values with a
    // parameter instruction
    ARRAY_DEFINE(IrType, param);

    // linked list of blocks in the function
    IrBasicBlock* start;

    // end of the linked list
    IrBasicBlock* end;
} IrFunction;

// any top level declaration
// todo: add struct definitions, etc
typedef struct IrTopLevel {
    enum {
        IR_TOP_LEVEL_GLOBAL,
        IR_TOP_LEVEL_FUNCTION,
    } kind;

    union {
        IrGlobal global;
        IrFunction function;
    } as;
} IrTopLevel;

// the main struct holding everything in a translation unit
typedef struct IrModule {

    // linked list of top level elements
    IrTopLevel* start;
    IrTopLevel* end;
} IrModule;

#endif