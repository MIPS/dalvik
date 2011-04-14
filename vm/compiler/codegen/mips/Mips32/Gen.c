/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This file contains codegen for the Mips ISA and is intended to be
 * includes by:
 *
 *        Codegen-$(TARGET_ARCH_VARIANT).c
 *
 */

/*
 * Perform a "reg cmp imm" operation and jump to the PCR region if condition
 * satisfies.
 */
static void genNegFloat(CompilationUnit *cUnit, RegLocation rlDest,
                        RegLocation rlSrc)
{
    RegLocation rlResult;
    rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    opRegRegImm(cUnit, kOpAdd, rlResult.lowReg,
                rlSrc.lowReg, 0x80000000);
    storeValue(cUnit, rlDest, rlResult);
}

static void genNegDouble(CompilationUnit *cUnit, RegLocation rlDest,
                         RegLocation rlSrc)
{
    RegLocation rlResult;
    rlSrc = loadValueWide(cUnit, rlSrc, kCoreReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    opRegRegImm(cUnit, kOpAdd, rlResult.highReg, rlSrc.highReg,
                        0x80000000);
    genRegCopy(cUnit, rlResult.lowReg, rlSrc.lowReg);
    storeValueWide(cUnit, rlDest, rlResult);
}

static void genMulLong(CompilationUnit *cUnit, RegLocation rlDest,
                       RegLocation rlSrc1, RegLocation rlSrc2)
{
    RegLocation rlResult;
    loadValueDirectWideFixed(cUnit, rlSrc1, r_ARG0, r_ARG1);
    loadValueDirectWideFixed(cUnit, rlSrc2, r_ARG2, r_ARG3);
    genDispatchToHandler(cUnit, TEMPLATE_MUL_LONG);
    rlResult = dvmCompilerGetReturnWide(cUnit);
    storeValueWide(cUnit, rlDest, rlResult);
}

static bool partialOverlap(int sreg1, int sreg2)
{
    return abs(sreg1 - sreg2) == 1;
}

static void withCarryHelper(CompilationUnit *cUnit, MipsOpCode opc,
                            RegLocation rlDest, RegLocation rlSrc1,
                            RegLocation rlSrc2, int sltuSrc1, int sltuSrc2)
{
    int tReg = dvmCompilerAllocTemp(cUnit);
    newLIR3(cUnit, opc, rlDest.lowReg, rlSrc1.lowReg, rlSrc2.lowReg);
    newLIR3(cUnit, kMipsSltu, tReg, sltuSrc1, sltuSrc2);
    newLIR3(cUnit, opc, rlDest.highReg, rlSrc1.highReg, rlSrc2.highReg);
    newLIR3(cUnit, opc, rlDest.highReg, rlDest.highReg, tReg);
    dvmCompilerFreeTemp(cUnit, tReg);
}

static void genLong3Addr(CompilationUnit *cUnit, MIR *mir, OpKind firstOp,
                         OpKind secondOp, RegLocation rlDest,
                         RegLocation rlSrc1, RegLocation rlSrc2)
{
    RegLocation rlResult;
    int carryOp = (secondOp == kOpAdc || secondOp == kOpSbc);

    if (partialOverlap(rlSrc1.sRegLow,rlSrc2.sRegLow) ||
        partialOverlap(rlSrc1.sRegLow,rlDest.sRegLow) ||
        partialOverlap(rlSrc2.sRegLow,rlDest.sRegLow)) {
        // Rare case - not enough registers to properly handle
        genInterpSingleStep(cUnit, mir);
    } else if (rlDest.sRegLow == rlSrc1.sRegLow) {
        rlResult = loadValueWide(cUnit, rlDest, kCoreReg);
        rlSrc2 = loadValueWide(cUnit, rlSrc2, kCoreReg);
        if (!carryOp) {
            opRegRegReg(cUnit, firstOp, rlResult.lowReg, rlResult.lowReg, rlSrc2.lowReg);
            opRegRegReg(cUnit, secondOp, rlResult.highReg, rlResult.highReg, rlSrc2.highReg);
        } else if (secondOp == kOpAdc) {
            withCarryHelper(cUnit, kMipsAddu, rlResult, rlResult, rlSrc2,
                            rlResult.lowReg, rlSrc2.lowReg);
        } else {
            int tReg = dvmCompilerAllocTemp(cUnit);
            newLIR2(cUnit, kMipsMove, tReg, rlResult.lowReg);
            withCarryHelper(cUnit, kMipsSubu, rlResult, rlResult, rlSrc2,
                            tReg, rlResult.lowReg);
            dvmCompilerFreeTemp(cUnit, tReg);
        }
        storeValueWide(cUnit, rlDest, rlResult);
    } else if (rlDest.sRegLow == rlSrc2.sRegLow) {
        rlResult = loadValueWide(cUnit, rlDest, kCoreReg);
        rlSrc1 = loadValueWide(cUnit, rlSrc1, kCoreReg);
        if (!carryOp) {
            opRegRegReg(cUnit, firstOp, rlResult.lowReg, rlSrc1.lowReg, rlResult.lowReg);
            opRegRegReg(cUnit, secondOp, rlResult.highReg, rlSrc1.highReg, rlResult.highReg);
        } else if (secondOp == kOpAdc) {
            withCarryHelper(cUnit, kMipsAddu, rlResult, rlSrc1, rlResult,
                            rlResult.lowReg, rlSrc1.lowReg);
        } else {
            withCarryHelper(cUnit, kMipsSubu, rlResult, rlSrc1, rlResult,
                            rlSrc1.lowReg, rlResult.lowReg);
        }
        storeValueWide(cUnit, rlDest, rlResult);
    } else {
        rlSrc1 = loadValueWide(cUnit, rlSrc1, kCoreReg);
        rlSrc2 = loadValueWide(cUnit, rlSrc2, kCoreReg);
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
        if (!carryOp) {
            opRegRegReg(cUnit, firstOp, rlResult.lowReg, rlSrc1.lowReg, rlSrc2.lowReg);
            opRegRegReg(cUnit, secondOp, rlResult.highReg, rlSrc1.highReg, rlSrc2.highReg);
        } else if (secondOp == kOpAdc) {
            withCarryHelper(cUnit, kMipsAddu, rlResult, rlSrc1, rlSrc2,
                            rlResult.lowReg, rlSrc1.lowReg);
        } else {
            withCarryHelper(cUnit, kMipsSubu, rlResult, rlSrc1, rlSrc2,
                            rlSrc1.lowReg, rlResult.lowReg);
        }
        storeValueWide(cUnit, rlDest, rlResult);
    }
}

void dvmCompilerInitializeRegAlloc(CompilationUnit *cUnit)
{
    int numTemps = sizeof(coreTemps)/sizeof(int);
    RegisterPool *pool = dvmCompilerNew(sizeof(*pool), true);
    cUnit->regPool = pool;
    pool->numCoreTemps = numTemps;
    pool->coreTemps =
            dvmCompilerNew(numTemps * sizeof(*pool->coreTemps), true);
    pool->numCoreRegs = 0;
    pool->coreRegs = NULL;
    pool->numFPRegs = 0;
    pool->FPRegs = NULL;
    dvmCompilerInitPool(pool->coreTemps, coreTemps, pool->numCoreTemps);
#ifdef __mips_hard_float
    int numFPTemps = sizeof(fpTemps)/sizeof(int);
    pool->numFPTemps = numFPTemps;
    pool->FPTemps =
            dvmCompilerNew(numFPTemps * sizeof(*pool->FPTemps), true);
    dvmCompilerInitPool(pool->FPTemps, fpTemps, pool->numFPTemps);
#else
    pool->numFPTemps = 0;
    pool->FPTemps = NULL;
    dvmCompilerInitPool(pool->FPTemps, NULL, 0);
#endif
    dvmCompilerInitPool(pool->coreRegs, NULL, 0);
    dvmCompilerInitPool(pool->FPRegs, NULL, 0);
    pool->nullCheckedRegs =
        dvmCompilerAllocBitVector(cUnit->numSSARegs, false);
}

/* Export the Dalvik PC assicated with an instruction to the StackSave area */
static MipsLIR *genExportPC(CompilationUnit *cUnit, MIR *mir)
{
    MipsLIR *res;
    int rDPC = dvmCompilerAllocTemp(cUnit);
    int rAddr = dvmCompilerAllocTemp(cUnit);
    int offset = offsetof(StackSaveArea, xtra.currentPc);
    res = loadConstant(cUnit, rDPC, (int) (cUnit->method->insns + mir->offset));
    newLIR3(cUnit, kMipsAddiu, rAddr, rFP, -(sizeof(StackSaveArea) - offset));
    storeWordDisp( cUnit, rAddr, 0, rDPC);
    return res;
}

static void genMonitor(CompilationUnit *cUnit, MIR *mir)
{
    genMonitorPortable(cUnit, mir);
}

static void genCmpLong(CompilationUnit *cUnit, MIR *mir, RegLocation rlDest,
                       RegLocation rlSrc1, RegLocation rlSrc2)
{
    RegLocation rlResult;
    loadValueDirectWideFixed(cUnit, rlSrc1, r_ARG0, r_ARG1);
    loadValueDirectWideFixed(cUnit, rlSrc2, r_ARG2, r_ARG3);
    genDispatchToHandler(cUnit, TEMPLATE_CMP_LONG);
    rlResult = dvmCompilerGetReturn(cUnit);
    storeValue(cUnit, rlDest, rlResult);
}

static bool genInlinedAbsFloat(CompilationUnit *cUnit, MIR *mir)
{
    int offset = offsetof(InterpState, retval);
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    int reg0 = loadValue(cUnit, rlSrc, kCoreReg).lowReg;
#if __mips_isa_rev>=2
    newLIR4(cUnit, kMipsExt, reg0, reg0, 0, 31-1 /* size-1 */);
#else
    newLIR2(cUnit, kMipsSll, reg0, 1);
    newLIR2(cUnit, kMipsSrl, reg0, 1);
#endif
    storeWordDisp(cUnit, rGLUE, offset, reg0);
    //TUNING: rewrite this to not clobber
    dvmCompilerClobber(cUnit, reg0);
    return true;
}

static bool genInlinedAbsDouble(CompilationUnit *cUnit, MIR *mir)
{
    int offset = offsetof(InterpState, retval);
    RegLocation rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    RegLocation regSrc = loadValueWide(cUnit, rlSrc, kCoreReg);
    int reglo = regSrc.lowReg;
    int reghi = regSrc.highReg;
    storeWordDisp(cUnit, rGLUE, offset + LOWORD_OFFSET, reglo);
#if __mips_isa_rev>=2
    newLIR4(cUnit, kMipsExt, reghi, reghi, 0, 31-1 /* size-1 */);
#else
    newLIR2(cUnit, kMipsSll, reghi, 1);
    newLIR2(cUnit, kMipsSrl, reghi, 1);
#endif
    storeWordDisp(cUnit, rGLUE, offset + HIWORD_OFFSET, reghi);
    //TUNING: rewrite this to not clobber
    dvmCompilerClobber(cUnit, reghi);
    return true;
}

/* No select in thumb, so we need to branch.  Thumb2 will do better */
static bool genInlinedMinMaxInt(CompilationUnit *cUnit, MIR *mir, bool isMin)
{
    int offset = offsetof(InterpState, retval);
    RegLocation rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 1);
    int reg0 = loadValue(cUnit, rlSrc1, kCoreReg).lowReg;
    int reg1 = loadValue(cUnit, rlSrc2, kCoreReg).lowReg;
    int tReg = dvmCompilerAllocTemp(cUnit);
    if (isMin) {
       newLIR3(cUnit, kMipsSlt, tReg, reg0, reg1);
    }
    else {
       newLIR3(cUnit, kMipsSlt, tReg, reg1, reg0);
    }
    newLIR3(cUnit, kMipsMovz, reg0, reg1, tReg);
    dvmCompilerFreeTemp(cUnit, tReg);
    newLIR3(cUnit, kMipsSw, reg0, offset, rGLUE);
    //TUNING: rewrite this to not clobber
    dvmCompilerClobber(cUnit,reg0);
    return false;
}

static void genMultiplyByTwoBitMultiplier(CompilationUnit *cUnit,
        RegLocation rlSrc, RegLocation rlResult, int lit,
        int firstBit, int secondBit)
{
    // We can't implement "add src, src, src, lsl#shift" on Thumb, so we have
    // to do a regular multiply.
    opRegRegImm(cUnit, kOpMul, rlResult.lowReg, rlSrc.lowReg, lit);
}
