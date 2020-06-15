#include "ir.h"

// output:
// function max $0(i32, i32) : i32 {
//   | @0:
// 0 |   %0 = parameter 0
// 1 |   %1 = parameter 1
// 2 |   %2 = compare greater %0 %1
// 3 |   jump if %2 @1 @2
//   | @1(@0):
// 0 |   jump @3
//   | @2(@0):
// 0 |   jump @3
//   | @3(@1, @2):
//   |   %3 = phi [@1 %0] [@2 %1]
// 0 |   return %3
// }

// int max(int x, int y) {
//     if(x > y) {
//         return x;
//     }
//     return y;
// }

void testfn(MemoryPool* pool) {
    IrContext c;
    IrContextCreate(&c, pool);

    IrParameter* retType = IrParameterCreate(&c);
    IrParameterIntegerType(retType, 32);

    IrParameter* inType = IrParametersCreate(&c, 2);
    IrParameterIntegerType(inType, 32);
    IrParameterIntegerType(inType + 1, 32);

    IrFunction* maxFn = IrFunctionCreate(&c, "max", 3, retType, inType, 2);
    IrBasicBlock* block = IrBasicBlockCreate(maxFn);
    IrBasicBlock* retFirst = IrBasicBlockCreate(maxFn);
    IrBasicBlock* retSecond = IrBasicBlockCreate(maxFn);
    IrBasicBlock* retBlock = IrBasicBlockCreate(maxFn);

    IrParameter* getParam1 = IrParametersCreate(&c, 2);
    IrParameterNewVReg(maxFn, getParam1);
    IrParameterConstant(getParam1 + 1, 0);
    IrInstructionSetCreate(&c, block, IR_INS_PARAMETER, getParam1, 2);

    IrParameter* getParam2 = IrParametersCreate(&c, 2);
    IrParameterNewVReg(maxFn, getParam2);
    IrParameterConstant(getParam2 + 1, 1);
    IrInstructionSetCreate(&c, block, IR_INS_PARAMETER, getParam2, 2);

    IrParameter* cmp = IrParametersCreate(&c, 3);
    IrParameterNewVReg(maxFn, cmp);
    IrParameterVRegRef(cmp + 1, getParam1);
    IrParameterVRegRef(cmp + 2, getParam2);
    IrInstruction* cmpInst =
        IrInstructionSetCreate(&c, block, IR_INS_COMPARE, cmp, 3);
    IrInstructionCondition(cmpInst, IR_COMPARE_GREATER);

    IrParameter* retFirstPred = IrBlockSetPredecessors(retFirst, 1);
    IrParameterBlock(retFirstPred, block);
    IrParameter* retFirstParam = IrParameterCreate(&c);
    IrParameterBlock(retFirstParam, retBlock);
    IrInstructionVoidCreate(&c, retFirst, IR_INS_JUMP, retFirstParam, 1);

    IrParameter* retSecondPred = IrBlockSetPredecessors(retSecond, 1);
    IrParameterBlock(retSecondPred, block);
    IrParameter* retSecondParam = IrParameterCreate(&c);
    IrParameterBlock(retSecondParam, retBlock);
    IrInstructionVoidCreate(&c, retSecond, IR_INS_JUMP, retSecondParam, 1);

    IrParameter* jump = IrParametersCreate(&c, 3);
    IrParameterVRegRef(jump, cmp);
    IrParameterBlock(jump + 1, retFirst);
    IrParameterBlock(jump + 2, retSecond);
    IrInstructionVoidCreate(&c, block, IR_INS_JUMP_IF, jump, 3);

    IrParameter* retBlockPred = IrBlockSetPredecessors(retBlock, 2);
    IrParameterBlock(retBlockPred, retFirst);
    IrParameterBlock(retBlockPred + 1, retSecond);
    IrPhi* phi = IrPhiCreate(&c, retBlock, 2);
    IrParameterBlock(phi->blocks, retFirst);
    IrParameterVRegRef(phi->params, getParam1);
    IrParameterBlock(phi->blocks + 1, retSecond);
    IrParameterVRegRef(phi->params + 1, getParam2);

    IrParameter* retParams = IrParametersCreate(&c, 1);
    IrParameterVRegRef(retParams, &phi->result);
    IrInstructionVoidCreate(&c, retBlock, IR_INS_RETURN, retParams, 1);

    IrContextPrint(&c);
}