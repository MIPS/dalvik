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
 * This file contains codegen and support common to all supported
 * Mips variants.  It is included by:
 *
 *        Codegen-$(TARGET_ARCH_VARIANT).c
 *
 * which combines this common code with specific support found in the
 * applicable directory below this one.
 */

static bool genConversionCall(CompilationUnit *cUnit, MIR *mir, void *funct,
                                     int srcSize, int tgtSize)
{
    /*
     * Don't optimize the register usage since it calls out to template
     * functions
     */
    RegLocation rlSrc;
    RegLocation rlDest;
    dvmCompilerFlushAllRegs(cUnit);   /* Send everything to home location */
    if (srcSize == 1) {
        rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
        loadValueDirectFixed(cUnit, rlSrc, r_A0);
    } else {
        rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
        loadValueDirectWideFixed(cUnit, rlSrc, r_A0, r_A1);
    }
    LOAD_FUNC_ADDR(cUnit, r_T9, (int)funct);
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    dvmCompilerClobberCallRegs(cUnit);
    if (tgtSize == 1) {
        RegLocation rlResult;
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
        rlResult = dvmCompilerGetReturn(cUnit);
        storeValue(cUnit, rlDest, rlResult);
    } else {
        RegLocation rlResult;
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
        rlResult = dvmCompilerGetReturnWide(cUnit);
        storeValueWide(cUnit, rlDest, rlResult);
    }
    return false;
}


static bool genArithOpFloatPortable(CompilationUnit *cUnit, MIR *mir,
                                    RegLocation rlDest, RegLocation rlSrc1,
                                    RegLocation rlSrc2)
{
    RegLocation rlResult;
    void* funct;

    switch (mir->dalvikInsn.opCode) {
        case OP_ADD_FLOAT_2ADDR:
        case OP_ADD_FLOAT:
            funct = (void*) __addsf3;
            break;
        case OP_SUB_FLOAT_2ADDR:
        case OP_SUB_FLOAT:
            funct = (void*) __subsf3;
            break;
        case OP_DIV_FLOAT_2ADDR:
        case OP_DIV_FLOAT:
            funct = (void*) __divsf3;
            break;
        case OP_MUL_FLOAT_2ADDR:
        case OP_MUL_FLOAT:
            funct = (void*) __mulsf3;
            break;
        case OP_REM_FLOAT_2ADDR:
        case OP_REM_FLOAT:
            funct = (void*) fmodf;
            break;
        case OP_NEG_FLOAT: {
            genNegFloat(cUnit, rlDest, rlSrc1);
            return false;
        }
        default:
            return true;
    }

    dvmCompilerFlushAllRegs(cUnit);   /* Send everything to home location */
    loadValueDirectFixed(cUnit, rlSrc1, r_A0);
    loadValueDirectFixed(cUnit, rlSrc2, r_A1);
    LOAD_FUNC_ADDR(cUnit, r_T9, (int)funct);
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    dvmCompilerClobberCallRegs(cUnit);
    rlResult = dvmCompilerGetReturn(cUnit);
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool genArithOpDoublePortable(CompilationUnit *cUnit, MIR *mir,
                                     RegLocation rlDest, RegLocation rlSrc1,
                                     RegLocation rlSrc2)
{
    RegLocation rlResult;
    void* funct;

    switch (mir->dalvikInsn.opCode) {
        case OP_ADD_DOUBLE_2ADDR:
        case OP_ADD_DOUBLE:
            funct = (void*) __adddf3;
            break;
        case OP_SUB_DOUBLE_2ADDR:
        case OP_SUB_DOUBLE:
            funct = (void*) __subdf3;
            break;
        case OP_DIV_DOUBLE_2ADDR:
        case OP_DIV_DOUBLE:
            funct = (void*) __divsf3;
            break;
        case OP_MUL_DOUBLE_2ADDR:
        case OP_MUL_DOUBLE:
            funct = (void*) __muldf3;
            break;
        case OP_REM_DOUBLE_2ADDR:
        case OP_REM_DOUBLE:
            funct = (void*) fmod;
            break;
        case OP_NEG_DOUBLE: {
            genNegDouble(cUnit, rlDest, rlSrc1);
            return false;
        }
        default:
            return true;
    }
    dvmCompilerFlushAllRegs(cUnit);   /* Send everything to home location */
    LOAD_FUNC_ADDR(cUnit, r_T9, (int)funct);
    loadValueDirectWideFixed(cUnit, rlSrc1, r_A0, r_A1);
    loadValueDirectWideFixed(cUnit, rlSrc2, r_A2, r_A3);
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    dvmCompilerClobberCallRegs(cUnit);
    rlResult = dvmCompilerGetReturnWide(cUnit);
    storeValueWide(cUnit, rlDest, rlResult);
    return false;
}

static bool genConversionPortable(CompilationUnit *cUnit, MIR *mir)
{
    OpCode opCode = mir->dalvikInsn.opCode;

    switch (opCode) {
        case OP_INT_TO_FLOAT:
            return genConversionCall(cUnit, mir, (void*)__floatsisf, 1, 1);
        case OP_FLOAT_TO_INT:
            return genConversionCall(cUnit, mir, (void*)__fixsfsi, 1, 1);
        case OP_DOUBLE_TO_FLOAT:
            return genConversionCall(cUnit, mir, (void*)__truncdfsf2, 2, 1);
        case OP_FLOAT_TO_DOUBLE:
            return genConversionCall(cUnit, mir, (void*)__extendsfdf2, 1, 2);
        case OP_INT_TO_DOUBLE:
            return genConversionCall(cUnit, mir, (void*)__floatsidf, 1, 2);
        case OP_DOUBLE_TO_INT:
            return genConversionCall(cUnit, mir, (void*)__fixdfsi, 2, 1);
        case OP_FLOAT_TO_LONG:
            return genConversionCall(cUnit, mir, (void*)dvmJitf2l, 1, 2);
        case OP_LONG_TO_FLOAT:
            return genConversionCall(cUnit, mir, (void*)__floatdisf, 2, 1);
        case OP_DOUBLE_TO_LONG:
            return genConversionCall(cUnit, mir, (void*)dvmJitd2l, 2, 2);
        case OP_LONG_TO_DOUBLE:
            return genConversionCall(cUnit, mir, (void*)__floatdidf, 2, 2);
        default:
            return true;
    }
    return false;
}

#if defined(WITH_SELF_VERIFICATION)
static void selfVerificationBranchInsert(LIR *currentLIR, MipsOpCode opCode,
                          int dest, int src1)
{
assert(0); /* MIPSTODO port selfVerificationBranchInsert() */
     MipsLIR *insn = dvmCompilerNew(sizeof(MipsLIR), true);
     insn->opCode = opCode;
     insn->operands[0] = dest;
     insn->operands[1] = src1;
     setupResourceMasks(insn);
     dvmCompilerInsertLIRBefore(currentLIR, (LIR *) insn);
}

static void selfVerificationBranchInsertPass(CompilationUnit *cUnit)
{
assert(0); /* MIPSTODO port selfVerificationBranchInsertPass() */
    MipsLIR *thisLIR;
    MipsLIR *branchLIR = dvmCompilerNew(sizeof(MipsLIR), true);
    TemplateOpCode opCode = TEMPLATE_MEM_OP_DECODE;

    for (thisLIR = (MipsLIR *) cUnit->firstLIRInsn;
         thisLIR != (MipsLIR *) cUnit->lastLIRInsn;
         thisLIR = NEXT_LIR(thisLIR)) {
        if (thisLIR->branchInsertSV) {
            /* Branch to mem op decode template */
            selfVerificationBranchInsert((LIR *) thisLIR, kThumbBlx1,
                       (int) gDvmJit.codeCache + templateEntryOffsets[opCode],
                       (int) gDvmJit.codeCache + templateEntryOffsets[opCode]);
            selfVerificationBranchInsert((LIR *) thisLIR, kThumbBlx2,
                       (int) gDvmJit.codeCache + templateEntryOffsets[opCode],
                       (int) gDvmJit.codeCache + templateEntryOffsets[opCode]);
        }
    }
}
#endif

/* Generate conditional branch instructions */
static MipsLIR *genConditionalBranchMips(CompilationUnit *cUnit,
                                    MipsOpCode opc, int rs, int rt,
                                    MipsLIR *target)
{
    MipsLIR *branch = opCondBranchMips(cUnit, opc, rs, rt);
    branch->generic.target = (LIR *) target;
    return branch;
}

/* Generate a unconditional branch to go to the interpreter */
static inline MipsLIR *genTrap(CompilationUnit *cUnit, int dOffset,
                                  MipsLIR *pcrLabel)
{
    MipsLIR *branch = opNone(cUnit, kOpUncondBr);
    return genCheckCommon(cUnit, dOffset, branch, pcrLabel);
}

/* Load a wide field from an object instance */
static void genIGetWide(CompilationUnit *cUnit, MIR *mir, int fieldOffset)
{
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    RegLocation rlResult;
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    int regPtr = dvmCompilerAllocTemp(cUnit);

    assert(rlDest.wide);

    genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg, mir->offset,
                 NULL);/* null object? */
    opRegRegImm(cUnit, kOpAdd, regPtr, rlObj.lowReg, fieldOffset);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);

    HEAP_ACCESS_SHADOW(true);
    loadPair(cUnit, regPtr, rlResult.lowReg, rlResult.highReg);
    HEAP_ACCESS_SHADOW(false);

    dvmCompilerFreeTemp(cUnit, regPtr);
    storeValueWide(cUnit, rlDest, rlResult);
}

/* Store a wide field to an object instance */
static void genIPutWide(CompilationUnit *cUnit, MIR *mir, int fieldOffset)
{
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    RegLocation rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 2);
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    int regPtr;
    rlSrc = loadValueWide(cUnit, rlSrc, kAnyReg);
    genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg, mir->offset,
                 NULL);/* null object? */
    regPtr = dvmCompilerAllocTemp(cUnit);
    opRegRegImm(cUnit, kOpAdd, regPtr, rlObj.lowReg, fieldOffset);

    HEAP_ACCESS_SHADOW(true);
    storePair(cUnit, regPtr, rlSrc.lowReg, rlSrc.highReg);
    HEAP_ACCESS_SHADOW(false);

    dvmCompilerFreeTemp(cUnit, regPtr);
}

/*
 * Load a field from an object instance
 *
 */
static void genIGet(CompilationUnit *cUnit, MIR *mir, OpSize size,
                    int fieldOffset)
{
    int regPtr;
    RegLocation rlResult;
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);
    genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg, mir->offset,
                 NULL);/* null object? */

    HEAP_ACCESS_SHADOW(true);
    loadBaseDisp(cUnit, mir, rlObj.lowReg, fieldOffset, rlResult.lowReg,
                 size, rlObj.sRegLow);
    HEAP_ACCESS_SHADOW(false);

    storeValue(cUnit, rlDest, rlResult);
}

/*
 * Store a field to an object instance
 *
 */
static void genIPut(CompilationUnit *cUnit, MIR *mir, OpSize size,
                    int fieldOffset)
{
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 1);
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    rlSrc = loadValue(cUnit, rlSrc, kAnyReg);
    int regPtr;
    genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg, mir->offset,
                 NULL);/* null object? */

    HEAP_ACCESS_SHADOW(true);
    storeBaseDisp(cUnit, rlObj.lowReg, fieldOffset, rlSrc.lowReg, size);
    HEAP_ACCESS_SHADOW(false);
}


/*
 * Generate array load
 */
static void genArrayGet(CompilationUnit *cUnit, MIR *mir, OpSize size,
                        RegLocation rlArray, RegLocation rlIndex,
                        RegLocation rlDest, int scale)
{
    int lenOffset = offsetof(ArrayObject, length);
    int dataOffset = offsetof(ArrayObject, contents);
    RegLocation rlResult;
    rlArray = loadValue(cUnit, rlArray, kCoreReg);
    rlIndex = loadValue(cUnit, rlIndex, kCoreReg);
    int regPtr;

    /* null object? */
    MipsLIR * pcrLabel = NULL;

    if (!(mir->OptimizationFlags & MIR_IGNORE_NULL_CHECK)) {
        pcrLabel = genNullCheck(cUnit, rlArray.sRegLow,
                                rlArray.lowReg, mir->offset, NULL);
    }

    regPtr = dvmCompilerAllocTemp(cUnit);

    if (!(mir->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        int regLen = dvmCompilerAllocTemp(cUnit);
        /* Get len */
        loadWordDisp(cUnit, rlArray.lowReg, lenOffset, regLen);
        /* regPtr -> array data */
        opRegRegImm(cUnit, kOpAdd, regPtr, rlArray.lowReg, dataOffset);
        genBoundsCheck(cUnit, rlIndex.lowReg, regLen, mir->offset,
                       pcrLabel);
        dvmCompilerFreeTemp(cUnit, regLen);
    } else {
        /* regPtr -> array data */
        opRegRegImm(cUnit, kOpAdd, regPtr, rlArray.lowReg, dataOffset);
    }
    if ((size == kLong) || (size == kDouble)) {
        if (scale) {
            int rNewIndex = dvmCompilerAllocTemp(cUnit);
            opRegRegImm(cUnit, kOpLsl, rNewIndex, rlIndex.lowReg, scale);
            opRegReg(cUnit, kOpAdd, regPtr, rNewIndex);
            dvmCompilerFreeTemp(cUnit, rNewIndex);
        } else {
            opRegReg(cUnit, kOpAdd, regPtr, rlIndex.lowReg);
        }
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);

        HEAP_ACCESS_SHADOW(true);
        loadPair(cUnit, regPtr, rlResult.lowReg, rlResult.highReg);
        HEAP_ACCESS_SHADOW(false);

        dvmCompilerFreeTemp(cUnit, regPtr);
        storeValueWide(cUnit, rlDest, rlResult);
    } else {
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);

        HEAP_ACCESS_SHADOW(true);
        loadBaseIndexed(cUnit, regPtr, rlIndex.lowReg, rlResult.lowReg,
                        scale, size);
        HEAP_ACCESS_SHADOW(false);

        dvmCompilerFreeTemp(cUnit, regPtr);
        storeValue(cUnit, rlDest, rlResult);
    }
}

/*
 * Generate array store
 *
 */
static void genArrayPut(CompilationUnit *cUnit, MIR *mir, OpSize size,
                        RegLocation rlArray, RegLocation rlIndex,
                        RegLocation rlSrc, int scale)
{
    int lenOffset = offsetof(ArrayObject, length);
    int dataOffset = offsetof(ArrayObject, contents);

    int regPtr;
    rlArray = loadValue(cUnit, rlArray, kCoreReg);
    rlIndex = loadValue(cUnit, rlIndex, kCoreReg);

    if (dvmCompilerIsTemp(cUnit, rlArray.lowReg)) {
        dvmCompilerClobber(cUnit, rlArray.lowReg);
        regPtr = rlArray.lowReg;
    } else {
        regPtr = dvmCompilerAllocTemp(cUnit);
        genRegCopy(cUnit, regPtr, rlArray.lowReg);
    }

    /* null object? */
    MipsLIR * pcrLabel = NULL;

    if (!(mir->OptimizationFlags & MIR_IGNORE_NULL_CHECK)) {
        pcrLabel = genNullCheck(cUnit, rlArray.sRegLow, rlArray.lowReg,
                                mir->offset, NULL);
    }

    if (!(mir->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        int regLen = dvmCompilerAllocTemp(cUnit);
        //NOTE: max live temps(4) here.
        /* Get len */
        loadWordDisp(cUnit, rlArray.lowReg, lenOffset, regLen);
        /* regPtr -> array data */
        opRegImm(cUnit, kOpAdd, regPtr, dataOffset);
        genBoundsCheck(cUnit, rlIndex.lowReg, regLen, mir->offset,
                       pcrLabel);
        dvmCompilerFreeTemp(cUnit, regLen);
    } else {
        /* regPtr -> array data */
        opRegImm(cUnit, kOpAdd, regPtr, dataOffset);
    }
    /* at this point, regPtr points to array, 2 live temps */
    if ((size == kLong) || (size == kDouble)) {
        //TODO: need specific wide routine that can handle fp regs
        if (scale) {
            int rNewIndex = dvmCompilerAllocTemp(cUnit);
            opRegRegImm(cUnit, kOpLsl, rNewIndex, rlIndex.lowReg, scale);
            opRegReg(cUnit, kOpAdd, regPtr, rNewIndex);
            dvmCompilerFreeTemp(cUnit, rNewIndex);
        } else {
            opRegReg(cUnit, kOpAdd, regPtr, rlIndex.lowReg);
        }
        rlSrc = loadValueWide(cUnit, rlSrc, kAnyReg);

        HEAP_ACCESS_SHADOW(true);
        storePair(cUnit, regPtr, rlSrc.lowReg, rlSrc.highReg);
        HEAP_ACCESS_SHADOW(false);

        dvmCompilerFreeTemp(cUnit, regPtr);
    } else {
        rlSrc = loadValue(cUnit, rlSrc, kAnyReg);

        HEAP_ACCESS_SHADOW(true);
        storeBaseIndexed(cUnit, regPtr, rlIndex.lowReg, rlSrc.lowReg,
                         scale, size);
        HEAP_ACCESS_SHADOW(false);
    }
}

/*
 * Generate array object store
 * Must use explicit register allocation here because of
 * call-out to dvmCanPutArrayElement
 */
static void genArrayObjectPut(CompilationUnit *cUnit, MIR *mir,
                              RegLocation rlArray, RegLocation rlIndex,
                              RegLocation rlSrc, int scale)
{
    int lenOffset = offsetof(ArrayObject, length);
    int dataOffset = offsetof(ArrayObject, contents);

    int regLen = r_A0;
    int regPtr = r_S0;  /* Preserved across call */
    int regArray = r_A1;
    int regIndex = r_S4;  /* Preserved across call */

    dvmCompilerFlushAllRegs(cUnit);
    // moved lock for r_S0 and r_S4 here from below since genBoundsCheck 
    // allocates a temporary that can result in clobbering either of them 
    dvmCompilerLockTemp(cUnit, regPtr);   // r_S0
    dvmCompilerLockTemp(cUnit, regIndex); // r_S4

    loadValueDirectFixed(cUnit, rlArray, regArray);
    loadValueDirectFixed(cUnit, rlIndex, regIndex);

    /* null object? */
    MipsLIR * pcrLabel = NULL;

    if (!(mir->OptimizationFlags & MIR_IGNORE_NULL_CHECK)) {
        pcrLabel = genNullCheck(cUnit, rlArray.sRegLow, regArray,
                                mir->offset, NULL);
    }

    if (!(mir->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        /* Get len */
        loadWordDisp(cUnit, regArray, lenOffset, regLen);
        /* regPtr -> array data */
        opRegRegImm(cUnit, kOpAdd, regPtr, regArray, dataOffset);
        genBoundsCheck(cUnit, regIndex, regLen, mir->offset,
                       pcrLabel);
    } else {
        /* regPtr -> array data */
        opRegRegImm(cUnit, kOpAdd, regPtr, regArray, dataOffset);
    }

    /* Get object to store */
    loadValueDirectFixed(cUnit, rlSrc, r_A0);
    LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmCanPutArrayElement);

    /* Are we storing null?  If so, avoid check */
    MipsLIR *branchOver = opCondBranchMips(cUnit, kMipsBeqz, r_A0, -1);

    /* Make sure the types are compatible */
    loadWordDisp(cUnit, regArray, offsetof(Object, clazz), r_A1);
    loadWordDisp(cUnit, r_A0, offsetof(Object, clazz), r_A0);
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    dvmCompilerClobberCallRegs(cUnit);

    /*
     * Using fixed registers here, and counting on r_S0 and r_S4 being
     * preserved across the above call.  Tell the register allocation
     * utilities about the regs we are using directly
     */
    dvmCompilerLockTemp(cUnit, r_A0);

    /* Bad? - roll back and re-execute if so */
    genRegImmCheck(cUnit, kMipsCondEq, r_V0, 0, mir->offset, pcrLabel);

    /* Resume here - must reload element, regPtr & index preserved */
    loadValueDirectFixed(cUnit, rlSrc, r_A0);

    MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
    target->defMask = ENCODE_ALL;
    branchOver->generic.target = (LIR *) target;

    HEAP_ACCESS_SHADOW(true);
    storeBaseIndexed(cUnit, regPtr, regIndex, r_A0,
                     scale, kWord);
    HEAP_ACCESS_SHADOW(false);
}

static bool genShiftOpLong(CompilationUnit *cUnit, MIR *mir,
                           RegLocation rlDest, RegLocation rlSrc1,
                           RegLocation rlShift)
{
    /*
     * Don't mess with the regsiters here as there is a particular calling
     * convention to the out-of-line handler.
     */
    RegLocation rlResult;

    loadValueDirectWideFixed(cUnit, rlSrc1, r_A0, r_A1);
    loadValueDirect(cUnit, rlShift, r_A2);
    switch( mir->dalvikInsn.opCode) {
        case OP_SHL_LONG:
        case OP_SHL_LONG_2ADDR:
            genDispatchToHandler(cUnit, TEMPLATE_SHL_LONG);
            break;
        case OP_SHR_LONG:
        case OP_SHR_LONG_2ADDR:
            genDispatchToHandler(cUnit, TEMPLATE_SHR_LONG);
            break;
        case OP_USHR_LONG:
        case OP_USHR_LONG_2ADDR:
            genDispatchToHandler(cUnit, TEMPLATE_USHR_LONG);
            break;
        default:
            return true;
    }
    rlResult = dvmCompilerGetReturnWide(cUnit);
    storeValueWide(cUnit, rlDest, rlResult);
    return false;
}

static bool genArithOpLong(CompilationUnit *cUnit, MIR *mir,
                           RegLocation rlDest, RegLocation rlSrc1,
                           RegLocation rlSrc2)
{
    RegLocation rlResult;
    OpKind firstOp = kOpBkpt;
    OpKind secondOp = kOpBkpt;
    bool callOut = false;
    void *callTgt;

    switch (mir->dalvikInsn.opCode) {
        case OP_NOT_LONG:
            rlSrc2 = loadValueWide(cUnit, rlSrc2, kCoreReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegReg(cUnit, kOpMvn, rlResult.lowReg, rlSrc2.lowReg);
            opRegReg(cUnit, kOpMvn, rlResult.highReg, rlSrc2.highReg);
            storeValueWide(cUnit, rlDest, rlResult);
            return false;
            break;
        case OP_ADD_LONG:
        case OP_ADD_LONG_2ADDR:
            firstOp = kOpAdd;
            secondOp = kOpAdc;
            break;
        case OP_SUB_LONG:
        case OP_SUB_LONG_2ADDR:
            firstOp = kOpSub;
            secondOp = kOpSbc;
            break;
        case OP_MUL_LONG:
        case OP_MUL_LONG_2ADDR:
            genMulLong(cUnit, rlDest, rlSrc1, rlSrc2);
            return false;
        case OP_DIV_LONG:
        case OP_DIV_LONG_2ADDR:
            callOut = true;
            callTgt = (void*)__divdi3;
            break;
        case OP_REM_LONG:
        case OP_REM_LONG_2ADDR:
            callOut = true;
            callTgt = (void*)__moddi3;
            break;
        case OP_AND_LONG_2ADDR:
        case OP_AND_LONG:
            firstOp = kOpAnd;
            secondOp = kOpAnd;
            break;
        case OP_OR_LONG:
        case OP_OR_LONG_2ADDR:
            firstOp = kOpOr;
            secondOp = kOpOr;
            break;
        case OP_XOR_LONG:
        case OP_XOR_LONG_2ADDR:
            firstOp = kOpXor;
            secondOp = kOpXor;
            break;
        case OP_NEG_LONG: {
            int tReg = dvmCompilerAllocTemp(cUnit);
            rlSrc2 = loadValueWide(cUnit, rlSrc2, kCoreReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            newLIR3(cUnit, kMipsSubu, rlResult.lowReg, r_ZERO, rlSrc2.lowReg);
            newLIR3(cUnit, kMipsSubu, tReg, r_ZERO, rlSrc2.highReg);
            newLIR3(cUnit, kMipsSltu, rlResult.highReg, r_ZERO, rlResult.lowReg);
            newLIR3(cUnit, kMipsSubu, rlResult.highReg, tReg, rlResult.highReg);
            dvmCompilerFreeTemp(cUnit, tReg);
            storeValueWide(cUnit, rlDest, rlResult);
            return false;
            break;
        }
        default:
            LOGE("Invalid long arith op");
            dvmCompilerAbort(cUnit);
    }
    if (!callOut) {
        genLong3Addr(cUnit, mir, firstOp, secondOp, rlDest, rlSrc1, rlSrc2);
    } else {
        dvmCompilerFlushAllRegs(cUnit);   /* Send everything to home location */
        loadValueDirectWideFixed(cUnit, rlSrc1, r_A0, r_A1);
        LOAD_FUNC_ADDR(cUnit, r_T9, (int) callTgt);
        loadValueDirectWideFixed(cUnit, rlSrc2, r_A2, r_A3);
        opReg(cUnit, kOpBlx, r_T9);
        newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
        dvmCompilerClobberCallRegs(cUnit);
        rlResult = dvmCompilerGetReturnWide(cUnit);
        storeValueWide(cUnit, rlDest, rlResult);
    }
    return false;
}

static bool genArithOpInt(CompilationUnit *cUnit, MIR *mir,
                          RegLocation rlDest, RegLocation rlSrc1,
                          RegLocation rlSrc2)
{
    OpKind op = kOpBkpt;
    bool callOut = false;
    bool checkZero = false;
    bool unary = false;
    int retReg = r_A0;
    void *callTgt;
    RegLocation rlResult;
    bool shiftOp = false;
    int isDivRem = false;
    MipsOpCode opc;
    int divReg;

    switch (mir->dalvikInsn.opCode) {
        case OP_NEG_INT:
            op = kOpNeg;
            unary = true;
            break;
        case OP_NOT_INT:
            op = kOpMvn;
            unary = true;
            break;
        case OP_ADD_INT:
        case OP_ADD_INT_2ADDR:
            op = kOpAdd;
            break;
        case OP_SUB_INT:
        case OP_SUB_INT_2ADDR:
            op = kOpSub;
            break;
        case OP_MUL_INT:
        case OP_MUL_INT_2ADDR:
            op = kOpMul;
            break;
        case OP_DIV_INT:
        case OP_DIV_INT_2ADDR:
            isDivRem = true;
            checkZero = true;
            opc = kMipsMflo;
            divReg = r_LO;
            break;
        case OP_REM_INT:
        case OP_REM_INT_2ADDR:
            isDivRem = true;
            checkZero = true;
            opc = kMipsMfhi;
            divReg = r_HI;
            break;
        case OP_AND_INT:
        case OP_AND_INT_2ADDR:
            op = kOpAnd;
            break;
        case OP_OR_INT:
        case OP_OR_INT_2ADDR:
            op = kOpOr;
            break;
        case OP_XOR_INT:
        case OP_XOR_INT_2ADDR:
            op = kOpXor;
            break;
        case OP_SHL_INT:
        case OP_SHL_INT_2ADDR:
            shiftOp = true;
            op = kOpLsl;
            break;
        case OP_SHR_INT:
        case OP_SHR_INT_2ADDR:
            shiftOp = true;
            op = kOpAsr;
            break;
        case OP_USHR_INT:
        case OP_USHR_INT_2ADDR:
            shiftOp = true;
            op = kOpLsr;
            break;
        default:
            LOGE("Invalid word arith op: 0x%x(%d)",
                 mir->dalvikInsn.opCode, mir->dalvikInsn.opCode);
            dvmCompilerAbort(cUnit);
    }

    rlSrc1 = loadValue(cUnit, rlSrc1, kCoreReg);
    if (unary) {
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
        opRegReg(cUnit, op, rlResult.lowReg,
                 rlSrc1.lowReg);
    } else if (isDivRem) {
        rlSrc2 = loadValue(cUnit, rlSrc2, kCoreReg);
        if (checkZero) {
            genNullCheck(cUnit, rlSrc2.sRegLow, rlSrc2.lowReg, mir->offset, NULL);
        }
        newLIR4(cUnit, kMipsDiv, r_HI, r_LO, rlSrc1.lowReg, rlSrc2.lowReg);
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
        newLIR2(cUnit, opc, rlResult.lowReg, divReg);
    } else {
        rlSrc2 = loadValue(cUnit, rlSrc2, kCoreReg);
        if (shiftOp) {
            int tReg = dvmCompilerAllocTemp(cUnit);
            opRegRegImm(cUnit, kOpAnd, tReg, rlSrc2.lowReg, 31);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegRegReg(cUnit, op, rlResult.lowReg,
                        rlSrc1.lowReg, tReg);
            dvmCompilerFreeTemp(cUnit, tReg);
        } else {
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegRegReg(cUnit, op, rlResult.lowReg,
                        rlSrc1.lowReg, rlSrc2.lowReg);
        }
    }
    storeValue(cUnit, rlDest, rlResult);

    return false;
}

static bool genArithOp(CompilationUnit *cUnit, MIR *mir)
{
    OpCode opCode = mir->dalvikInsn.opCode;
    RegLocation rlDest;
    RegLocation rlSrc1;
    RegLocation rlSrc2;
    /* Deduce sizes of operands */
    if (mir->ssaRep->numUses == 2) {
        rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 0);
        rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 1);
    } else if (mir->ssaRep->numUses == 3) {
        rlSrc1 = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
        rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 2);
    } else {
        rlSrc1 = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
        rlSrc2 = dvmCompilerGetSrcWide(cUnit, mir, 2, 3);
        assert(mir->ssaRep->numUses == 4);
    }
    if (mir->ssaRep->numDefs == 1) {
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    } else {
        assert(mir->ssaRep->numDefs == 2);
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    }

    if ((opCode >= OP_ADD_LONG_2ADDR) && (opCode <= OP_XOR_LONG_2ADDR)) {
        return genArithOpLong(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opCode >= OP_ADD_LONG) && (opCode <= OP_XOR_LONG)) {
        return genArithOpLong(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opCode >= OP_SHL_LONG_2ADDR) && (opCode <= OP_USHR_LONG_2ADDR)) {
        return genShiftOpLong(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opCode >= OP_SHL_LONG) && (opCode <= OP_USHR_LONG)) {
        return genShiftOpLong(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opCode >= OP_ADD_INT_2ADDR) && (opCode <= OP_USHR_INT_2ADDR)) {
        return genArithOpInt(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opCode >= OP_ADD_INT) && (opCode <= OP_USHR_INT)) {
        return genArithOpInt(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opCode >= OP_ADD_FLOAT_2ADDR) && (opCode <= OP_REM_FLOAT_2ADDR)) {
        return genArithOpFloat(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opCode >= OP_ADD_FLOAT) && (opCode <= OP_REM_FLOAT)) {
        return genArithOpFloat(cUnit, mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opCode >= OP_ADD_DOUBLE_2ADDR) && (opCode <= OP_REM_DOUBLE_2ADDR)) {
        return genArithOpDouble(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    if ((opCode >= OP_ADD_DOUBLE) && (opCode <= OP_REM_DOUBLE)) {
        return genArithOpDouble(cUnit,mir, rlDest, rlSrc1, rlSrc2);
    }
    return true;
}

/* Generate unconditional branch instructions */
static MipsLIR *genUnconditionalBranch(CompilationUnit *cUnit, MipsLIR *target)
{
    MipsLIR *branch = opNone(cUnit, kOpUncondBr);
    branch->generic.target = (LIR *) target;
    return branch;
}

/* Perform the actual operation for OP_RETURN_* */
void genReturnCommon(CompilationUnit *cUnit, MIR *mir) 
{
    genDispatchToHandler(cUnit, TEMPLATE_RETURN);
#if defined(JIT_STATS)
    gDvmJit.returnOp++;
#endif
    int dPC = (int) (cUnit->method->insns + mir->offset);
    /* Insert branch, but defer setting of target */
    MipsLIR *branch = genUnconditionalBranch(cUnit, NULL);
    /* Set up the place holder to reconstruct this Dalvik PC */
    MipsLIR *pcrLabel = dvmCompilerNew(sizeof(MipsLIR), true);
    pcrLabel->opCode = kMipsPseudoPCReconstructionCell;
    pcrLabel->operands[0] = dPC;
    pcrLabel->operands[1] = mir->offset;
    /* Insert the place holder to the growable list */
    dvmInsertGrowableList(&cUnit->pcReconstructionList, pcrLabel);
    /* Branch to the PC reconstruction code */
    branch->generic.target = (LIR *) pcrLabel;
}

static void genProcessArgsNoRange(CompilationUnit *cUnit, MIR *mir,
                                  DecodedInstruction *dInsn,
                                  MipsLIR **pcrLabel)
{
    unsigned int i;
    unsigned int regMask = 0;
    RegLocation rlArg;
    int numDone = 0;

    /*
     * Load arguments to r_A0..r_T0.  Note that these registers may contain
     * live values, so we clobber them immediately after loading to prevent
     * them from being used as sources for subsequent loads.
     */
    dvmCompilerLockAllTemps(cUnit);
    for (i = 0; i < dInsn->vA; i++) {
        regMask |= 1 << i;
        rlArg = dvmCompilerGetSrc(cUnit, mir, numDone++);
        loadValueDirectFixed(cUnit, rlArg, i+r_A0); /* r_A0 thru r_T0 */
    }
    if (regMask) {
        /* Up to 5 args are pushed on top of FP - sizeofStackSaveArea */
        opRegRegImm(cUnit, kOpSub, r_S4, rFP,
                    sizeof(StackSaveArea) + (dInsn->vA << 2));
        /* generate null check */
        if (pcrLabel) {
            *pcrLabel = genNullCheck(cUnit, dvmCompilerSSASrc(mir, 0), r_A0,
                                     mir->offset, NULL);
        }
        storeMultiple(cUnit, r_S4, regMask);
    }
}

static void genProcessArgsRange(CompilationUnit *cUnit, MIR *mir,
                                DecodedInstruction *dInsn,
                                MipsLIR **pcrLabel)
{
    int srcOffset = dInsn->vC << 2;
    int numArgs = dInsn->vA;
    int regMask;

    /*
     * Note: here, all promoted registers will have been flushed
     * back to the Dalvik base locations, so register usage restrictins
     * are lifted.  All parms loaded from original Dalvik register
     * region - even though some might conceivably have valid copies
     * cached in a preserved register.
     */
    dvmCompilerLockAllTemps(cUnit);

    /*
     * r4PC     : &rFP[vC]
     * r_S4: &newFP[0]
     */
    opRegRegImm(cUnit, kOpAdd, r4PC, rFP, srcOffset);
    /* load [r_A0 up to r_A3)] */
    regMask = (1 << ((numArgs < 4) ? numArgs : 4)) - 1;
    /*
     * Protect the loadMultiple instruction from being reordered with other
     * Dalvik stack accesses.
     */
    loadMultiple(cUnit, r4PC, regMask);

    opRegRegImm(cUnit, kOpSub, r_S4, rFP,
                sizeof(StackSaveArea) + (numArgs << 2));
    /* generate null check */
    if (pcrLabel) {
        *pcrLabel = genNullCheck(cUnit, dvmCompilerSSASrc(mir, 0), r_A0,
                                 mir->offset, NULL);
    }

    /*
     * Handle remaining 4n arguments:
     * store previously loaded 4 values and load the next 4 values
     */
    if (numArgs >= 8) {
        MipsLIR *loopLabel = NULL;
        /*
         * r_A0 contains "this" and it will be used later, so push it to the stack
         * first. Pushing r_S1 (rFP) is just for stack alignment purposes.
         */

        newLIR2(cUnit, kMipsMove, r_T0, r_A0);
        newLIR2(cUnit, kMipsMove, r_T1, r_S1);

        /* No need to generate the loop structure if numArgs <= 11 */
        if (numArgs > 11) {
            loadConstant(cUnit, rFP, ((numArgs - 4) >> 2) << 2);
            loopLabel = newLIR0(cUnit, kMipsPseudoTargetLabel);
            loopLabel->defMask = ENCODE_ALL;
        }
        storeMultiple(cUnit, r_S4, regMask);
        /*
         * Protect the loadMultiple instruction from being reordered with other
         * Dalvik stack accesses.
         */
        loadMultiple(cUnit, r4PC, regMask);
        /* No need to generate the loop structure if numArgs <= 11 */
        if (numArgs > 11) {
            opRegImm(cUnit, kOpSub, rFP, 4); 
            genConditionalBranchMips(cUnit, kMipsBne, rFP, r_ZERO, loopLabel);
        }
    }

    /* Save the last batch of loaded values */
    storeMultiple(cUnit, r_S4, regMask);

    /* Generate the loop epilogue - don't use r_A0 */
    if ((numArgs > 4) && (numArgs % 4)) {
        regMask = ((1 << (numArgs & 0x3)) - 1) << 1;
        /*
         * Protect the loadMultiple instruction from being reordered with other
         * Dalvik stack accesses.
         */
        loadMultiple(cUnit, r4PC, regMask);
    }
    if (numArgs >= 8) {
        newLIR2(cUnit, kMipsMove, r_A0, r_T0);
        newLIR2(cUnit, kMipsMove, r_S1, r_T1);
    }

    /* Save the modulo 4 arguments */
    if ((numArgs > 4) && (numArgs % 4)) {
        storeMultiple(cUnit, r_S4, regMask);
    }
}

/*
 * Generate code to setup the call stack then jump to the chaining cell if it
 * is not a native method.
 */
static void genInvokeSingletonCommon(CompilationUnit *cUnit, MIR *mir,
                                     BasicBlock *bb, MipsLIR *labelList,
                                     MipsLIR *pcrLabel,
                                     const Method *calleeMethod)
{
    /*
     * Note: all Dalvik register state should be flushed to
     * memory by the point, so register usage restrictions no
     * longer apply.  All temp & preserved registers may be used.
     */
    dvmCompilerLockAllTemps(cUnit);
    MipsLIR *retChainingCell = &labelList[bb->fallThrough->id];

    /* r_A1 = &retChainingCell */
    dvmCompilerLockTemp(cUnit, r_A1);
    MipsLIR *addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;
    addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;

    /* r4PC = dalvikCallsite */
    loadConstant(cUnit, r4PC,
                 (int) (cUnit->method->insns + mir->offset));
    /*
     * r_A0 = calleeMethod (loaded upon calling genInvokeSingletonCommon)
     * r_A1 = &ChainingCell
     * r4PC = callsiteDPC
     */
    if (dvmIsNativeMethod(calleeMethod)) {
        genDispatchToHandler(cUnit, TEMPLATE_INVOKE_METHOD_NATIVE);
#if defined(JIT_STATS)
        gDvmJit.invokeNative++;
#endif
    } else {
        genDispatchToHandler(cUnit, TEMPLATE_INVOKE_METHOD_CHAIN);
#if defined(JIT_STATS)
        gDvmJit.invokeMonomorphic++;
#endif
        /* Branch to the chaining cell */
        genUnconditionalBranch(cUnit, &labelList[bb->taken->id]);
    }
    /* Handle exceptions using the interpreter */
    genTrap(cUnit, mir->offset, pcrLabel);
}

/*
 * Generate code to check the validity of a predicted chain and take actions
 * based on the result.
 *
 * 0x426a99aa : ldr     r4, [pc, #72] --> r4 <- dalvikPC of this invoke
 * 0x426a99ac : add     r1, pc, #32   --> r1 <- &retChainingCell
 * 0x426a99ae : add     r2, pc, #40   --> r2 <- &predictedChainingCell
 * 0x426a99b0 : blx_1   0x426a918c    --+ TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN
 * 0x426a99b2 : blx_2   see above     --+
 * 0x426a99b4 : b       0x426a99d8    --> off to the predicted chain
 * 0x426a99b6 : b       0x426a99c8    --> punt to the interpreter
 * 0x426a99b8 : ldr     r0, [r7, #44] --> r0 <- this->class->vtable[methodIdx]
 * 0x426a99ba : cmp     r1, #0        --> compare r1 (rechain count) against 0
 * 0x426a99bc : bgt     0x426a99c2    --> >=0? don't rechain
 * 0x426a99be : ldr     r7, [r6, #96] --+ dvmJitToPatchPredictedChain
 * 0x426a99c0 : blx     r7            --+
 * 0x426a99c2 : add     r1, pc, #12   --> r1 <- &retChainingCell
 * 0x426a99c4 : blx_1   0x426a9098    --+ TEMPLATE_INVOKE_METHOD_NO_OPT
 * 0x426a99c6 : blx_2   see above     --+
 */
static void genInvokeVirtualCommon(CompilationUnit *cUnit, MIR *mir,
                                   int methodIndex,
                                   MipsLIR *retChainingCell,
                                   MipsLIR *predChainingCell,
                                   MipsLIR *pcrLabel)
{
    /*
     * Note: all Dalvik register state should be flushed to
     * memory by the point, so register usage restrictions no
     * longer apply.  Lock temps to prevent them from being
     * allocated by utility routines.
     */
    dvmCompilerLockAllTemps(cUnit);

    /* "this" is already left in r_A0 by genProcessArgs* */

    /* r4PC = dalvikCallsite */
    loadConstant(cUnit, r4PC,
                 (int) (cUnit->method->insns + mir->offset));

    /* r_A1 = &retChainingCell */
    MipsLIR *addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;
    addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;

    /* r_A2 = &predictedChainingCell */
    MipsLIR *predictedChainingCell = newLIR2(cUnit, kMipsLahi, r_A2, 0);
    predictedChainingCell->generic.target = (LIR *) predChainingCell;
    predictedChainingCell = newLIR3(cUnit, kMipsLalo, r_A2, r_A2, 0);
    predictedChainingCell->generic.target = (LIR *) predChainingCell;

    genDispatchToHandler(cUnit, TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN);

    /* return through lr - jump to the chaining cell */
    genUnconditionalBranch(cUnit, predChainingCell);

    /*
     * null-check on "this" may have been eliminated, but we still need a PC-
     * reconstruction label for stack overflow bailout.
     */
    if (pcrLabel == NULL) {
        int dPC = (int) (cUnit->method->insns + mir->offset);
        pcrLabel = dvmCompilerNew(sizeof(MipsLIR), true);
        pcrLabel->opCode = kMipsPseudoPCReconstructionCell;
        pcrLabel->operands[0] = dPC;
        pcrLabel->operands[1] = mir->offset;
        /* Insert the place holder to the growable list */
        dvmInsertGrowableList(&cUnit->pcReconstructionList, pcrLabel);
    }

    /* return through lr+8 - punt to the interpreter */
    genUnconditionalBranch(cUnit, pcrLabel);

    /*
     * return through lr+16 - fully resolve the callee method.
     * r_A1 <- count
     * r_A2 <- &predictedChainCell
     * r_A3 <- this->class
     * r4 <- dPC
     * r_S4 <- this->class->vtable
     */

    /* r_A0 <- calleeMethod */
    loadWordDisp(cUnit, r_S4, methodIndex * 4, r_A0);

    /* Check if rechain limit is reached */
    MipsLIR *bypassRechaining = opCondBranchMips(cUnit, kMipsBgtz, r_A1, -1);

    loadWordDisp(cUnit, rGLUE, offsetof(InterpState,
                 jitToInterpEntries.dvmJitToPatchPredictedChain), r_T9);

    /*
     * r_A0 = calleeMethod
     * r_A2 = &predictedChainingCell
     * r_A3 = class
     *
     * &returnChainingCell has been loaded into r_A1 but is not needed
     * when patching the chaining cell and will be clobbered upon
     * returning so it will be reconstructed again.
     */
    opReg(cUnit, kOpBlx, r_T9);
    newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
    newLIR2(cUnit, kMipsMove, r_A0, r_V0);

    /* r_A1 = &retChainingCell */
    addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;
    bypassRechaining->generic.target = (LIR *) addrRetChain;
    addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;

    /*
     * r_A0 = calleeMethod,
     * r_A1 = &ChainingCell,
     * r4PC = callsiteDPC,
     */
    genDispatchToHandler(cUnit, TEMPLATE_INVOKE_METHOD_NO_OPT);
#if defined(JIT_STATS)
    gDvmJit.invokePolymorphic++;
#endif
    /* Handle exceptions using the interpreter */
    genTrap(cUnit, mir->offset, pcrLabel);
}

/*
 * Up calling this function, "this" is stored in a0. The actual class will be
 * chased down off a0 and the predicted one will be retrieved through
 * predictedChainingCell then a comparison is performed to see whether the
 * previously established chaining is still valid.
 *
 * The return LIR is a branch based on the comparison result. The actual branch
 * target will be setup in the caller.
 */
static MipsLIR *genCheckPredictedChain(CompilationUnit *cUnit,
                                          MipsLIR *predChainingCell,
                                          MipsLIR *retChainingCell,
                                          MIR *mir)
{
    /*
     * Note: all Dalvik register state should be flushed to
     * memory by the point, so register usage restrictions no
     * longer apply.  All temp & preserved registers may be used.
     */
    dvmCompilerLockAllTemps(cUnit);

    /* a3 now contains this->clazz */
    loadWordDisp(cUnit, r_A0, offsetof(Object, clazz), r_A3);

    /*
     * a2 now contains predicted class. The starting offset of the
     * cached value is 8 bytes into the chaining cell.
     */
    MipsLIR *getPredictedClass = newLIR2(cUnit, kMipsLahi, r_A2, 0);
    getPredictedClass->generic.target = (LIR *) predChainingCell;
    getPredictedClass = newLIR3(cUnit, kMipsLalo, r_A2, r_A2, 0);
    getPredictedClass->generic.target = (LIR *) predChainingCell;
    newLIR3(cUnit, kMipsAddiu, r_A2, r_A2, offsetof(PredictedChainingCell, clazz));
                                            
    /*
     * r_A0 now contains predicted method. The starting offset of the
     * cached value is 12 bytes into the chaining cell.
     */
    MipsLIR *getPredictedMethod = newLIR2(cUnit, kMipsLahi, r_A0, 0);
    getPredictedMethod->generic.target = (LIR *) predChainingCell;
    getPredictedMethod = newLIR3(cUnit, kMipsLalo, r_A0, r_A0, 0);
    getPredictedMethod->generic.target = (LIR *) predChainingCell;
    newLIR3(cUnit, kMipsAddiu, r_A0, r_A0, offsetof(PredictedChainingCell, method));

    /* Load the stats counter to see if it is time to unchain and refresh */
    MipsLIR *getRechainingRequestCount = newLIR2(cUnit, kMipsLahi, r_S4, 0);
    getRechainingRequestCount->generic.target = (LIR *) predChainingCell;
    getRechainingRequestCount = newLIR3(cUnit, kMipsLalo, r_S4, r_S4, 0);
    getRechainingRequestCount->generic.target = (LIR *) predChainingCell;
    newLIR3(cUnit, kMipsAddiu, r_S4, r_S4, offsetof(PredictedChainingCell, counter));

    /* r4PC = dalvikCallsite */
    loadConstant(cUnit, r4PC,
                 (int) (cUnit->method->insns + mir->offset));

    /* r_A1 = &retChainingCell */
    MipsLIR *addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;
    addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
    addrRetChain->generic.target = (LIR *) retChainingCell;

    /* Check if r2 (predicted class) == r3 (actual class) */
    return opCondBranchMips(cUnit, kMipsBeq, r_A2, r_A3);
}

/* Geneate a branch to go back to the interpreter */
static void genPuntToInterp(CompilationUnit *cUnit, unsigned int offset)
{
    /* a0 = dalvik pc */
    dvmCompilerFlushAllRegs(cUnit);
    loadConstant(cUnit, r_A0, (int) (cUnit->method->insns + offset));
#if 0 /* MIPSTODO tempoary workaround unaligned access on sigma hardware
             this can removed when we're not punting to genInterpSingleStep 
             for opcodes that haven't been activated yet */
    loadWordDisp(cUnit, r_A0, offsetof(Object, clazz), r_A3);
#endif
    loadWordDisp(cUnit, rGLUE, offsetof(InterpState,
                 jitToInterpEntries.dvmJitToInterpPunt), r_A1);

    opReg(cUnit, kOpBlx, r_A1);
}

/*
 * Attempt to single step one instruction using the interpreter and return
 * to the compiled code for the next Dalvik instruction
 */
static void genInterpSingleStep(CompilationUnit *cUnit, MIR *mir)
{
    int flags = dexGetInstrFlags(gDvm.instrFlags, mir->dalvikInsn.opCode);
    int flagsToCheck = kInstrCanBranch | kInstrCanSwitch | kInstrCanReturn |
                       kInstrCanThrow;

    //If already optimized out, just ignore
    if (mir->dalvikInsn.opCode == OP_NOP)
        return;

    //Ugly, but necessary.  Flush all Dalvik regs so Interp can find them
    dvmCompilerFlushAllRegs(cUnit);

    if ((mir->next == NULL) || (flags & flagsToCheck)) {
       genPuntToInterp(cUnit, mir->offset);
       return;
    }
    int entryAddr = offsetof(InterpState,
                             jitToInterpEntries.dvmJitToInterpSingleStep);
    loadWordDisp(cUnit, rGLUE, entryAddr, r_A2);
    /* a0 = dalvik pc */
    loadConstant(cUnit, r_A0, (int) (cUnit->method->insns + mir->offset));
    /* a1 = dalvik pc of following instruction */
    loadConstant(cUnit, r_A1, (int) (cUnit->method->insns + mir->next->offset));
    opReg(cUnit, kOpBlx, r_A2);
}

/*
 * To prevent a thread in a monitor wait from blocking the Jit from
 * resetting the code cache, heavyweight monitor lock will not
 * be allowed to return to an existing translation.  Instead, we will
 * handle them by branching to a handler, which will in turn call the
 * runtime lock routine and then branch directly back to the
 * interpreter main loop.  Given the high cost of the heavyweight
 * lock operation, this additional cost should be slight (especially when
 * considering that we expect the vast majority of lock operations to
 * use the fast-path thin lock bypass).
 */
static void genMonitorPortable(CompilationUnit *cUnit, MIR *mir)
{
    bool isEnter = (mir->dalvikInsn.opCode == OP_MONITOR_ENTER);
    genExportPC(cUnit, mir);
    dvmCompilerFlushAllRegs(cUnit);   /* Send everything to home location */
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    loadValueDirectFixed(cUnit, rlSrc, r_A1);
    loadWordDisp(cUnit, rGLUE, offsetof(InterpState, self), r_A0);
    genNullCheck(cUnit, rlSrc.sRegLow, r_A1, mir->offset, NULL);
    if (isEnter) {
        /* Get dPC of next insn */
        loadConstant(cUnit, r4PC, (int)(cUnit->method->insns + mir->offset +
                 dexGetInstrWidthAbs(gDvm.instrWidth, OP_MONITOR_ENTER)));
#if defined(WITH_DEADLOCK_PREDICTION)
        genDispatchToHandler(cUnit, TEMPLATE_MONITOR_ENTER_DEBUG);
#else
        genDispatchToHandler(cUnit, TEMPLATE_MONITOR_ENTER);
#endif
    } else {
        LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmUnlockObject);
        /* Do the call */
        opReg(cUnit, kOpBlx, r_T9);
        newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
        /* Did we throw? */
        MipsLIR *branchOver = opCondBranchMips(cUnit, kMipsBne, r_V0, r_ZERO);
        loadConstant(cUnit, r_A0,
                     (int) (cUnit->method->insns + mir->offset +
                     dexGetInstrWidthAbs(gDvm.instrWidth, OP_MONITOR_EXIT)));
        genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
        MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
        target->defMask = ENCODE_ALL;
        branchOver->generic.target = (LIR *) target;
        dvmCompilerClobberCallRegs(cUnit);
    }
}

/*
 * The following are the first-level codegen routines that analyze the format
 * of each bytecode then either dispatch special purpose codegen routines
 * or produce corresponding Thumb instructions directly.
 */

static bool handleFmt10t_Fmt20t_Fmt30t(CompilationUnit *cUnit, MIR *mir,
                                       BasicBlock *bb, MipsLIR *labelList)
{
    /* For OP_GOTO, OP_GOTO_16, and OP_GOTO_32 */
    genUnconditionalBranch(cUnit, &labelList[bb->taken->id]);
    return false;
}

static bool handleFmt10x(CompilationUnit *cUnit, MIR *mir)
{
    OpCode dalvikOpCode = mir->dalvikInsn.opCode;
    if (((dalvikOpCode >= OP_UNUSED_3E) && (dalvikOpCode <= OP_UNUSED_43)) ||
        ((dalvikOpCode >= OP_UNUSED_E3) && (dalvikOpCode <= OP_UNUSED_EB))) {
        LOGE("Codegen: got unused opcode 0x%x\n",dalvikOpCode);
        return true;
    }
    switch (dalvikOpCode) {
        case OP_RETURN_VOID:
            genReturnCommon(cUnit,mir);
            break;
        case OP_UNUSED_73:
        case OP_UNUSED_79:
        case OP_UNUSED_7A:
            LOGE("Codegen: got unused opcode 0x%x\n",dalvikOpCode);
            return true;
        case OP_NOP:
            break;
        default:
            return true;
    }
    return false;
}

static bool handleFmt11n_Fmt31i(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlDest;
    RegLocation rlResult;
    if (mir->ssaRep->numDefs == 2) {
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    } else {
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    }

    switch (mir->dalvikInsn.opCode) {
        case OP_CONST:
        case OP_CONST_4: {
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);
            loadConstantNoClobber(cUnit, rlResult.lowReg, mir->dalvikInsn.vB);
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_CONST_WIDE_32: {
            //TUNING: single routine to load constant pair for support doubles
            //TUNING: load 0/-1 separately to avoid load dependency
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            loadConstantNoClobber(cUnit, rlResult.lowReg, mir->dalvikInsn.vB);
            opRegRegImm(cUnit, kOpAsr, rlResult.highReg,
                        rlResult.lowReg, 31);
            storeValueWide(cUnit, rlDest, rlResult);
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt21h(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlDest;
    RegLocation rlResult;
    if (mir->ssaRep->numDefs == 2) {
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    } else {
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    }
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);

    switch (mir->dalvikInsn.opCode) {
        case OP_CONST_HIGH16: {
            loadConstantNoClobber(cUnit, rlResult.lowReg,
                                  mir->dalvikInsn.vB << 16);
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_CONST_WIDE_HIGH16: {
            loadConstantValueWide(cUnit, rlResult.lowReg, rlResult.highReg,
                                  0, mir->dalvikInsn.vB << 16);
            storeValueWide(cUnit, rlDest, rlResult);
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt20bc(CompilationUnit *cUnit, MIR *mir)
{
    /* For OP_THROW_VERIFICATION_ERROR */
    genInterpSingleStep(cUnit, mir);
    return false;
}

static bool handleFmt21c_Fmt31c(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlResult;
    RegLocation rlDest;
    RegLocation rlSrc;

    switch (mir->dalvikInsn.opCode) {
        case OP_CONST_STRING_JUMBO:
        case OP_CONST_STRING: {
            void *strPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResStrings[mir->dalvikInsn.vB]);
            assert(strPtr != NULL);
            rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            loadConstantNoClobber(cUnit, rlResult.lowReg, (int) strPtr );
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_CONST_CLASS: {
            void *classPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResClasses[mir->dalvikInsn.vB]);
            assert(classPtr != NULL);
            rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            loadConstantNoClobber(cUnit, rlResult.lowReg, (int) classPtr );
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_SGET_OBJECT:
        case OP_SGET_BOOLEAN:
        case OP_SGET_CHAR:
        case OP_SGET_BYTE:
        case OP_SGET_SHORT:
        case OP_SGET: {
            int valOffset = offsetof(StaticField, value);
            int tReg = dvmCompilerAllocTemp(cUnit);
            void *fieldPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResFields[mir->dalvikInsn.vB]);
            assert(fieldPtr != NULL);
            rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);
            loadConstant(cUnit, tReg,  (int) fieldPtr + valOffset);

            HEAP_ACCESS_SHADOW(true);
            loadWordDisp(cUnit, tReg, 0, rlResult.lowReg);
            HEAP_ACCESS_SHADOW(false);

            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_SGET_WIDE: {
            int valOffset = offsetof(StaticField, value);
            void *fieldPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResFields[mir->dalvikInsn.vB]);
            int tReg = dvmCompilerAllocTemp(cUnit);
            assert(fieldPtr != NULL);
            rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);
            loadConstant(cUnit, tReg,  (int) fieldPtr + valOffset);

            HEAP_ACCESS_SHADOW(true);
            loadPair(cUnit, tReg, rlResult.lowReg, rlResult.highReg);
            HEAP_ACCESS_SHADOW(false);

            storeValueWide(cUnit, rlDest, rlResult);
            break;
        }
        case OP_SPUT_OBJECT:
        case OP_SPUT_BOOLEAN:
        case OP_SPUT_CHAR:
        case OP_SPUT_BYTE:
        case OP_SPUT_SHORT:
        case OP_SPUT: {
            int valOffset = offsetof(StaticField, value);
            int tReg = dvmCompilerAllocTemp(cUnit);
            void *fieldPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResFields[mir->dalvikInsn.vB]);

            assert(fieldPtr != NULL);
            rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            rlSrc = loadValue(cUnit, rlSrc, kAnyReg);
            loadConstant(cUnit, tReg,  (int) fieldPtr + valOffset);

            HEAP_ACCESS_SHADOW(true);
            storeWordDisp(cUnit, tReg, 0 ,rlSrc.lowReg);
            HEAP_ACCESS_SHADOW(false);

            break;
        }
        case OP_SPUT_WIDE: {
            int tReg = dvmCompilerAllocTemp(cUnit);
            int valOffset = offsetof(StaticField, value);
            void *fieldPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResFields[mir->dalvikInsn.vB]);

            assert(fieldPtr != NULL);
            rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
            rlSrc = loadValueWide(cUnit, rlSrc, kAnyReg);
            loadConstant(cUnit, tReg,  (int) fieldPtr + valOffset);

            HEAP_ACCESS_SHADOW(true);
            storePair(cUnit, tReg, rlSrc.lowReg, rlSrc.highReg);
            HEAP_ACCESS_SHADOW(false);
            break;
        }
        case OP_NEW_INSTANCE: {
            /*
             * Obey the calling convention and don't mess with the register
             * usage.
             */
            ClassObject *classPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResClasses[mir->dalvikInsn.vB]);
            assert(classPtr != NULL);
            assert(classPtr->status & CLASS_INITIALIZED);
            /*
             * If it is going to throw, it should not make to the trace to begin
             * with.  However, Alloc might throw, so we need to genExportPC()
             */
            assert((classPtr->accessFlags & (ACC_INTERFACE|ACC_ABSTRACT)) == 0);
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            genExportPC(cUnit, mir);
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmAllocObject);
            loadConstant(cUnit, r_A0, (int) classPtr);
            loadConstant(cUnit, r_A1, ALLOC_DONT_TRACK);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /* generate a branch over if allocation is successful */
            MipsLIR *branchOver = opCondBranchMips(cUnit, kMipsBne, r_V0, r_ZERO);

            /*
             * OOM exception needs to be thrown here and cannot re-execute
             */
            loadConstant(cUnit, r_A0,
                         (int) (cUnit->method->insns + mir->offset));
            genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
            /* noreturn */

            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branchOver->generic.target = (LIR *) target;
            rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            rlResult = dvmCompilerGetReturn(cUnit);
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_CHECK_CAST: {
            /*
             * Obey the calling convention and don't mess with the register
             * usage.
             */
            ClassObject *classPtr =
              (cUnit->method->clazz->pDvmDex->pResClasses[mir->dalvikInsn.vB]);
            /*
             * Note: It is possible that classPtr is NULL at this point,
             * even though this instruction has been successfully interpreted.
             * If the previous interpretation had a null source, the
             * interpreter would not have bothered to resolve the clazz.
             * Bail out to the interpreter in this case, and log it
             * so that we can tell if it happens frequently.
             */
            if (classPtr == NULL) {
                 LOGVV("null clazz in OP_CHECK_CAST, single-stepping");
                 genInterpSingleStep(cUnit, mir);
                 return false;
            }
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            loadConstant(cUnit, r_A1, (int) classPtr );
            rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            MipsLIR *branch1 = opCondBranchMips(cUnit, kMipsBeqz, rlSrc.lowReg, -1);
            /*
             *  rlSrc.lowReg now contains object->clazz.  Note that
             *  it could have been allocated r_A0, but we're okay so long
             *  as we don't do anything desctructive until r_A0 is loaded
             *  with clazz.
             */
            /* r_A0 now contains object->clazz */
            loadWordDisp(cUnit, rlSrc.lowReg, offsetof(Object, clazz), r_A0);
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmInstanceofNonTrivial);
            MipsLIR *branch2 = opCondBranchMips(cUnit, kMipsBeq, r_A0, r_A1);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /*
             * If null, check cast failed - punt to the interpreter.  Because
             * interpreter will be the one throwing, we don't need to
             * genExportPC() here.
             */
            genRegCopy(cUnit, r_A0, r_V0);
            genZeroCheck(cUnit, r_V0, mir->offset, NULL);
            /* check cast passed - branch target here */
            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branch1->generic.target = (LIR *)target;
            branch2->generic.target = (LIR *)target;
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt11x(CompilationUnit *cUnit, MIR *mir)
{
    OpCode dalvikOpCode = mir->dalvikInsn.opCode;
    RegLocation rlResult;
    switch (dalvikOpCode) {
        case OP_MOVE_EXCEPTION: {
            int offset = offsetof(InterpState, self);
            int exOffset = offsetof(Thread, exception);
            int selfReg = dvmCompilerAllocTemp(cUnit);
            int resetReg = dvmCompilerAllocTemp(cUnit);
            RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            loadWordDisp(cUnit, rGLUE, offset, selfReg);
            loadConstant(cUnit, resetReg, 0);
            loadWordDisp(cUnit, selfReg, exOffset, rlResult.lowReg);
            storeWordDisp(cUnit, selfReg, exOffset, resetReg);
            storeValue(cUnit, rlDest, rlResult);
           break;
        }
        case OP_MOVE_RESULT:
        case OP_MOVE_RESULT_OBJECT: {
            RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            RegLocation rlSrc = LOC_DALVIK_RETURN_VAL;
            rlSrc.fp = rlDest.fp;
            storeValue(cUnit, rlDest, rlSrc);
            break;
        }
        case OP_MOVE_RESULT_WIDE: {
            RegLocation rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
            RegLocation rlSrc = LOC_DALVIK_RETURN_VAL_WIDE;
            rlSrc.fp = rlDest.fp;
            storeValueWide(cUnit, rlDest, rlSrc);
            break;
        }
        case OP_RETURN_WIDE: {
            RegLocation rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
            RegLocation rlDest = LOC_DALVIK_RETURN_VAL_WIDE;
            rlDest.fp = rlSrc.fp;
            storeValueWide(cUnit, rlDest, rlSrc);
            genReturnCommon(cUnit,mir);
            break;
        }
        case OP_RETURN:
        case OP_RETURN_OBJECT: {
            RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            RegLocation rlDest = LOC_DALVIK_RETURN_VAL;
            rlDest.fp = rlSrc.fp;
            storeValue(cUnit, rlDest, rlSrc);
            genReturnCommon(cUnit,mir);
            break;
        }
        case OP_MONITOR_EXIT:
        case OP_MONITOR_ENTER:
#if defined(WITH_DEADLOCK_PREDICTION) || defined(WITH_MONITOR_TRACKING)
            genMonitorPortable(cUnit, mir);
#else
            genMonitor(cUnit, mir);
#endif
            break;
        case OP_THROW: {
            genInterpSingleStep(cUnit, mir);
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt12x(CompilationUnit *cUnit, MIR *mir)
{
    OpCode opCode = mir->dalvikInsn.opCode;
    RegLocation rlDest;
    RegLocation rlSrc;
    RegLocation rlResult;

    if ( (opCode >= OP_ADD_INT_2ADDR) && (opCode <= OP_REM_DOUBLE_2ADDR)) {
        return genArithOp( cUnit, mir );
    }

    if (mir->ssaRep->numUses == 2)
        rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    else
        rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    if (mir->ssaRep->numDefs == 2)
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    else
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);

    switch (opCode) {
        case OP_DOUBLE_TO_INT:
        case OP_INT_TO_FLOAT:
        case OP_FLOAT_TO_INT:
        case OP_DOUBLE_TO_FLOAT:
        case OP_FLOAT_TO_DOUBLE:
        case OP_INT_TO_DOUBLE:
        case OP_FLOAT_TO_LONG:
        case OP_LONG_TO_FLOAT:
        case OP_DOUBLE_TO_LONG:
        case OP_LONG_TO_DOUBLE:
            return genConversion(cUnit, mir);
        case OP_NEG_INT:
        case OP_NOT_INT:
            return genArithOpInt(cUnit, mir, rlDest, rlSrc, rlSrc);
        case OP_NEG_LONG:
        case OP_NOT_LONG:
            return genArithOpLong(cUnit, mir, rlDest, rlSrc, rlSrc);
        case OP_NEG_FLOAT:
            return genArithOpFloat(cUnit, mir, rlDest, rlSrc, rlSrc);
        case OP_NEG_DOUBLE:
            return genArithOpDouble(cUnit, mir, rlDest, rlSrc, rlSrc);
        case OP_MOVE_WIDE:
            storeValueWide(cUnit, rlDest, rlSrc);
            break;
        case OP_INT_TO_LONG:
            rlSrc = dvmCompilerUpdateLoc(cUnit, rlSrc);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            //TUNING: shouldn't loadValueDirect already check for phys reg?
            if (rlSrc.location == kLocPhysReg) {
                genRegCopy(cUnit, rlResult.lowReg, rlSrc.lowReg);
            } else {
                loadValueDirect(cUnit, rlSrc, rlResult.lowReg);
            }
            opRegRegImm(cUnit, kOpAsr, rlResult.highReg,
                        rlResult.lowReg, 31);
            storeValueWide(cUnit, rlDest, rlResult);
            break;
        case OP_LONG_TO_INT:
            rlSrc = dvmCompilerUpdateLocWide(cUnit, rlSrc);
            rlSrc = dvmCompilerWideToNarrow(cUnit, rlSrc);
            // Intentional fallthrough
        case OP_MOVE:
        case OP_MOVE_OBJECT:
            storeValue(cUnit, rlDest, rlSrc);
            break;
        case OP_INT_TO_BYTE:
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegReg(cUnit, kOp2Byte, rlResult.lowReg, rlSrc.lowReg);
            storeValue(cUnit, rlDest, rlResult);
            break;
        case OP_INT_TO_SHORT:
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegReg(cUnit, kOp2Short, rlResult.lowReg, rlSrc.lowReg);
            storeValue(cUnit, rlDest, rlResult);
            break;
        case OP_INT_TO_CHAR:
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegReg(cUnit, kOp2Char, rlResult.lowReg, rlSrc.lowReg);
            storeValue(cUnit, rlDest, rlResult);
            break;
        case OP_ARRAY_LENGTH: {
            int lenOffset = offsetof(ArrayObject, length);
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            genNullCheck(cUnit, rlSrc.sRegLow, rlSrc.lowReg,
                         mir->offset, NULL);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            loadWordDisp(cUnit, rlSrc.lowReg, lenOffset,
                         rlResult.lowReg);
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt21s(CompilationUnit *cUnit, MIR *mir)
{
    OpCode dalvikOpCode = mir->dalvikInsn.opCode;
    RegLocation rlDest;
    RegLocation rlResult;
    int BBBB = mir->dalvikInsn.vB;
    if (dalvikOpCode == OP_CONST_WIDE_16) {
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
        loadConstantNoClobber(cUnit, rlResult.lowReg, BBBB);
        //TUNING: do high separately to avoid load dependency
        opRegRegImm(cUnit, kOpAsr, rlResult.highReg, rlResult.lowReg, 31);
        storeValueWide(cUnit, rlDest, rlResult);
    } else if (dalvikOpCode == OP_CONST_16) {
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
        rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kAnyReg, true);
        loadConstantNoClobber(cUnit, rlResult.lowReg, BBBB);
        storeValue(cUnit, rlDest, rlResult);
    } else
        return true;
    return false;
}

/* Compare agaist zero */
static bool handleFmt21t(CompilationUnit *cUnit, MIR *mir, BasicBlock *bb,
                         MipsLIR *labelList)
{
    OpCode dalvikOpCode = mir->dalvikInsn.opCode;
    MipsOpCode opc;
    int rt = -1;

    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    rlSrc = loadValue(cUnit, rlSrc, kCoreReg);

//TUNING: break this out to allow use of Thumb2 CB[N]Z
    switch (dalvikOpCode) {
        case OP_IF_EQZ:
            opc = kMipsBeqz;
            break;
        case OP_IF_NEZ:
            opc = kMipsBne;
            rt = r_ZERO;
            break;
        case OP_IF_LTZ:
            opc = kMipsBltz;
            break;
        case OP_IF_GEZ:
            opc = kMipsBgez;
            break;
        case OP_IF_GTZ:
            opc = kMipsBgtz;
            break;
        case OP_IF_LEZ:
            opc = kMipsBlez;
            break;
        default:
            LOGE("Unexpected opcode (%d) for Fmt21t\n", dalvikOpCode);
            dvmCompilerAbort(cUnit);
    }
    genConditionalBranchMips(cUnit, opc, rlSrc.lowReg, rt, &labelList[bb->taken->id]);
    /* This mostly likely will be optimized away in a later phase */
    genUnconditionalBranch(cUnit, &labelList[bb->fallThrough->id]);
    return false;
}

static bool isPowerOfTwo(int x)
{
    return (x & (x - 1)) == 0;
}

// Returns true if no more than two bits are set in 'x'.
static bool isPopCountLE2(unsigned int x)
{
    x &= x - 1;
    return (x & (x - 1)) == 0;
}

// Returns the index of the lowest set bit in 'x'.
static int lowestSetBit(unsigned int x) {
    int bit_posn = 0;
    while ((x & 0xf) == 0) {
        bit_posn += 4;
        x >>= 4;
    }
    while ((x & 1) == 0) {
        bit_posn++;
        x >>= 1;
    }
    return bit_posn;
}

// Returns true if it added instructions to 'cUnit' to multiply 'rlSrc' by 'lit'
// and store the result in 'rlDest'.
static bool handleEasyMultiply(CompilationUnit *cUnit,
                               RegLocation rlSrc, RegLocation rlDest, int lit)
{
    // Can we simplify this multiplication?
    bool powerOfTwo = false;
    bool popCountLE2 = false;
    bool powerOfTwoMinusOne = false;
    if (lit < 2) {
        // Avoid special cases.
        return false;
    } else if (isPowerOfTwo(lit)) {
        powerOfTwo = true;
    } else if (isPopCountLE2(lit)) {
        popCountLE2 = true;
    } else if (isPowerOfTwo(lit + 1)) {
        powerOfTwoMinusOne = true;
    } else {
        return false;
    }
    rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    if (powerOfTwo) {
        // Shift.
        opRegRegImm(cUnit, kOpLsl, rlResult.lowReg, rlSrc.lowReg,
                    lowestSetBit(lit));
    } else if (popCountLE2) {
        // Shift and add and shift.
        int firstBit = lowestSetBit(lit);
        int secondBit = lowestSetBit(lit ^ (1 << firstBit));
        genMultiplyByTwoBitMultiplier(cUnit, rlSrc, rlResult, lit,
                                      firstBit, secondBit);
    } else {
        // Reverse subtract: (src << (shift + 1)) - src.
        assert(powerOfTwoMinusOne);
        // TODO: rsb dst, src, src lsl#lowestSetBit(lit + 1)
        int tReg = dvmCompilerAllocTemp(cUnit);
        opRegRegImm(cUnit, kOpLsl, tReg, rlSrc.lowReg, lowestSetBit(lit + 1));
        opRegRegReg(cUnit, kOpSub, rlResult.lowReg, tReg, rlSrc.lowReg);
    }
    storeValue(cUnit, rlDest, rlResult);
    return true;
}

static bool handleFmt22b_Fmt22s(CompilationUnit *cUnit, MIR *mir)
{
    OpCode dalvikOpCode = mir->dalvikInsn.opCode;
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    RegLocation rlResult;
    int lit = mir->dalvikInsn.vC;
    OpKind op = 0;      /* Make gcc happy */
    int shiftOp = false;
    bool isDiv = false;

    switch (dalvikOpCode) {
        case OP_RSUB_INT_LIT8:
        case OP_RSUB_INT: {
            int tReg;
            //TUNING: add support for use of Arm rsub op
            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            tReg = dvmCompilerAllocTemp(cUnit);
            loadConstant(cUnit, tReg, lit);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            opRegRegReg(cUnit, kOpSub, rlResult.lowReg,
                        tReg, rlSrc.lowReg);
            storeValue(cUnit, rlDest, rlResult);
            return false;
            break;
        }

        case OP_ADD_INT_LIT8:
        case OP_ADD_INT_LIT16:
            op = kOpAdd;
            break;
        case OP_MUL_INT_LIT8:
        case OP_MUL_INT_LIT16: {
            if (handleEasyMultiply(cUnit, rlSrc, rlDest, lit)) {
                return false;
            }
            op = kOpMul;
            break;
        }
        case OP_AND_INT_LIT8:
        case OP_AND_INT_LIT16:
            op = kOpAnd;
            break;
        case OP_OR_INT_LIT8:
        case OP_OR_INT_LIT16:
            op = kOpOr;
            break;
        case OP_XOR_INT_LIT8:
        case OP_XOR_INT_LIT16:
            op = kOpXor;
            break;
        case OP_SHL_INT_LIT8:
            lit &= 31;
            shiftOp = true;
            op = kOpLsl;
            break;
        case OP_SHR_INT_LIT8:
            lit &= 31;
            shiftOp = true;
            op = kOpAsr;
            break;
        case OP_USHR_INT_LIT8:
            lit &= 31;
            shiftOp = true;
            op = kOpLsr;
            break;

        case OP_DIV_INT_LIT8:
        case OP_DIV_INT_LIT16:
        case OP_REM_INT_LIT8:
        case OP_REM_INT_LIT16: {
            if (lit == 0) {
                /* Let the interpreter deal with div by 0 */
                genInterpSingleStep(cUnit, mir);
                return false;
            }

            MipsOpCode opc;
            int divReg;  

            if ((dalvikOpCode == OP_DIV_INT_LIT8) ||
                (dalvikOpCode == OP_DIV_INT_LIT16)) {
                opc = kMipsMflo;
                divReg = r_LO;
            } else {
                opc = kMipsMfhi;
                divReg = r_HI;
            }

            rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
            int tReg = dvmCompilerAllocTemp(cUnit);
            newLIR3(cUnit, kMipsAddiu, tReg, r_ZERO, lit);
            newLIR4(cUnit, kMipsDiv, r_HI, r_LO, rlSrc.lowReg, tReg);
            rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
            newLIR2(cUnit, opc, rlResult.lowReg, divReg);
            dvmCompilerFreeTemp(cUnit, tReg);
            storeValue(cUnit, rlDest, rlResult);
            return false;
            break;
        }
        default:
            return true;
    }
    rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    // Avoid shifts by literal 0 - no support in Thumb.  Change to copy
    if (shiftOp && (lit == 0)) {
        genRegCopy(cUnit, rlResult.lowReg, rlSrc.lowReg);
    } else {
        opRegRegImm(cUnit, op, rlResult.lowReg, rlSrc.lowReg, lit);
    }
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool handleFmt22c(CompilationUnit *cUnit, MIR *mir)
{
    OpCode dalvikOpCode = mir->dalvikInsn.opCode;
    int fieldOffset;

    if (dalvikOpCode >= OP_IGET && dalvikOpCode <= OP_IPUT_SHORT) {
        InstField *pInstField = (InstField *)
            cUnit->method->clazz->pDvmDex->pResFields[mir->dalvikInsn.vC];

        assert(pInstField != NULL);
        fieldOffset = pInstField->byteOffset;
    } else {
        /* Deliberately break the code while make the compiler happy */
        fieldOffset = -1;
    }
    switch (dalvikOpCode) {
        case OP_NEW_ARRAY: {
#if 0 /* 080 triggers assert in Interp.c:1290 for out of memory exception.
             i think the assert is in error and should be disabled. With
             asserts disabled, 080 passes. */
genInterpSingleStep(cUnit, mir);
return false;
#endif
            // Generates a call - use explicit registers
            RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            RegLocation rlResult;
            void *classPtr = (void*)
              (cUnit->method->clazz->pDvmDex->pResClasses[mir->dalvikInsn.vC]);
            assert(classPtr != NULL);
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            genExportPC(cUnit, mir);
            loadValueDirectFixed(cUnit, rlSrc, r_A1);   /* Len */
            loadConstant(cUnit, r_A0, (int) classPtr );
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmAllocArrayByClass);
            /*
             * "len < 0": bail to the interpreter to re-execute the
             * instruction
             */
            MipsLIR *pcrLabel =
                genRegImmCheck(cUnit, kMipsCondMi, r_A1, 0, mir->offset, NULL);
            loadConstant(cUnit, r_A2, ALLOC_DONT_TRACK);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /* generate a branch over if allocation is successful */
            MipsLIR *branchOver = opCondBranchMips(cUnit, kMipsBne, r_V0, r_ZERO);
            /*
             * OOM exception needs to be thrown here and cannot re-execute
             */
            loadConstant(cUnit, r_A0,
                         (int) (cUnit->method->insns + mir->offset));
            genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
            /* noreturn */

            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branchOver->generic.target = (LIR *) target;
            rlResult = dvmCompilerGetReturn(cUnit);
            storeValue(cUnit, rlDest, rlResult);
            break;
        }
        case OP_INSTANCE_OF: {
            // May generate a call - use explicit registers
            RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            RegLocation rlDest = dvmCompilerGetDest(cUnit, mir, 0);
            RegLocation rlResult;
            ClassObject *classPtr =
              (cUnit->method->clazz->pDvmDex->pResClasses[mir->dalvikInsn.vC]);
            /*
             * Note: It is possible that classPtr is NULL at this point,
             * even though this instruction has been successfully interpreted.
             * If the previous interpretation had a null source, the
             * interpreter would not have bothered to resolve the clazz.
             * Bail out to the interpreter in this case, and log it
             * so that we can tell if it happens frequently.
             */
            if (classPtr == NULL) {
                LOGD("null clazz in OP_INSTANCE_OF, single-stepping");
                genInterpSingleStep(cUnit, mir);
                break;
            }
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            loadValueDirectFixed(cUnit, rlSrc, r_V0);  /* Ref */
            loadConstant(cUnit, r_A2, (int) classPtr );
//TUNING: compare to 0 primative to allow use of CB[N]Z
            /* When taken r_V0 has NULL which can be used for store directly */
            MipsLIR *branch1 = opCondBranchMips(cUnit, kMipsBeqz, r_V0, -1);
            /* r_A1 now contains object->clazz */
            loadWordDisp(cUnit, r_V0, offsetof(Object, clazz), r_A1);
            /* r_A1 now contains object->clazz */
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmInstanceofNonTrivial);
            loadConstant(cUnit, r_V0, 1);                /* Assume true */
            MipsLIR *branch2 = opCondBranchMips(cUnit, kMipsBeq, r_A1, r_A2);
            genRegCopy(cUnit, r_A0, r_A1);
            genRegCopy(cUnit, r_A1, r_A2);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /* branch target here */
            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            rlResult = dvmCompilerGetReturn(cUnit);
            storeValue(cUnit, rlDest, rlResult);
            branch1->generic.target = (LIR *)target;
            branch2->generic.target = (LIR *)target;
            break;
        }
        case OP_IGET_WIDE:
            genIGetWide(cUnit, mir, fieldOffset);
            break;
        case OP_IGET:
        case OP_IGET_OBJECT:
            genIGet(cUnit, mir, kWord, fieldOffset);
            break;
        case OP_IGET_BOOLEAN:
            genIGet(cUnit, mir, kUnsignedByte, fieldOffset);
            break;
        case OP_IGET_BYTE:
            genIGet(cUnit, mir, kSignedByte, fieldOffset);
            break;
        case OP_IGET_CHAR:
            genIGet(cUnit, mir, kUnsignedHalf, fieldOffset);
            break;
        case OP_IGET_SHORT:
            genIGet(cUnit, mir, kSignedHalf, fieldOffset);
            break;
        case OP_IPUT_WIDE:
            genIPutWide(cUnit, mir, fieldOffset);
            break;
        case OP_IPUT:
        case OP_IPUT_OBJECT:
            genIPut(cUnit, mir, kWord, fieldOffset);
            break;
        case OP_IPUT_SHORT:
        case OP_IPUT_CHAR:
            genIPut(cUnit, mir, kUnsignedHalf, fieldOffset);
            break;
        case OP_IPUT_BYTE:
        case OP_IPUT_BOOLEAN:
            genIPut(cUnit, mir, kUnsignedByte, fieldOffset);
            break;
        default:
            return true;
    }
    return false;
}

static bool handleFmt22cs(CompilationUnit *cUnit, MIR *mir)
{
    OpCode dalvikOpCode = mir->dalvikInsn.opCode;
    int fieldOffset =  mir->dalvikInsn.vC;
    switch (dalvikOpCode) {
        case OP_IGET_QUICK:
        case OP_IGET_OBJECT_QUICK:
            genIGet(cUnit, mir, kWord, fieldOffset);
            break;
        case OP_IPUT_QUICK:
        case OP_IPUT_OBJECT_QUICK:
            genIPut(cUnit, mir, kWord, fieldOffset);
            break;
        case OP_IGET_WIDE_QUICK:
            genIGetWide(cUnit, mir, fieldOffset);
            break;
        case OP_IPUT_WIDE_QUICK:
            genIPutWide(cUnit, mir, fieldOffset);
            break;
        default:
            return true;
    }
    return false;

}

static bool handleFmt22t(CompilationUnit *cUnit, MIR *mir, BasicBlock *bb,
                         MipsLIR *labelList)
{
    OpCode dalvikOpCode = mir->dalvikInsn.opCode;
    MipsConditionCode cond;
    MipsOpCode opc;
    MipsLIR * test = NULL;
    RegLocation rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 1);
    rlSrc1 = loadValue(cUnit, rlSrc1, kCoreReg);
    rlSrc2 = loadValue(cUnit, rlSrc2, kCoreReg);
    int reg1 = rlSrc1.lowReg;
    int reg2 = rlSrc2.lowReg;
    int tReg;

    switch (dalvikOpCode) {
        case OP_IF_EQ:
            opc = kMipsBeq;
            break;
        case OP_IF_NE:
            opc = kMipsBne;
            break;
        case OP_IF_LT:
            opc = kMipsBne;
            tReg = dvmCompilerAllocTemp(cUnit);
            test = newLIR3(cUnit, kMipsSlt, tReg, reg1, reg2);
            reg1 = tReg;
            reg2 = r_ZERO;
            break;
        case OP_IF_LE:
            opc = kMipsBeqz;
            tReg = dvmCompilerAllocTemp(cUnit);
            test = newLIR3(cUnit, kMipsSlt, tReg, reg2, reg1);
            reg1 = tReg;
            reg2 = -1;
            break;
        case OP_IF_GT:
            opc = kMipsBne;
            tReg = dvmCompilerAllocTemp(cUnit);
            test = newLIR3(cUnit, kMipsSlt, tReg, reg2, reg1);
            reg1 = tReg;
            reg2 = r_ZERO;
            break;
        case OP_IF_GE:
            opc = kMipsBeqz;
            tReg = dvmCompilerAllocTemp(cUnit);
            test = newLIR3(cUnit, kMipsSlt, tReg, reg1, reg2);
            reg1 = tReg;
            reg2 = -1;
            break;
        default:
            cond = 0;
            LOGE("Unexpected opcode (%d) for Fmt22t\n", dalvikOpCode);
            dvmCompilerAbort(cUnit);
    }

    genConditionalBranchMips(cUnit, opc, reg1, reg2, &labelList[bb->taken->id]);
    /* This mostly likely will be optimized away in a later phase */
    genUnconditionalBranch(cUnit, &labelList[bb->fallThrough->id]);
    return false;
}

static bool handleFmt22x_Fmt32x(CompilationUnit *cUnit, MIR *mir)
{
    OpCode opCode = mir->dalvikInsn.opCode;

    switch (opCode) {
        case OP_MOVE_16:
        case OP_MOVE_OBJECT_16:
        case OP_MOVE_FROM16:
        case OP_MOVE_OBJECT_FROM16: {
            storeValue(cUnit, dvmCompilerGetDest(cUnit, mir, 0),
                       dvmCompilerGetSrc(cUnit, mir, 0));
            break;
        }
        case OP_MOVE_WIDE_16:
        case OP_MOVE_WIDE_FROM16: {
            storeValueWide(cUnit, dvmCompilerGetDestWide(cUnit, mir, 0, 1),
                           dvmCompilerGetSrcWide(cUnit, mir, 0, 1));
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt23x(CompilationUnit *cUnit, MIR *mir)
{
    OpCode opCode = mir->dalvikInsn.opCode;
    RegLocation rlSrc1;
    RegLocation rlSrc2;
    RegLocation rlDest;

    if ((opCode >= OP_ADD_INT) && (opCode <= OP_REM_DOUBLE)) {
        return genArithOp( cUnit, mir );
    }

    /* APUTs have 3 sources and no targets */
    if (mir->ssaRep->numDefs == 0) {
        if (mir->ssaRep->numUses == 3) {
            rlDest = dvmCompilerGetSrc(cUnit, mir, 0);
            rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 1);
            rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 2);
        } else {
            assert(mir->ssaRep->numUses == 4);
            rlDest = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
            rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 2);
            rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 3);
        }
    } else {
        /* Two sources and 1 dest.  Deduce the operand sizes */
        if (mir->ssaRep->numUses == 4) {
            rlSrc1 = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
            rlSrc2 = dvmCompilerGetSrcWide(cUnit, mir, 2, 3);
        } else {
            assert(mir->ssaRep->numUses == 2);
            rlSrc1 = dvmCompilerGetSrc(cUnit, mir, 0);
            rlSrc2 = dvmCompilerGetSrc(cUnit, mir, 1);
        }
        if (mir->ssaRep->numDefs == 2) {
            rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
        } else {
            assert(mir->ssaRep->numDefs == 1);
            rlDest = dvmCompilerGetDest(cUnit, mir, 0);
        }
    }

    switch (opCode) {
        case OP_CMPL_FLOAT:
        case OP_CMPG_FLOAT:
        case OP_CMPL_DOUBLE:
        case OP_CMPG_DOUBLE:
            return genCmpFP(cUnit, mir, rlDest, rlSrc1, rlSrc2);
        case OP_CMP_LONG:
            genCmpLong(cUnit, mir, rlDest, rlSrc1, rlSrc2);
            break;
        case OP_AGET_WIDE:
            genArrayGet(cUnit, mir, kLong, rlSrc1, rlSrc2, rlDest, 3);
            break;
        case OP_AGET:
        case OP_AGET_OBJECT:
            genArrayGet(cUnit, mir, kWord, rlSrc1, rlSrc2, rlDest, 2);
            break;
        case OP_AGET_BOOLEAN:
            genArrayGet(cUnit, mir, kUnsignedByte, rlSrc1, rlSrc2, rlDest, 0);
            break;
        case OP_AGET_BYTE:
            genArrayGet(cUnit, mir, kSignedByte, rlSrc1, rlSrc2, rlDest, 0);
            break;
        case OP_AGET_CHAR:
            genArrayGet(cUnit, mir, kUnsignedHalf, rlSrc1, rlSrc2, rlDest, 1);
            break;
        case OP_AGET_SHORT:
            genArrayGet(cUnit, mir, kSignedHalf, rlSrc1, rlSrc2, rlDest, 1);
            break;
        case OP_APUT_WIDE:
            genArrayPut(cUnit, mir, kLong, rlSrc1, rlSrc2, rlDest, 3);
            break;
        case OP_APUT:
            genArrayPut(cUnit, mir, kWord, rlSrc1, rlSrc2, rlDest, 2);
            break;
        case OP_APUT_OBJECT:
            genArrayObjectPut(cUnit, mir, rlSrc1, rlSrc2, rlDest, 2);
            break;
        case OP_APUT_SHORT:
        case OP_APUT_CHAR:
            genArrayPut(cUnit, mir, kUnsignedHalf, rlSrc1, rlSrc2, rlDest, 1);
            break;
        case OP_APUT_BYTE:
        case OP_APUT_BOOLEAN:
            genArrayPut(cUnit, mir, kUnsignedByte, rlSrc1, rlSrc2, rlDest, 0);
            break;
        default:
            return true;
    }
    return false;
}

/*
 * Find the matching case.
 *
 * return values:
 * r_A0 (low 32-bit): pc of the chaining cell corresponding to the resolved case,
 *    including default which is placed at MIN(size, MAX_CHAINED_SWITCH_CASES).
 * r_A1 (high 32-bit): the branch offset of the matching case (only for indexes
 *    above MAX_CHAINED_SWITCH_CASES).
 *
 * Instructions around the call are:
 *
 * jalr &findPackedSwitchIndex
 * nop
 * lw gp, 84(sp) |
 * addu          | 20 bytes for these 5 instructions
 * move          | (NOTE: if this sequence is shortened or lengthened, then
 * jr            |  the 20 byte offset added below in 3 places must be changed 
 * nop           |  accordingly.)
 * chaining cell for case 0 [16 bytes]
 * chaining cell for case 1 [16 bytes]
 *               :
 * chaining cell for case MIN(size, MAX_CHAINED_SWITCH_CASES)-1 [16 bytes]
 * chaining cell for case default [16 bytes]
 * noChain exit
 */
static s8 findPackedSwitchIndex(const u2* switchData, int testVal)
{
    int size;
    int firstKey;
    const int *entries;
    int index;
    int jumpIndex;
    int caseDPCOffset = 0;

    /*
     * Packed switch data format:
     *  ushort ident = 0x0100   magic value
     *  ushort size             number of entries in the table
     *  int first_key           first (and lowest) switch case value
     *  int targets[size]       branch targets, relative to switch opcode
     *
     * Total size is (4+size*2) 16-bit code units.
     */
    size = switchData[1];
    assert(size > 0);

    firstKey = switchData[2];
    firstKey |= switchData[3] << 16;


    /* The entries are guaranteed to be aligned on a 32-bit boundary;
     * we can treat them as a native int array.
     */
    entries = (const int*) &switchData[4];
    assert(((u4)entries & 0x3) == 0);

    index = testVal - firstKey;

    /* Jump to the default cell */
    if (index < 0 || index >= size) {
        jumpIndex = MIN(size, MAX_CHAINED_SWITCH_CASES);
    /* Jump to the non-chaining exit point */
    } else if (index >= MAX_CHAINED_SWITCH_CASES) {
        jumpIndex = MAX_CHAINED_SWITCH_CASES + 1;
        caseDPCOffset = entries[index];
    /* Jump to the inline chaining cell */
    } else {
        jumpIndex = index;
    }

    return (((s8) caseDPCOffset) << 32) | (u8) (jumpIndex * 16 + 20);
}

/* See comments for findPackedSwitchIndex */
static s8 findSparseSwitchIndex(const u2* switchData, int testVal)
{
    int size;
    const int *keys;
    const int *entries;
    int i;

    /*
     * Sparse switch data format:
     *  ushort ident = 0x0200   magic value
     *  ushort size             number of entries in the table; > 0
     *  int keys[size]          keys, sorted low-to-high; 32-bit aligned
     *  int targets[size]       branch targets, relative to switch opcode
     *
     * Total size is (2+size*4) 16-bit code units.
     */

    size = switchData[1];
    assert(size > 0);

    /* The keys are guaranteed to be aligned on a 32-bit boundary;
     * we can treat them as a native int array.
     */
    keys = (const int*) &switchData[2];
    assert(((u4)keys & 0x3) == 0);

    /* The entries are guaranteed to be aligned on a 32-bit boundary;
     * we can treat them as a native int array.
     */
    entries = keys + size;
    assert(((u4)entries & 0x3) == 0);

    /*
     * Run through the list of keys, which are guaranteed to
     * be sorted low-to-high.
     *
     * Most tables have 3-4 entries.  Few have more than 10.  A binary
     * search here is probably not useful.
     */
    for (i = 0; i < size; i++) {
        int k = keys[i];
        if (k == testVal) {
            /* MAX_CHAINED_SWITCH_CASES + 1 is the start of the overflow case */
            int jumpIndex = (i < MAX_CHAINED_SWITCH_CASES) ?
                           i : MAX_CHAINED_SWITCH_CASES + 1;
            return (((s8) entries[i]) << 32) | (u8) (jumpIndex * 16 + 20);
        } else if (k > testVal) {
            break;
        }
    }
    return MIN(size, MAX_CHAINED_SWITCH_CASES) * 16 + 20;
}

static bool handleFmt31t(CompilationUnit *cUnit, MIR *mir)
{
    OpCode dalvikOpCode = mir->dalvikInsn.opCode;
    switch (dalvikOpCode) {
        case OP_FILL_ARRAY_DATA: {
            RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            // Making a call - use explicit registers
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            genExportPC(cUnit, mir);
            loadValueDirectFixed(cUnit, rlSrc, r_A0);
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)dvmInterpHandleFillArrayData);
            loadConstant(cUnit, r_A1,
               (int) (cUnit->method->insns + mir->offset + mir->dalvikInsn.vB));
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /* generate a branch over if successful */
            MipsLIR *branchOver = opCondBranchMips(cUnit, kMipsBne, r_V0, r_ZERO);
            loadConstant(cUnit, r_A0,
                         (int) (cUnit->method->insns + mir->offset));
            genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branchOver->generic.target = (LIR *) target;
            break;
        }
        /*
         * Compute the goto target of up to
         * MIN(switchSize, MAX_CHAINED_SWITCH_CASES) + 1 chaining cells.
         * See the comment before findPackedSwitchIndex for the code layout.
         */
        case OP_PACKED_SWITCH:
        case OP_SPARSE_SWITCH: {
            RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            loadValueDirectFixed(cUnit, rlSrc, r_A1);
            dvmCompilerLockAllTemps(cUnit);
            const u2 *switchData =
                cUnit->method->insns + mir->offset + mir->dalvikInsn.vB;
            u2 size = switchData[1];

            if (dalvikOpCode == OP_PACKED_SWITCH) {
                LOAD_FUNC_ADDR(cUnit, r_T9, (int)findPackedSwitchIndex);
            } else {
                LOAD_FUNC_ADDR(cUnit, r_T9, (int)findSparseSwitchIndex);
            }
            /* r_A0 <- Addr of the switch data */
            loadConstant(cUnit, r_A0,
               (int) (cUnit->method->insns + mir->offset + mir->dalvikInsn.vB));
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            dvmCompilerClobberCallRegs(cUnit);
            /* pc <- computed goto target using value in RA */
            newLIR3(cUnit, kMipsAddu, r_A0, r_RA, r_V0); 
            newLIR2(cUnit, kMipsMove, r_A1, r_V1);
            newLIR1(cUnit, kMipsJr, r_A0);
            newLIR0(cUnit, kMipsNop); /* for maintaining 20 byte offset */
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt35c_3rc(CompilationUnit *cUnit, MIR *mir, BasicBlock *bb,
                             MipsLIR *labelList)
{
    MipsLIR *retChainingCell = NULL;
    MipsLIR *pcrLabel = NULL;

    if (bb->fallThrough != NULL)
        retChainingCell = &labelList[bb->fallThrough->id];

    DecodedInstruction *dInsn = &mir->dalvikInsn;
    switch (mir->dalvikInsn.opCode) {
        /*
         * calleeMethod = this->clazz->vtable[
         *     method->clazz->pDvmDex->pResMethods[BBBB]->methodIndex
         * ]
         */
        case OP_INVOKE_VIRTUAL:
        case OP_INVOKE_VIRTUAL_RANGE: {
            MipsLIR *predChainingCell = &labelList[bb->taken->id];
            int methodIndex =
                cUnit->method->clazz->pDvmDex->pResMethods[dInsn->vB]->
                methodIndex;

            if (mir->dalvikInsn.opCode == OP_INVOKE_VIRTUAL)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            genInvokeVirtualCommon(cUnit, mir, methodIndex,
                                   retChainingCell,
                                   predChainingCell,
                                   pcrLabel);
            break;
        }
        /*
         * calleeMethod = method->clazz->super->vtable[method->clazz->pDvmDex
         *                ->pResMethods[BBBB]->methodIndex]
         */
        case OP_INVOKE_SUPER:
        case OP_INVOKE_SUPER_RANGE: {
            int mIndex = cUnit->method->clazz->pDvmDex->
                pResMethods[dInsn->vB]->methodIndex;
            const Method *calleeMethod =
                cUnit->method->clazz->super->vtable[mIndex];

            if (mir->dalvikInsn.opCode == OP_INVOKE_SUPER)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            /* r_A0 = calleeMethod */
            loadConstant(cUnit, r_A0, (int) calleeMethod);

            genInvokeSingletonCommon(cUnit, mir, bb, labelList, pcrLabel,
                                     calleeMethod);
            break;
        }
        /* calleeMethod = method->clazz->pDvmDex->pResMethods[BBBB] */
        case OP_INVOKE_DIRECT:
        case OP_INVOKE_DIRECT_RANGE: {
            const Method *calleeMethod =
                cUnit->method->clazz->pDvmDex->pResMethods[dInsn->vB];

            if (mir->dalvikInsn.opCode == OP_INVOKE_DIRECT)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            /* r_A0 = calleeMethod */
            loadConstant(cUnit, r_A0, (int) calleeMethod);

            genInvokeSingletonCommon(cUnit, mir, bb, labelList, pcrLabel,
                                     calleeMethod);
            break;
        }
        /* calleeMethod = method->clazz->pDvmDex->pResMethods[BBBB] */
        case OP_INVOKE_STATIC:
        case OP_INVOKE_STATIC_RANGE: {
            const Method *calleeMethod =
                cUnit->method->clazz->pDvmDex->pResMethods[dInsn->vB];

            if (mir->dalvikInsn.opCode == OP_INVOKE_STATIC)
                genProcessArgsNoRange(cUnit, mir, dInsn,
                                      NULL /* no null check */);
            else
                genProcessArgsRange(cUnit, mir, dInsn,
                                    NULL /* no null check */);

            /* r_A0 = calleeMethod */
            loadConstant(cUnit, r_A0, (int) calleeMethod);

            genInvokeSingletonCommon(cUnit, mir, bb, labelList, pcrLabel,
                                     calleeMethod);
            break;
        }
        /*
         * calleeMethod = dvmFindInterfaceMethodInCache(this->clazz,
         *                    BBBB, method, method->clazz->pDvmDex)
         *
         * The following is an example of generated code for
         *      "invoke-interface v0"
         *
         * -------- dalvik offset: 0x0008 @ invoke-interface v0
         * 0x47357e36 : ldr     r0, [r5, #0]   --+
         * 0x47357e38 : sub     r7,r5,#24        |
         * 0x47357e3c : cmp     r0, #0           | genProcessArgsNoRange
         * 0x47357e3e : beq     0x47357e82       |
         * 0x47357e40 : stmia   r7, <r0>       --+
         * 0x47357e42 : ldr     r4, [pc, #120] --> r4 <- dalvikPC of this invoke
         * 0x47357e44 : add     r1, pc, #64    --> r1 <- &retChainingCell
         * 0x47357e46 : add     r2, pc, #72    --> r2 <- &predictedChainingCell
         * 0x47357e48 : blx_1   0x47348190     --+ TEMPLATE_INVOKE_METHOD_
         * 0x47357e4a : blx_2   see above      --+     PREDICTED_CHAIN
         * 0x47357e4c : b       0x47357e90     --> off to the predicted chain
         * 0x47357e4e : b       0x47357e82     --> punt to the interpreter
         * 0x47357e50 : mov     r8, r1         --+
         * 0x47357e52 : mov     r9, r2           |
         * 0x47357e54 : ldr     r2, [pc, #96]    |
         * 0x47357e56 : mov     r10, r3          |
         * 0x47357e58 : movs    r0, r3           | dvmFindInterfaceMethodInCache
         * 0x47357e5a : ldr     r3, [pc, #88]    |
         * 0x47357e5c : ldr     r7, [pc, #80]    |
         * 0x47357e5e : mov     r1, #1452        |
         * 0x47357e62 : blx     r7             --+
         * 0x47357e64 : cmp     r0, #0         --> calleeMethod == NULL?
         * 0x47357e66 : bne     0x47357e6e     --> branch over the throw if !r0
         * 0x47357e68 : ldr     r0, [pc, #80]  --> load Dalvik PC of the invoke
         * 0x47357e6a : blx_1   0x47348494     --+ TEMPLATE_THROW_EXCEPTION_
         * 0x47357e6c : blx_2   see above      --+     COMMON
         * 0x47357e6e : mov     r1, r8         --> r1 <- &retChainingCell
         * 0x47357e70 : cmp     r1, #0         --> compare against 0
         * 0x47357e72 : bgt     0x47357e7c     --> >=0? don't rechain
         * 0x47357e74 : ldr     r7, [r6, #108] --+
         * 0x47357e76 : mov     r2, r9           | dvmJitToPatchPredictedChain
         * 0x47357e78 : mov     r3, r10          |
         * 0x47357e7a : blx     r7             --+
         * 0x47357e7c : add     r1, pc, #8     --> r1 <- &retChainingCell
         * 0x47357e7e : blx_1   0x4734809c     --+ TEMPLATE_INVOKE_METHOD_NO_OPT
         * 0x47357e80 : blx_2   see above      --+
         * -------- reconstruct dalvik PC : 0x425719dc @ +0x0008
         * 0x47357e82 : ldr     r0, [pc, #56]
         * Exception_Handling:
         * 0x47357e84 : ldr     r1, [r6, #92]
         * 0x47357e86 : blx     r1
         * 0x47357e88 : .align4
         * -------- chaining cell (hot): 0x000b
         * 0x47357e88 : ldr     r0, [r6, #104]
         * 0x47357e8a : blx     r0
         * 0x47357e8c : data    0x19e2(6626)
         * 0x47357e8e : data    0x4257(16983)
         * 0x47357e90 : .align4
         * -------- chaining cell (predicted)
         * 0x47357e90 : data    0xe7fe(59390)  --> will be patched into bx
         * 0x47357e92 : data    0x0000(0)
         * 0x47357e94 : data    0x0000(0)      --> class
         * 0x47357e96 : data    0x0000(0)
         * 0x47357e98 : data    0x0000(0)      --> method
         * 0x47357e9a : data    0x0000(0)
         * 0x47357e9c : data    0x0000(0)      --> rechain count
         * 0x47357e9e : data    0x0000(0)
         * -------- end of chaining cells (0x006c)
         * 0x47357eb0 : .word (0xad03e369)
         * 0x47357eb4 : .word (0x28a90)
         * 0x47357eb8 : .word (0x41a63394)
         * 0x47357ebc : .word (0x425719dc)
         */
        case OP_INVOKE_INTERFACE:
        case OP_INVOKE_INTERFACE_RANGE: {
            MipsLIR *predChainingCell = &labelList[bb->taken->id];
            int methodIndex = dInsn->vB;

            /* Ensure that nothing is both live and dirty */
            dvmCompilerFlushAllRegs(cUnit);

            if (mir->dalvikInsn.opCode == OP_INVOKE_INTERFACE)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            /* "this" is already left in r_A0 by genProcessArgs* */

            /* r4PC = dalvikCallsite */
            loadConstant(cUnit, r4PC,
                         (int) (cUnit->method->insns + mir->offset));

            /* r_A1 = &retChainingCell */
            MipsLIR *addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
            addrRetChain->generic.target = (LIR *) retChainingCell;
            addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
            addrRetChain->generic.target = (LIR *) retChainingCell;


            /* r_A2 = &predictedChainingCell */
            MipsLIR *predictedChainingCell = newLIR2(cUnit, kMipsLahi, r_A2, 0);
            predictedChainingCell->generic.target = (LIR *) predChainingCell;
            predictedChainingCell = newLIR3(cUnit, kMipsLalo, r_A2, r_A2, 0);
            predictedChainingCell->generic.target = (LIR *) predChainingCell;

            genDispatchToHandler(cUnit, TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN);

            /* return through lr - jump to the chaining cell */
            genUnconditionalBranch(cUnit, predChainingCell);

            /*
             * null-check on "this" may have been eliminated, but we still need
             * a PC-reconstruction label for stack overflow bailout.
             */
            if (pcrLabel == NULL) {
                int dPC = (int) (cUnit->method->insns + mir->offset);
                pcrLabel = dvmCompilerNew(sizeof(MipsLIR), true);
                pcrLabel->opCode = kMipsPseudoPCReconstructionCell;
                pcrLabel->operands[0] = dPC;
                pcrLabel->operands[1] = mir->offset;
                /* Insert the place holder to the growable list */
                dvmInsertGrowableList(&cUnit->pcReconstructionList, pcrLabel);
            }

            /* return through lr+8 - punt to the interpreter */
            genUnconditionalBranch(cUnit, pcrLabel);

            /*
             * return through lr+16 - fully resolve the callee method.
             * r_A1 <- count
             * r_A2 <- &predictedChainCell
             * r_A3 <- this->class
             * r4 <- dPC
             * r_S4 <- this->class->vtable
             */

            /* Save count, &predictedChainCell, and class to high regs first */
            genRegCopy(cUnit, r_S5, r_A1);
            genRegCopy(cUnit, r_S6, r_A2);
            genRegCopy(cUnit, r_S7, r_A3);

            /* r_A0 now contains this->clazz */
            genRegCopy(cUnit, r_A0, r_A3);

            /* r_A1 = BBBB */
            loadConstant(cUnit, r_A1, dInsn->vB);

            /* r_A2 = method (caller) */
            loadConstant(cUnit, r_A2, (int) cUnit->method);

            /* r_A3 = pDvmDex */
            loadConstant(cUnit, r_A3, (int) cUnit->method->clazz->pDvmDex);

            LOAD_FUNC_ADDR(cUnit, r_T9,
                           (intptr_t) dvmFindInterfaceMethodInCache);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            /* r_V0 = calleeMethod (returned from dvmFindInterfaceMethodInCache */
            genRegCopy(cUnit, r_A0, r_V0);

            dvmCompilerClobberCallRegs(cUnit);
            /* generate a branch over if the interface method is resolved */
            MipsLIR *branchOver = opCondBranchMips(cUnit, kMipsBne, r_A0, r_ZERO);
            /*
             * calleeMethod == NULL -> throw
             */
            loadConstant(cUnit, r_A0,
                         (int) (cUnit->method->insns + mir->offset));
            genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
            /* noreturn */

            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branchOver->generic.target = (LIR *) target;

            genRegCopy(cUnit, r_A1, r_S5);

            /* Check if rechain limit is reached */
            MipsLIR *bypassRechaining = opCondBranchMips(cUnit, kMipsBgtz, r_A1, -1);

            loadWordDisp(cUnit, rGLUE, offsetof(InterpState,
                         jitToInterpEntries.dvmJitToPatchPredictedChain), r_T9);

            genRegCopy(cUnit, r_A2, r_S6);
            genRegCopy(cUnit, r_A3, r_S7);

            /*
             * r_A0 = calleeMethod
             * r_A2 = &predictedChainingCell
             * r_A3 = class
             *
             * &returnChainingCell has been loaded into r_A1 but is not needed
             * when patching the chaining cell and will be clobbered upon
             * returning so it will be reconstructed again.
             */
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);
            genRegCopy(cUnit, r_A0, r_V0);

            /* r_A1 = &retChainingCell */
            addrRetChain = newLIR2(cUnit, kMipsLahi, r_A1, 0);
            addrRetChain->generic.target = (LIR *) retChainingCell;
            bypassRechaining->generic.target = (LIR *) addrRetChain;
            addrRetChain = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
            addrRetChain->generic.target = (LIR *) retChainingCell;


            /*
             * r_A0 = this, r_A1 = calleeMethod,
             * r_A1 = &ChainingCell,
             * r4PC = callsiteDPC,
             */
            genDispatchToHandler(cUnit, TEMPLATE_INVOKE_METHOD_NO_OPT);
#if defined(JIT_STATS)
            gDvmJit.invokePolymorphic++;
#endif
            /* Handle exceptions using the interpreter */
            genTrap(cUnit, mir->offset, pcrLabel);
            break;
        }
        /* NOP */
        case OP_INVOKE_DIRECT_EMPTY: {
            return false;
        }
        case OP_FILLED_NEW_ARRAY:
        case OP_FILLED_NEW_ARRAY_RANGE: {
            /* Just let the interpreter deal with these */
            genInterpSingleStep(cUnit, mir);
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt35ms_3rms(CompilationUnit *cUnit, MIR *mir,
                               BasicBlock *bb, MipsLIR *labelList)
{
    MipsLIR *retChainingCell = &labelList[bb->fallThrough->id];
    MipsLIR *predChainingCell = &labelList[bb->taken->id];
    MipsLIR *pcrLabel = NULL;

    DecodedInstruction *dInsn = &mir->dalvikInsn;
    switch (mir->dalvikInsn.opCode) {
        /* calleeMethod = this->clazz->vtable[BBBB] */
        case OP_INVOKE_VIRTUAL_QUICK_RANGE:
        case OP_INVOKE_VIRTUAL_QUICK: {
            int methodIndex = dInsn->vB;
            if (mir->dalvikInsn.opCode == OP_INVOKE_VIRTUAL_QUICK)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            genInvokeVirtualCommon(cUnit, mir, methodIndex,
                                   retChainingCell,
                                   predChainingCell,
                                   pcrLabel);
            break;
        }
        /* calleeMethod = method->clazz->super->vtable[BBBB] */
        case OP_INVOKE_SUPER_QUICK:
        case OP_INVOKE_SUPER_QUICK_RANGE: {
            const Method *calleeMethod =
                cUnit->method->clazz->super->vtable[dInsn->vB];

            if (mir->dalvikInsn.opCode == OP_INVOKE_SUPER_QUICK)
                genProcessArgsNoRange(cUnit, mir, dInsn, &pcrLabel);
            else
                genProcessArgsRange(cUnit, mir, dInsn, &pcrLabel);

            /* r_A0 = calleeMethod */
            loadConstant(cUnit, r_A0, (int) calleeMethod);

            genInvokeSingletonCommon(cUnit, mir, bb, labelList, pcrLabel,
                                     calleeMethod);
            /* Handle exceptions using the interpreter */ /* duplicate genTrap? */
            genTrap(cUnit, mir->offset, pcrLabel);
            break;
        }
        default:
            return true;
    }
    return false;
}

/*
 * This operation is complex enough that we'll do it partly inline
 * and partly with a handler.  NOTE: the handler uses hardcoded
 * values for string object offsets and must be revisitied if the
 * layout changes.
 */
static bool genInlinedCompareTo(CompilationUnit *cUnit, MIR *mir)
{
#if defined(USE_GLOBAL_STRING_DEFS)
    return false;
#else
    MipsLIR *rollback;
    RegLocation rlThis = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlComp = dvmCompilerGetSrc(cUnit, mir, 1);

    loadValueDirectFixed(cUnit, rlThis, r_A0);
    loadValueDirectFixed(cUnit, rlComp, r_A1);
    /* Test objects for NULL */
    rollback = genNullCheck(cUnit, rlThis.sRegLow, r_A0, mir->offset, NULL);
    genNullCheck(cUnit, rlComp.sRegLow, r_A1, mir->offset, rollback);
    /*
     * TUNING: we could check for object pointer equality before invoking
     * handler. Unclear whether the gain would be worth the added code size
     * expansion.
     */
    genDispatchToHandler(cUnit, TEMPLATE_STRING_COMPARETO);
    storeValue(cUnit, inlinedTarget(cUnit, mir, false),
               dvmCompilerGetReturn(cUnit));
    return true;
#endif
}

static bool genInlinedIndexOf(CompilationUnit *cUnit, MIR *mir, bool singleI)
{
#if defined(USE_GLOBAL_STRING_DEFS)
    return false;
#else
    RegLocation rlThis = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlChar = dvmCompilerGetSrc(cUnit, mir, 1);

    loadValueDirectFixed(cUnit, rlThis, r_A0);
    loadValueDirectFixed(cUnit, rlChar, r_A1);
    if (!singleI) {
        RegLocation rlStart = dvmCompilerGetSrc(cUnit, mir, 2);
        loadValueDirectFixed(cUnit, rlStart, r_A2);
    } else {
        loadConstant(cUnit, r_A2, 0);
    }
    /* Test objects for NULL */
    genNullCheck(cUnit, rlThis.sRegLow, r_A0, mir->offset, NULL);
    genDispatchToHandler(cUnit, TEMPLATE_STRING_INDEXOF);
    storeValue(cUnit, inlinedTarget(cUnit, mir, false),
               dvmCompilerGetReturn(cUnit));
    return true;
#endif
}

static bool genInlinedStringLength(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlDest = inlinedTarget(cUnit, mir, false);
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg, mir->offset, NULL);
    loadWordDisp(cUnit, rlObj.lowReg, gDvm.offJavaLangString_count,
                 rlResult.lowReg);
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool genInlinedStringCharAt(CompilationUnit *cUnit, MIR *mir)
{
    int contents = offsetof(ArrayObject, contents);
    RegLocation rlObj = dvmCompilerGetSrc(cUnit, mir, 0);
    RegLocation rlIdx = dvmCompilerGetSrc(cUnit, mir, 1);
    RegLocation rlDest = inlinedTarget(cUnit, mir, false);
    RegLocation rlResult;
    rlObj = loadValue(cUnit, rlObj, kCoreReg);
    rlIdx = loadValue(cUnit, rlIdx, kCoreReg);
    int regMax = dvmCompilerAllocTemp(cUnit);
    int regOff = dvmCompilerAllocTemp(cUnit);
    int regPtr = dvmCompilerAllocTemp(cUnit);
    MipsLIR *pcrLabel = genNullCheck(cUnit, rlObj.sRegLow, rlObj.lowReg,
                                    mir->offset, NULL);
    loadWordDisp(cUnit, rlObj.lowReg, gDvm.offJavaLangString_count, regMax);
    loadWordDisp(cUnit, rlObj.lowReg, gDvm.offJavaLangString_offset, regOff);
    loadWordDisp(cUnit, rlObj.lowReg, gDvm.offJavaLangString_value, regPtr);
    genBoundsCheck(cUnit, rlIdx.lowReg, regMax, mir->offset, pcrLabel);
    dvmCompilerFreeTemp(cUnit, regMax);
    opRegImm(cUnit, kOpAdd, regPtr, contents);
    opRegReg(cUnit, kOpAdd, regOff, rlIdx.lowReg);
    rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    loadBaseIndexed(cUnit, regPtr, regOff, rlResult.lowReg, 1, kUnsignedHalf);
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool genInlinedAbsInt(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    rlSrc = loadValue(cUnit, rlSrc, kCoreReg);
    RegLocation rlDest = inlinedTarget(cUnit, mir, false);;
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    int signReg = dvmCompilerAllocTemp(cUnit);
    /*
     * abs(x) = y<=x>>31, (x+y)^y.
     * Thumb2's IT block also yields 3 instructions, but imposes
     * scheduling constraints.
     */
    opRegRegImm(cUnit, kOpAsr, signReg, rlSrc.lowReg, 31);
    opRegRegReg(cUnit, kOpAdd, rlResult.lowReg, rlSrc.lowReg, signReg);
    opRegReg(cUnit, kOpXor, rlResult.lowReg, signReg);
    storeValue(cUnit, rlDest, rlResult);
    return false;
}

static bool genInlinedAbsLong(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    RegLocation rlDest = inlinedTargetWide(cUnit, mir, false);
    rlSrc = loadValueWide(cUnit, rlSrc, kCoreReg);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    int signReg = dvmCompilerAllocTemp(cUnit);
    int tReg = dvmCompilerAllocTemp(cUnit);
    /*
     * abs(x) = y<=x>>31, (x+y)^y.
     * Thumb2 IT block allows slightly shorter sequence,
     * but introduces a scheduling barrier.  Stick with this
     * mechanism for now.
     */
    opRegRegImm(cUnit, kOpAsr, signReg, rlSrc.highReg, 31);
    opRegRegReg(cUnit, kOpAdd, rlResult.lowReg, rlSrc.lowReg, signReg);
    newLIR3(cUnit, kMipsSltu, tReg, rlResult.lowReg, signReg);
    opRegRegReg(cUnit, kOpAdd, rlResult.highReg, rlSrc.highReg, signReg);
    opRegRegReg(cUnit, kOpAdd, rlResult.highReg, rlResult.highReg, tReg);
    opRegReg(cUnit, kOpXor, rlResult.lowReg, signReg);
    opRegReg(cUnit, kOpXor, rlResult.highReg, signReg);
    dvmCompilerFreeTemp(cUnit, signReg);
    dvmCompilerFreeTemp(cUnit, tReg);
    storeValueWide(cUnit, rlDest, rlResult);
    return false;
}

/*
 * NOTE: Handles both range and non-range versions (arguments
 * have already been normalized by this point).
 */
static bool handleExecuteInline(CompilationUnit *cUnit, MIR *mir)
{
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    switch( mir->dalvikInsn.opCode) {
        case OP_EXECUTE_INLINE_RANGE:
        case OP_EXECUTE_INLINE: {
            unsigned int i;
            const InlineOperation* inLineTable = dvmGetInlineOpsTable();
            int offset = offsetof(InterpState, retval);
            int operation = dInsn->vB;
            int tReg1;
            int tReg2;
            switch (operation) {
                case INLINE_EMPTYINLINEMETHOD:
                    return false;  /* Nop */
                case INLINE_STRING_LENGTH:
                    return genInlinedStringLength(cUnit, mir);
                case INLINE_MATH_ABS_INT:
                    return genInlinedAbsInt(cUnit, mir);
                case INLINE_MATH_ABS_LONG:
                    return genInlinedAbsLong(cUnit, mir);
                case INLINE_MATH_MIN_INT:
                    return genInlinedMinMaxInt(cUnit, mir, true);
                case INLINE_MATH_MAX_INT:
                    return genInlinedMinMaxInt(cUnit, mir, false);
                case INLINE_STRING_CHARAT:
                    return genInlinedStringCharAt(cUnit, mir);
                case INLINE_MATH_SQRT:
                    if (genInlineSqrt(cUnit, mir))
                        return false;
                    else
                        break;   /* Handle with C routine */
                case INLINE_MATH_ABS_FLOAT:
                    if (genInlinedAbsFloat(cUnit, mir))
                        return false;
                    else
                        break;
                case INLINE_MATH_ABS_DOUBLE:
                    if (genInlinedAbsDouble(cUnit, mir))
                        return false;
                    else
                        break;
                case INLINE_STRING_COMPARETO:
                    if (genInlinedCompareTo(cUnit, mir))
                        return false;
                    else
                        break;
                case INLINE_STRING_INDEXOF_I:
                    if (genInlinedIndexOf(cUnit, mir, true /* I */))
                        return false;
                    else
                        break;
                case INLINE_STRING_INDEXOF_II:
                    if (genInlinedIndexOf(cUnit, mir, false /* I */))
                        return false;
                    else
                        break;
                case INLINE_STRING_EQUALS:
                case INLINE_MATH_COS:
                case INLINE_MATH_SIN:
                    break;   /* Handle with C routine */
                default:
                    dvmCompilerAbort(cUnit);
            }

            dvmCompilerFlushAllRegs(cUnit);   /* Everything to home location */
            dvmCompilerClobberCallRegs(cUnit);
            dvmCompilerClobber(cUnit, r4PC);
            dvmCompilerClobber(cUnit, rINST);
            opRegRegImm(cUnit, kOpAdd, r4PC, rGLUE, offset);
            newLIR3(cUnit, kMipsSw, r4PC, 16, r_SP); /* sp has plenty of space */

            genExportPC(cUnit, mir);
            assert(dInsn->vA <= 4);
            for (i=0; i < dInsn->vA; i++) {
                loadValueDirect(cUnit, dvmCompilerGetSrc(cUnit, mir, i), i+r_A0);
            }
            LOAD_FUNC_ADDR(cUnit, r_T9, (int)inLineTable[operation].func);
            opReg(cUnit, kOpBlx, r_T9);
            newLIR3(cUnit, kMipsLw, r_GP, STACK_OFFSET_GP, r_SP);

            MipsLIR *branchOver = opCondBranchMips(cUnit, kMipsBne, r_V0, r_ZERO);
            loadConstant(cUnit, r_A0,
                         (int) (cUnit->method->insns + mir->offset));
            genDispatchToHandler(cUnit, TEMPLATE_THROW_EXCEPTION_COMMON);
            MipsLIR *target = newLIR0(cUnit, kMipsPseudoTargetLabel);
            target->defMask = ENCODE_ALL;
            branchOver->generic.target = (LIR *) target;
            break;
        }
        default:
            return true;
    }
    return false;
}

static bool handleFmt51l(CompilationUnit *cUnit, MIR *mir)
{
    //TUNING: We're using core regs here - not optimal when target is a double
    RegLocation rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    RegLocation rlResult = dvmCompilerEvalLoc(cUnit, rlDest, kCoreReg, true);
    loadConstantNoClobber(cUnit, rlResult.lowReg,
                          mir->dalvikInsn.vB_wide & 0xFFFFFFFFUL);
    loadConstantNoClobber(cUnit, rlResult.highReg,
                          (mir->dalvikInsn.vB_wide>>32) & 0xFFFFFFFFUL);
    storeValueWide(cUnit, rlDest, rlResult);
    return false;
}

/*
 * The following are special processing routines that handle transfer of
 * controls between compiled code and the interpreter. Certain VM states like
 * Dalvik PC and special-purpose registers are reconstructed here.
 */

/* Chaining cell for code that may need warmup. */
static void handleNormalChainingCell(CompilationUnit *cUnit,
                                     unsigned int offset)
{
    newLIR3(cUnit, kMipsLw, r_A0, 
        offsetof(InterpState, jitToInterpEntries.dvmJitToInterpNormal),
        rGLUE);
    newLIR2(cUnit, kMipsJalr, r_RA, r_A0);
    addWordData(cUnit, (int) (cUnit->method->insns + offset), true);
}

/*
 * Chaining cell for instructions that immediately following already translated
 * code.
 */
static void handleHotChainingCell(CompilationUnit *cUnit,
                                  unsigned int offset)
{
    newLIR3(cUnit, kMipsLw, r_A0, 
        offsetof(InterpState, jitToInterpEntries.dvmJitToInterpTraceSelect),
        rGLUE);
    newLIR2(cUnit, kMipsJalr, r_RA, r_A0);
    addWordData(cUnit, (int) (cUnit->method->insns + offset), true);
}

#if defined(WITH_SELF_VERIFICATION) || defined(WITH_JIT_TUNING)
/* Chaining cell for branches that branch back into the same basic block */
static void handleBackwardBranchChainingCell(CompilationUnit *cUnit,
                                             unsigned int offset)
{
    /*
     * Use raw instruction constructors to guarantee that the generated
     * instructions fit the predefined cell size.
     */
#if defined(WITH_SELF_VERIFICATION)
    newLIR3(cUnit, kMipsLw, r_A0, 
        offsetof(InterpState, jitToInterpEntries.dvmJitToInterpBackwardBranch),
        rGLUE);
#else
    newLIR3(cUnit, kMipsLw, r_A0,
        offsetof(InterpState, jitToInterpEntries.dvmJitToInterpNormal),
        rGLUE);
#endif
    newLIR2(cUnit, kMipsJalr, r_RA, r_A0);
    addWordData(cUnit, (int) (cUnit->method->insns + offset), true);
}

#endif
/* Chaining cell for monomorphic method invocations. */
static void handleInvokeSingletonChainingCell(CompilationUnit *cUnit,
                                              const Method *callee)
{
    newLIR3(cUnit, kMipsLw, r_A0, 
        offsetof(InterpState, jitToInterpEntries.dvmJitToInterpTraceSelect),
        rGLUE);
    newLIR2(cUnit, kMipsJalr, r_RA, r_A0);
    addWordData(cUnit, (int) (callee->insns), true);
}

/* Chaining cell for monomorphic method invocations. */
static void handleInvokePredictedChainingCell(CompilationUnit *cUnit)
{
    /* Should not be executed in the initial state */
    addWordData(cUnit, PREDICTED_CHAIN_BX_PAIR_INIT, true);
    /* branch delay slot nop */ 
    addWordData(cUnit, PREDICTED_CHAIN_DELAY_SLOT_INIT, true);
    /* To be filled: class */
    addWordData(cUnit, PREDICTED_CHAIN_CLAZZ_INIT, true);
    /* To be filled: method */
    addWordData(cUnit, PREDICTED_CHAIN_METHOD_INIT, true);
    /*
     * Rechain count. The initial value of 0 here will trigger chaining upon
     * the first invocation of this callsite.
     */
    addWordData(cUnit, PREDICTED_CHAIN_COUNTER_INIT, true);
}

/* Load the Dalvik PC into a0 and jump to the specified target */
static void handlePCReconstruction(CompilationUnit *cUnit,
                                   MipsLIR *targetLabel)
{
    MipsLIR **pcrLabel =
        (MipsLIR **) cUnit->pcReconstructionList.elemList;
    int numElems = cUnit->pcReconstructionList.numUsed;
    int i;
    for (i = 0; i < numElems; i++) {
        dvmCompilerAppendLIR(cUnit, (LIR *) pcrLabel[i]);
        /* a0 = dalvik PC */
        loadConstant(cUnit, r_A0, pcrLabel[i]->operands[0]);
        genUnconditionalBranch(cUnit, targetLabel);
    }
}

static char *extendedMIROpNames[kMirOpLast - kMirOpFirst] = {
    "kMirOpPhi",
    "kMirOpNullNRangeUpCheck",
    "kMirOpNullNRangeDownCheck",
    "kMirOpLowerBound",
    "kMirOpPunt",
};

/*
 * vA = arrayReg;
 * vB = idxReg;
 * vC = endConditionReg;
 * arg[0] = maxC
 * arg[1] = minC
 * arg[2] = loopBranchConditionCode
 */
static void genHoistedChecksForCountUpLoop(CompilationUnit *cUnit, MIR *mir)
{
    /*
     * NOTE: these synthesized blocks don't have ssa names assigned
     * for Dalvik registers.  However, because they dominate the following
     * blocks we can simply use the Dalvik name w/ subscript 0 as the
     * ssa name.
     */
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    const int lenOffset = offsetof(ArrayObject, length);
    const int maxC = dInsn->arg[0];
    const int minC = dInsn->arg[1];
    int regLength;
    RegLocation rlArray = cUnit->regLocation[mir->dalvikInsn.vA];
    RegLocation rlIdxEnd = cUnit->regLocation[mir->dalvikInsn.vC];

    /* regArray <- arrayRef */
    rlArray = loadValue(cUnit, rlArray, kCoreReg);
    rlIdxEnd = loadValue(cUnit, rlIdxEnd, kCoreReg);
    genRegImmCheck(cUnit, kMipsCondEq, rlArray.lowReg, 0, 0,
                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);

    /* regLength <- len(arrayRef) */
    regLength = dvmCompilerAllocTemp(cUnit);
    loadWordDisp(cUnit, rlArray.lowReg, lenOffset, regLength);

    int delta = maxC;
    /*
     * If the loop end condition is ">=" instead of ">", then the largest value
     * of the index is "endCondition - 1".
     */
    if (dInsn->arg[2] == OP_IF_GE) {
        delta--;
    }

    if (delta) {
        int tReg = dvmCompilerAllocTemp(cUnit);
        opRegRegImm(cUnit, kOpAdd, tReg, rlIdxEnd.lowReg, delta);
        rlIdxEnd.lowReg = tReg;
        dvmCompilerFreeTemp(cUnit, tReg);
    }
    /* Punt if "regIdxEnd < len(Array)" is false */
    genRegRegCheck(cUnit, kMipsCondGe, rlIdxEnd.lowReg, regLength, 0,
                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);
}

/*
 * vA = arrayReg;
 * vB = idxReg;
 * vC = endConditionReg;
 * arg[0] = maxC
 * arg[1] = minC
 * arg[2] = loopBranchConditionCode
 */
static void genHoistedChecksForCountDownLoop(CompilationUnit *cUnit, MIR *mir)
{
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    const int lenOffset = offsetof(ArrayObject, length);
    const int regLength = dvmCompilerAllocTemp(cUnit);
    const int maxC = dInsn->arg[0];
    const int minC = dInsn->arg[1];
    RegLocation rlArray = cUnit->regLocation[mir->dalvikInsn.vA];
    RegLocation rlIdxInit = cUnit->regLocation[mir->dalvikInsn.vB];

    /* regArray <- arrayRef */
    rlArray = loadValue(cUnit, rlArray, kCoreReg);
    rlIdxInit = loadValue(cUnit, rlIdxInit, kCoreReg);
    genRegImmCheck(cUnit, kMipsCondEq, rlArray.lowReg, 0, 0,
                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);

    /* regLength <- len(arrayRef) */
    loadWordDisp(cUnit, rlArray.lowReg, lenOffset, regLength);

    if (maxC) {
        int tReg = dvmCompilerAllocTemp(cUnit);
        opRegRegImm(cUnit, kOpAdd, tReg, rlIdxInit.lowReg, maxC);
        rlIdxInit.lowReg = tReg;
        dvmCompilerFreeTemp(cUnit, tReg);
    }

    /* Punt if "regIdxInit < len(Array)" is false */
    genRegRegCheck(cUnit, kMipsCondGe, rlIdxInit.lowReg, regLength, 0,
                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);
}

/*
 * vA = idxReg;
 * vB = minC;
 */
static void genHoistedLowerBoundCheck(CompilationUnit *cUnit, MIR *mir)
{
    DecodedInstruction *dInsn = &mir->dalvikInsn;
    const int minC = dInsn->vB;
    RegLocation rlIdx = cUnit->regLocation[mir->dalvikInsn.vA];

    /* regIdx <- initial index value */
    rlIdx = loadValue(cUnit, rlIdx, kCoreReg);

    /* Punt if "regIdxInit + minC >= 0" is false */
    genRegImmCheck(cUnit, kMipsCondLt, rlIdx.lowReg, -minC, 0,
                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);
}

/* Extended MIR instructions like PHI */
static void handleExtendedMIR(CompilationUnit *cUnit, MIR *mir)
{
    int opOffset = mir->dalvikInsn.opCode - kMirOpFirst;
    char *msg = dvmCompilerNew(strlen(extendedMIROpNames[opOffset]) + 1,
                               false);
    strcpy(msg, extendedMIROpNames[opOffset]);
    newLIR1(cUnit, kMipsPseudoExtended, (int) msg);

    switch (mir->dalvikInsn.opCode) {
        case kMirOpPhi: {
            char *ssaString = dvmCompilerGetSSAString(cUnit, mir->ssaRep);
            newLIR1(cUnit, kMipsPseudoSSARep, (int) ssaString);
            break;
        }
        case kMirOpNullNRangeUpCheck: {
            genHoistedChecksForCountUpLoop(cUnit, mir);
            break;
        }
        case kMirOpNullNRangeDownCheck: {
            genHoistedChecksForCountDownLoop(cUnit, mir);
            break;
        }
        case kMirOpLowerBound: {
            genHoistedLowerBoundCheck(cUnit, mir);
            break;
        }
        case kMirOpPunt: {
            genUnconditionalBranch(cUnit,
                                   (MipsLIR *) cUnit->loopAnalysis->branchToPCR);
            break;
        }
        default:
            break;
    }
}

/*
 * Create a PC-reconstruction cell for the starting offset of this trace.
 * Since the PCR cell is placed near the end of the compiled code which is
 * usually out of range for a conditional branch, we put two branches (one
 * branch over to the loop body and one layover branch to the actual PCR) at the
 * end of the entry block.
 */
static void setupLoopEntryBlock(CompilationUnit *cUnit, BasicBlock *entry,
                                MipsLIR *bodyLabel)
{
    /* Set up the place holder to reconstruct this Dalvik PC */
    MipsLIR *pcrLabel = dvmCompilerNew(sizeof(MipsLIR), true);
    pcrLabel->opCode = kMipsPseudoPCReconstructionCell;
    pcrLabel->operands[0] =
        (int) (cUnit->method->insns + entry->startOffset);
    pcrLabel->operands[1] = entry->startOffset;
    /* Insert the place holder to the growable list */
    dvmInsertGrowableList(&cUnit->pcReconstructionList, pcrLabel);

    /*
     * Next, create two branches - one branch over to the loop body and the
     * other branch to the PCR cell to punt.
     */
    MipsLIR *branchToBody = dvmCompilerNew(sizeof(MipsLIR), true);
    branchToBody->opCode = kMipsB;
    branchToBody->generic.target = (LIR *) bodyLabel;
    setupResourceMasks(branchToBody);
    cUnit->loopAnalysis->branchToBody = (LIR *) branchToBody;

    MipsLIR *branchToPCR = dvmCompilerNew(sizeof(MipsLIR), true);
    branchToPCR->opCode = kMipsB;
    branchToPCR->generic.target = (LIR *) pcrLabel;
    setupResourceMasks(branchToPCR);
    cUnit->loopAnalysis->branchToPCR = (LIR *) branchToPCR;
}

#if defined(WITH_SELF_VERIFICATION)
static bool selfVerificationPuntOps(MIR *mir)
{
assert(0); /* MIPSTODO port selfVerificationPuntOps() */
    DecodedInstruction *decInsn = &mir->dalvikInsn;
    OpCode op = decInsn->opCode;
    int flags =  dexGetInstrFlags(gDvm.instrFlags, op);
    /*
     * All opcodes that can throw exceptions and use the
     * TEMPLATE_THROW_EXCEPTION_COMMON template should be excluded in the trace
     * under self-verification mode.
     */
    return (op == OP_MONITOR_ENTER || op == OP_MONITOR_EXIT ||
            op == OP_NEW_INSTANCE || op == OP_NEW_ARRAY ||
            op == OP_CHECK_CAST || op == OP_MOVE_EXCEPTION ||
            op == OP_FILL_ARRAY_DATA || op == OP_EXECUTE_INLINE ||
            op == OP_EXECUTE_INLINE_RANGE ||
            (flags & kInstrInvoke));
}
#endif

void dvmCompilerMIR2LIR(CompilationUnit *cUnit)
{
    /* Used to hold the labels of each block */
    MipsLIR *labelList =
        dvmCompilerNew(sizeof(MipsLIR) * cUnit->numBlocks, true);
    GrowableList chainingListByType[kChainingCellGap];
    int i;

    /*
     * Initialize various types chaining lists.
     */
    for (i = 0; i < kChainingCellGap; i++) {
        dvmInitGrowableList(&chainingListByType[i], 2);
    }

    BasicBlock **blockList = cUnit->blockList;

    if (cUnit->executionCount) {
        /*
         * Reserve 8 bytes at the beginning of the trace
         *        +----------------------------+
         *        | execution count (4 bytes)  |
         *        +----------------------------+
         *        | chain cell offset (4 bytes)|
         *        +----------------------------+
         * ...and then code to increment the execution
         * count:
         *       mov   r0, pc       @ move adr of "mov r0,pc" + 4 to r0
         *       sub   r0, #10      @ back up to addr of executionCount
         *       ldr   r1, [r0]
         *       add   r1, #1
         *       str   r1, [r0]
         */
        MipsLIR *executionCount = newLIR1(cUnit, kMips32BitData, 0);
        cUnit->chainCellOffsetLIR =
            (LIR *) newLIR1(cUnit, kMips32BitData, CHAIN_CELL_OFFSET_TAG);
        cUnit->headerSize = 8;
        MipsLIR *loadAddr = newLIR2(cUnit, kMipsLahi, r_A1, 0);
        loadAddr->generic.target = (LIR *) executionCount;
        loadAddr = newLIR3(cUnit, kMipsLalo, r_A1, r_A1, 0);
        loadAddr ->generic.target = (LIR *) executionCount;
        newLIR3(cUnit, kMipsLw, r_A0, 0, r_A1);
        newLIR3(cUnit, kMipsAddiu, r_A0, r_A0, 1); 
        newLIR3(cUnit, kMipsSw, r_A0, 0, r_A1);
    } else {
         /* Just reserve 4 bytes for the chain cell offset */
        cUnit->chainCellOffsetLIR =
            (LIR *) newLIR1(cUnit, kMips32BitData, CHAIN_CELL_OFFSET_TAG);
        cUnit->headerSize = 4;
    }

    /* Handle the content in each basic block */
    for (i = 0; i < cUnit->numBlocks; i++) {
        blockList[i]->visited = true;
        MIR *mir;

        labelList[i].operands[0] = blockList[i]->startOffset;

        if (blockList[i]->blockType >= kChainingCellGap) {
            /*
             * Append the label pseudo LIR first. Chaining cells will be handled
             * separately afterwards.
             */
            dvmCompilerAppendLIR(cUnit, (LIR *) &labelList[i]);
        }

        if (blockList[i]->blockType == kEntryBlock) {
            labelList[i].opCode = kMipsPseudoEntryBlock;
            if (blockList[i]->firstMIRInsn == NULL) {
                continue;
            } else {
              setupLoopEntryBlock(cUnit, blockList[i],
                                  &labelList[blockList[i]->fallThrough->id]);
            }
        } else if (blockList[i]->blockType == kExitBlock) {
            labelList[i].opCode = kMipsPseudoExitBlock;
            goto gen_fallthrough;
        } else if (blockList[i]->blockType == kDalvikByteCode) {
            labelList[i].opCode = kMipsPseudoNormalBlockLabel;
            /* Reset the register state */
            dvmCompilerResetRegPool(cUnit);
            dvmCompilerClobberAllRegs(cUnit);
            dvmCompilerResetNullCheck(cUnit);
        } else {
            switch (blockList[i]->blockType) {
                case kChainingCellNormal:
                    labelList[i].opCode = kMipsPseudoChainingCellNormal;
                    /* handle the codegen later */
                    dvmInsertGrowableList(
                        &chainingListByType[kChainingCellNormal], (void *) i);
                    break;
                case kChainingCellInvokeSingleton:
                    labelList[i].opCode =
                        kMipsPseudoChainingCellInvokeSingleton;
                    labelList[i].operands[0] =
                        (int) blockList[i]->containingMethod;
                    /* handle the codegen later */
                    dvmInsertGrowableList(
                        &chainingListByType[kChainingCellInvokeSingleton],
                        (void *) i);
                    break;
                case kChainingCellInvokePredicted:
                    labelList[i].opCode =
                        kMipsPseudoChainingCellInvokePredicted;
                    /* handle the codegen later */
                    dvmInsertGrowableList(
                        &chainingListByType[kChainingCellInvokePredicted],
                        (void *) i);
                    break;
                case kChainingCellHot:
                    labelList[i].opCode =
                        kMipsPseudoChainingCellHot;
                    /* handle the codegen later */
                    dvmInsertGrowableList(
                        &chainingListByType[kChainingCellHot],
                        (void *) i);
                    break;
                case kPCReconstruction:
                    /* Make sure exception handling block is next */
                    labelList[i].opCode =
                        kMipsPseudoPCReconstructionBlockLabel;
                    assert (i == cUnit->numBlocks - 2);
                    handlePCReconstruction(cUnit, &labelList[i+1]);
                    break;
                case kExceptionHandling:
                    labelList[i].opCode = kMipsPseudoEHBlockLabel;
                    if (cUnit->pcReconstructionList.numUsed) {
                        loadWordDisp(cUnit, rGLUE, offsetof(InterpState,
                                     jitToInterpEntries.dvmJitToInterpPunt),
                                     r_A1);
                        opReg(cUnit, kOpBlx, r_A1);
                    }
                    break;
#if defined(WITH_SELF_VERIFICATION) || defined(WITH_JIT_TUNING)
                case kChainingCellBackwardBranch:
                    labelList[i].opCode =
                        kMipsPseudoChainingCellBackwardBranch;
                    /* handle the codegen later */
                    dvmInsertGrowableList(
                        &chainingListByType[kChainingCellBackwardBranch],
                        (void *) i);
                    break;
#endif
                default:
                    break;
            }
            continue;
        }

        MipsLIR *headLIR = NULL;

        for (mir = blockList[i]->firstMIRInsn; mir; mir = mir->next) {

            dvmCompilerResetRegPool(cUnit);
            if (gDvmJit.disableOpt & (1 << kTrackLiveTemps)) {
                dvmCompilerClobberAllRegs(cUnit);
            }

            if (gDvmJit.disableOpt & (1 << kSuppressLoads)) {
                dvmCompilerResetDefTracking(cUnit);
            }

            if (mir->dalvikInsn.opCode >= kMirOpFirst) {
                handleExtendedMIR(cUnit, mir);
                continue;
            }


            OpCode dalvikOpCode = mir->dalvikInsn.opCode;
            InstructionFormat dalvikFormat =
                dexGetInstrFormat(gDvm.instrFormat, dalvikOpCode);
            MipsLIR *boundaryLIR =
                newLIR2(cUnit, kMipsPseudoDalvikByteCodeBoundary,
                        mir->offset,
                        (int) dvmCompilerGetDalvikDisassembly(&mir->dalvikInsn)
                       );
            if (mir->ssaRep) {
                char *ssaString = dvmCompilerGetSSAString(cUnit, mir->ssaRep);
                newLIR1(cUnit, kMipsPseudoSSARep, (int) ssaString);
            }

            /* Remember the first LIR for this block */
            if (headLIR == NULL) {
                headLIR = boundaryLIR;
                /* Set the first boundaryLIR as a scheduling barrier */
                headLIR->defMask = ENCODE_ALL;
            }

            bool notHandled;
            /*
             * Debugging: screen the opcode first to see if it is in the
             * do[-not]-compile list
             */
            bool singleStepMe =
                gDvmJit.includeSelectedOp !=
                ((gDvmJit.opList[dalvikOpCode >> 3] &
                  (1 << (dalvikOpCode & 0x7))) !=
                 0);
#if defined(WITH_SELF_VERIFICATION)
          if (singleStepMe == false) {
              singleStepMe = selfVerificationPuntOps(mir);
          }
#endif
            if (singleStepMe || cUnit->allSingleStep) {
                notHandled = false;
                genInterpSingleStep(cUnit, mir);
            } else {
                opcodeCoverage[dalvikOpCode]++;
                switch (dalvikFormat) {
                    case kFmt10t:
                    case kFmt20t:
                    case kFmt30t:
                        notHandled = handleFmt10t_Fmt20t_Fmt30t(cUnit,
                                  mir, blockList[i], labelList);
                        break;
                    case kFmt10x:
                        notHandled = handleFmt10x(cUnit, mir);
                        break;
                    case kFmt11n:
                    case kFmt31i:
                        notHandled = handleFmt11n_Fmt31i(cUnit, mir);
                        break;
                    case kFmt11x:
                        notHandled = handleFmt11x(cUnit, mir);
                        break;
                    case kFmt12x:
                        notHandled = handleFmt12x(cUnit, mir);
                        break;
                    case kFmt20bc:
                        notHandled = handleFmt20bc(cUnit, mir);
                        break;
                    case kFmt21c:
                    case kFmt31c:
                        notHandled = handleFmt21c_Fmt31c(cUnit, mir);
                        break;
                    case kFmt21h:
                        notHandled = handleFmt21h(cUnit, mir);
                        break;
                    case kFmt21s:
                        notHandled = handleFmt21s(cUnit, mir);
                        break;
                    case kFmt21t:
                        notHandled = handleFmt21t(cUnit, mir, blockList[i],
                                                  labelList);
                        break;
                    case kFmt22b:
                    case kFmt22s:
                        notHandled = handleFmt22b_Fmt22s(cUnit, mir);
                        break;
                    case kFmt22c:
                        notHandled = handleFmt22c(cUnit, mir);
                        break;
                    case kFmt22cs:
                        notHandled = handleFmt22cs(cUnit, mir);
                        break;
                    case kFmt22t:
                        notHandled = handleFmt22t(cUnit, mir, blockList[i],
                                                  labelList);
                        break;
                    case kFmt22x:
                    case kFmt32x:
                        notHandled = handleFmt22x_Fmt32x(cUnit, mir);
                        break;
                    case kFmt23x:
                        notHandled = handleFmt23x(cUnit, mir);
                        break;
                    case kFmt31t:
                        notHandled = handleFmt31t(cUnit, mir);
                        break;
                    case kFmt3rc:
                    case kFmt35c:
                        notHandled = handleFmt35c_3rc(cUnit, mir, blockList[i],
                                                      labelList);
                        break;
                    case kFmt3rms:
                    case kFmt35ms:
                        notHandled = handleFmt35ms_3rms(cUnit, mir,blockList[i],
                                                        labelList);
                        break;
                    case kFmt3inline:
                    case kFmt3rinline:
                        notHandled = handleExecuteInline(cUnit, mir);
                        break;
                    case kFmt51l:
                        notHandled = handleFmt51l(cUnit, mir);
                        break;
                    default:
                        notHandled = true;
                        break;
                }
            }
            if (notHandled) {
                LOGE("%#06x: Opcode 0x%x (%s) / Fmt %d not handled\n",
                     mir->offset,
                     dalvikOpCode, getOpcodeName(dalvikOpCode),
                     dalvikFormat);
                dvmCompilerAbort(cUnit);
                break;
            }
        }

        if (blockList[i]->blockType == kEntryBlock) {
            dvmCompilerAppendLIR(cUnit,
                                 (LIR *) cUnit->loopAnalysis->branchToBody);
            dvmCompilerAppendLIR(cUnit,
                                 (LIR *) cUnit->loopAnalysis->branchToPCR);
        }

        if (headLIR) {
            /*
             * Eliminate redundant loads/stores and delay stores into later
             * slots
             */
            dvmCompilerApplyLocalOptimizations(cUnit, (LIR *) headLIR,
                                               cUnit->lastLIRInsn);
        }

gen_fallthrough:
        /*
         * Check if the block is terminated due to trace length constraint -
         * insert an unconditional branch to the chaining cell.
         */
        if (blockList[i]->needFallThroughBranch) {
            genUnconditionalBranch(cUnit,
                                   &labelList[blockList[i]->fallThrough->id]);
        }

    }

    /* Handle the chaining cells in predefined order */
    for (i = 0; i < kChainingCellGap; i++) {
        size_t j;
        int *blockIdList = (int *) chainingListByType[i].elemList;

        cUnit->numChainingCells[i] = chainingListByType[i].numUsed;

        /* No chaining cells of this type */
        if (cUnit->numChainingCells[i] == 0)
            continue;

        /* Record the first LIR for a new type of chaining cell */
        cUnit->firstChainingLIR[i] = (LIR *) &labelList[blockIdList[0]];

        for (j = 0; j < chainingListByType[i].numUsed; j++) {
            int blockId = blockIdList[j];

            /* Insert the pseudo chaining instruction */
            dvmCompilerAppendLIR(cUnit, (LIR *) &labelList[blockId]);


            switch (blockList[blockId]->blockType) {
                case kChainingCellNormal:
                    handleNormalChainingCell(cUnit,
                      blockList[blockId]->startOffset);
                    break;
                case kChainingCellInvokeSingleton:
                    handleInvokeSingletonChainingCell(cUnit,
                        blockList[blockId]->containingMethod);
                    break;
                case kChainingCellInvokePredicted:
                    handleInvokePredictedChainingCell(cUnit);
                    break;
                case kChainingCellHot:
                    handleHotChainingCell(cUnit,
                        blockList[blockId]->startOffset);
                    break;
#if defined(WITH_SELF_VERIFICATION) || defined(WITH_JIT_TUNING)
                case kChainingCellBackwardBranch:
                    handleBackwardBranchChainingCell(cUnit,
                        blockList[blockId]->startOffset);
                    break;
#endif
                default:
                    LOGE("Bad blocktype %d", blockList[blockId]->blockType);
                    dvmCompilerAbort(cUnit);
            }
        }
    }

    /* Mark the bottom of chaining cells */
    cUnit->chainingCellBottom = (LIR *) newLIR0(cUnit, kMipsChainingCellBottom);

    /*
     * Generate the branch to the dvmJitToInterpNoChain entry point at the end
     * of all chaining cells for the overflow cases.
     */
    if (cUnit->switchOverflowPad) {
        loadConstant(cUnit, r_A0, (int) cUnit->switchOverflowPad);
        loadWordDisp(cUnit, rGLUE, offsetof(InterpState,
                     jitToInterpEntries.dvmJitToInterpNoChain), r_A2);
        opRegReg(cUnit, kOpAdd, r_A1, r_A1);
        opRegRegReg(cUnit, kOpAdd, r4PC, r_A0, r_A1);
#if defined(JIT_STATS)
        loadConstant(cUnit, r_A0, kSwitchOverflow);
#endif
        opReg(cUnit, kOpBlx, r_A2);
    }

    dvmCompilerApplyGlobalOptimizations(cUnit);

#if defined(WITH_SELF_VERIFICATION)
    selfVerificationBranchInsertPass(cUnit);
#endif
}

/* Accept the work and start compiling */
bool dvmCompilerDoWork(CompilerWorkOrder *work)
{
    bool res;

    if (gDvmJit.codeCacheFull) {
        return false;
    }

    switch (work->kind) {
        case kWorkOrderMethod:
            res = dvmCompileMethod(work->info, &work->result);
            break;
        case kWorkOrderTrace:
            /* Start compilation with maximally allowed trace length */
            res = dvmCompileTrace(work->info, JIT_MAX_TRACE_LEN, &work->result,
                                  work->bailPtr);
            break;
        case kWorkOrderTraceDebug: {
            bool oldPrintMe = gDvmJit.printMe;
            gDvmJit.printMe = true;
            /* Start compilation with maximally allowed trace length */
            res = dvmCompileTrace(work->info, JIT_MAX_TRACE_LEN, &work->result,
                                  work->bailPtr);
            gDvmJit.printMe = oldPrintMe;;
            break;
        }
        default:
            res = false;
            LOGE("Jit: unknown work order type");
            assert(0);  // Bail if debug build, discard oteherwise
    }
    return res;
}

/* Architectural-specific debugging helpers go here */
void dvmCompilerArchDump(void)
{
    /* Print compiled opcode in this VM instance */
    int i, start, streak;
    char buf[1024];

    streak = i = 0;
    buf[0] = 0;
    while (opcodeCoverage[i] == 0 && i < 256) {
        i++;
    }
    if (i == 256) {
        return;
    }
    for (start = i++, streak = 1; i < 256; i++) {
        if (opcodeCoverage[i]) {
            streak++;
        } else {
            if (streak == 1) {
                sprintf(buf+strlen(buf), "%x,", start);
            } else {
                sprintf(buf+strlen(buf), "%x-%x,", start, start + streak - 1);
            }
            streak = 0;
            while (opcodeCoverage[i] == 0 && i < 256) {
                i++;
            }
            if (i < 256) {
                streak = 1;
                start = i;
            }
        }
    }
    if (streak) {
        if (streak == 1) {
            sprintf(buf+strlen(buf), "%x", start);
        } else {
            sprintf(buf+strlen(buf), "%x-%x", start, start + streak - 1);
        }
    }
    if (strlen(buf)) {
        LOGD("dalvik.vm.jit.op = %s", buf);
    }
}

/* Common initialization routine for an architecture family */
bool dvmCompilerArchInit()
{
    int i;

    for (i = 0; i < kMipsLast; i++) {
        if (EncodingMap[i].opCode != i) {
            LOGE("Encoding order for %s is wrong: expecting %d, seeing %d",
                 EncodingMap[i].name, i, EncodingMap[i].opCode);
            dvmAbort();  // OK to dvmAbort - build error
        }
    }

    return dvmCompilerArchVariantInit();
}

void *dvmCompilerGetInterpretTemplate()
{
assert(0); /* MIPSTODO activate dvmCompilerGetInterpretTemplate() */
      return (void*) ((int)gDvmJit.codeCache +
                      templateEntryOffsets[TEMPLATE_INTERPRET]);
}

/* Needed by the ld/st optmizatons */
MipsLIR* dvmCompilerRegCopyNoInsert(CompilationUnit *cUnit, int rDest, int rSrc)
{
    return genRegCopyNoInsert(cUnit, rDest, rSrc);
}

/* Needed by the register allocator */
MipsLIR* dvmCompilerRegCopy(CompilationUnit *cUnit, int rDest, int rSrc)
{
    return genRegCopy(cUnit, rDest, rSrc);
}

/* Needed by the register allocator */
void dvmCompilerRegCopyWide(CompilationUnit *cUnit, int destLo, int destHi,
                            int srcLo, int srcHi)
{
    genRegCopyWide(cUnit, destLo, destHi, srcLo, srcHi);
}

void dvmCompilerFlushRegImpl(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc, OpSize size)
{
    storeBaseDisp(cUnit, rBase, displacement, rSrc, size);
}

void dvmCompilerFlushRegWideImpl(CompilationUnit *cUnit, int rBase,
                                 int displacement, int rSrcLo, int rSrcHi)
{
    storeBaseDispWide(cUnit, rBase, displacement, rSrcLo, rSrcHi);
}
