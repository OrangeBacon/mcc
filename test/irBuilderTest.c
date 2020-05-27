#include "ir.h"

// output:
// global a $0 : i0 = 0

// function add $1(i32, i32) : i32 {
//   | @0:
// 0 |   %0 = parameter 0
// 1 |   %1 = parameter 1
// 2 |   %2 = add %0 %1
// 3 |   %3 = compare greater %2 5
// 4 |   jump if %3 @1 @1
//   | @1:
// 0 |   return $0
//   | @2:
// 0 |   return %2
// }

void testfn(MemoryPool* pool) {
    IrContext c;
    IrContextCreate(&c, pool);

    IrGlobal* globlA = IrGlobalCreate(&c, "a", 1, 0);

    IrParameter* retType = IrParameterCreate(&c);
    IrParameterIntegerType(retType, 32);

    IrParameter* inType = IrParametersCreate(&c, 2);
    IrParameterIntegerType(inType, 32);
    IrParameterIntegerType(inType + 1, 32);

    IrFunction* addfn = IrFunctionCreate(&c, "add", 3, retType, inType, 2);
    IrBasicBlock* block = IrBasicBlockCreate(addfn);

    IrParameter* getParam1 = IrParametersCreate(&c, 2);
    IrParameterNewVReg(addfn, getParam1);
    IrParameterConstant(getParam1 + 1, 0);
    IrInstructionSetCreate(&c, block, IR_INS_PARAMETER, getParam1, 2);

    IrParameter* getParam2 = IrParametersCreate(&c, 2);
    IrParameterNewVReg(addfn, getParam2);
    IrParameterConstant(getParam2 + 1, 1);
    IrInstructionSetCreate(&c, block, IR_INS_PARAMETER, getParam2, 2);

    IrParameter* add = IrParametersCreate(&c, 3);
    IrParameterNewVReg(addfn, add);
    IrParameterVRegRef(add + 1, getParam1);
    IrParameterVRegRef(add + 2, getParam2);
    IrInstructionSetCreate(&c, block, IR_INS_ADD, add, 3);

    IrParameter* cmp = IrParametersCreate(&c, 3);
    IrParameterNewVReg(addfn, cmp);
    IrParameterVRegRef(cmp + 1, add);
    IrParameterConstant(cmp + 2, 5);
    IrInstruction* cmpInst =
        IrInstructionSetCreate(&c, block, IR_INS_COMPARE, cmp, 3);
    IrInstructionCondition(cmpInst, IR_COMPARE_GREATER);

    IrBasicBlock* globlRet = IrBasicBlockCreate(addfn);
    IrParameter* globlRetParam = IrParameterCreate(&c);
    IrParameterGlobal(globlRetParam, globlA);
    IrInstructionVoidCreate(&c, globlRet, IR_INS_RETURN, globlRetParam, 1);

    IrBasicBlock* localRet = IrBasicBlockCreate(addfn);
    IrParameter* localRetParam = IrParameterCreate(&c);
    IrParameterVRegRef(localRetParam, add);
    IrInstructionVoidCreate(&c, localRet, IR_INS_RETURN, localRetParam, 1);

    IrParameter* jump = IrParametersCreate(&c, 3);
    IrParameterVRegRef(jump, cmp);
    IrParameterBlock(jump + 1, globlRet);
    IrParameterBlock(jump + 2, localRet);
    IrInstructionVoidCreate(&c, block, IR_INS_JUMP_IF, jump, 3);

    IrContextPrint(&c);
}