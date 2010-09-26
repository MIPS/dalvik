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
 * This file contains codegen for the Thumb ISA and is intended to be
 * includes by:
 *
 *        Codegen-$(TARGET_ARCH_VARIANT).c
 *
 */

#ifdef OLD_ARM
static int coreTemps[] = {r0, r1, r2, r3, r4PC, r7}; 
#else
static int coreTemps[] = {r_V0, r_V1, r_A0, r_A1, r_A2, r_A3, r_T0, r_T1, r_T2, r_T3, r_T4, r_T5, r_T6, r_T7, r_T8, r_T9, r_S0, r_S4};
#endif
static int corePreserved[] = {};

static void storePair(CompilationUnit *cUnit, int base, int lowReg,
                      int highReg);
static void loadPair(CompilationUnit *cUnit, int base, int lowReg, int highReg);
static ArmLIR *loadWordDisp(CompilationUnit *cUnit, int rBase, int displacement,
                            int rDest);
static ArmLIR *storeWordDisp(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc);
static ArmLIR *genRegRegCheck(CompilationUnit *cUnit,
                              ArmConditionCode cond,
                              int reg1, int reg2, int dOffset,
                              ArmLIR *pcrLabel);


/*
 * Load a immediate using a shortcut if possible; otherwise
 * grab from the per-translation literal pool.  If target is
 * a high register, build constant into a low register and copy.
 *
 * No additional register clobbering operation performed. Use this version when
 * 1) rDest is freshly returned from dvmCompilerAllocTemp or
 * 2) The codegen is under fixed register usage
 */
static ArmLIR *loadConstantNoClobber(CompilationUnit *cUnit, int rDest,
                                     int value)
{
assert(1); /* DRP verify loadConstantNoClobber() */
    ArmLIR *res;

    /* See if the value can be constructed cheaply */
    if ((value >= 0) && (value <= 65535)) {
        res = newLIR3(cUnit, kMipsOri, rDest, r_ZERO, value); 
    } else if ((value < 0) && (value >= -32768)) {
        res = newLIR3(cUnit, kMipsAddiu, rDest, r_ZERO, value); 
    } else {
        res = newLIR2(cUnit, kMipsLui, rDest, value>>16);
        newLIR3(cUnit, kMipsOri, rDest, rDest, value); 
    }
    return res; 

#ifdef OLD_ARM
    ArmLIR *res;
    int tDest = LOWREG(rDest) ? rDest : dvmCompilerAllocTemp(cUnit);
    /* See if the value can be constructed cheaply */
    if ((value >= 0) && (value <= 255)) {
        res = newLIR2(cUnit, kThumbMovImm, tDest, value);
        if (rDest != tDest) {
           opRegReg(cUnit, kOpMov, rDest, tDest);
           dvmCompilerFreeTemp(cUnit, tDest);
        }
        return res;
    } else if ((value & 0xFFFFFF00) == 0xFFFFFF00) {
        res = newLIR2(cUnit, kThumbMovImm, tDest, ~value);
        newLIR2(cUnit, kThumbMvn, tDest, tDest);
        if (rDest != tDest) {
           opRegReg(cUnit, kOpMov, rDest, tDest);
           dvmCompilerFreeTemp(cUnit, tDest);
        }
        return res;
    }
    /* No shortcut - go ahead and use literal pool */
    ArmLIR *dataTarget = scanLiteralPool(cUnit, value, 255);
    if (dataTarget == NULL) {
        dataTarget = addWordData(cUnit, value, false);
    }
    ArmLIR *loadPcRel = dvmCompilerNew(sizeof(ArmLIR), true);
    loadPcRel->opCode = kThumbLdrPcRel;
    loadPcRel->generic.target = (LIR *) dataTarget;
    loadPcRel->operands[0] = tDest;
    setupResourceMasks(loadPcRel);
    /*
     * Special case for literal loads with a link register target.
     * Self-cosim mode will insert calls prior to heap references
     * after optimization, and those will destroy r14.  The easy
     * workaround is to treat literal loads into r14 as heap references
     * to prevent them from being hoisted.  Use of r14 in this manner
     * is currently rare.  Revist if that changes.
     */
    if (rDest != rlr)
        setMemRefType(loadPcRel, true, kLiteral);
    loadPcRel->aliasInfo = dataTarget->operands[0];
    res = loadPcRel;
    dvmCompilerAppendLIR(cUnit, (LIR *) loadPcRel);

    /*
     * To save space in the constant pool, we use the ADD_RRI8 instruction to
     * add up to 255 to an existing constant value.
     */
    if (dataTarget->operands[0] != value) {
        newLIR2(cUnit, kThumbAddRI8, tDest, value - dataTarget->operands[0]);
    }
    if (rDest != tDest) {
       opRegReg(cUnit, kOpMov, rDest, tDest);
       dvmCompilerFreeTemp(cUnit, tDest);
    }
    return res;
#endif
}

/*
 * Load an immediate value into a fixed or temp register.  Target
 * register is clobbered, and marked inUse.
 */
static ArmLIR *loadConstant(CompilationUnit *cUnit, int rDest, int value)
{
assert(1); /* DRP verify loadConstant() */
    if (dvmCompilerIsTemp(cUnit, rDest)) {
        dvmCompilerClobber(cUnit, rDest);
        dvmCompilerMarkInUse(cUnit, rDest);
    }
    return loadConstantNoClobber(cUnit, rDest, value);
}

static ArmLIR *opNone(CompilationUnit *cUnit, OpKind op)
{
assert(1); /* DRP cleanup opNone() */
    ArmLIR *res;
    ArmOpCode opCode = kThumbBkpt;
    switch (op) {
        case kOpUncondBr:
            opCode = kMipsB;
            break;
        default:
            LOGE("Jit: bad case in opNone");
            dvmCompilerAbort(cUnit);
    }
    res = newLIR0(cUnit, opCode);
    return res;
}

static ArmLIR *opCondBranch(CompilationUnit *cUnit, ArmConditionCode cc)
{
assert(0); /* DRP cleanup/replace opCondBranch() with Mips version */
    return newLIR2(cUnit, kThumbBCond, 0 /* offset to be patched */, cc);
}

static ArmLIR *opCondBranchMips(CompilationUnit *cUnit, ArmOpCode opc, int rs, int rt)
{
    ArmLIR *res;
    if (rt < 0) {
      assert(opc >= kMipsBeqz && opc <= kMipsBltzal);
      res = newLIR1(cUnit, opc, rs);
    } else  {
      assert(opc == kMipsBeq || opc == kMipsBne);
      res = newLIR2(cUnit, opc, rs, rt);
    }
    return res;
}

static ArmLIR *loadMultiple(CompilationUnit *cUnit, int rBase, int rMask);

static ArmLIR *opImm(CompilationUnit *cUnit, OpKind op, int value)
{
assert(0); /* DRP cleanup/remove opImm() */
    ArmOpCode opCode = kThumbBkpt;
    switch (op) {
        case kOpPush:
            /* need stmdb so can't call storeMultiple which does stdmia */
            break;
        case kOpPop: /* ldmia */
            return loadMultiple(cUnit, r_SP, value);
            break;
        default:
            LOGE("Jit: bad case in opCondBranch");
            dvmCompilerAbort(cUnit);
    }
   
    assert(0); /* would need stmdb here */

    return newLIR1(cUnit, opCode, value);
}

static ArmLIR *opReg(CompilationUnit *cUnit, OpKind op, int rDestSrc)
{
assert(1); /* DRP verify opReg() */
    ArmOpCode opCode = kThumbBkpt;
    switch (op) {
        case kOpBlx:
            opCode = kMipsJalr;
            break;
        default:
            LOGE("Jit: bad case in opReg");
            dvmCompilerAbort(cUnit);
    }
    return newLIR2(cUnit, opCode, r_RA, rDestSrc);
}

static ArmLIR *opRegRegImm(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int value);
static ArmLIR *opRegImm(CompilationUnit *cUnit, OpKind op, int rDestSrc1,
                        int value)
{
assert(1); /* DRP cleanup/remove kOpCmp in opRegImm() */
    ArmLIR *res;
    bool neg = (value < 0);
    int absValue = (neg) ? -value : value;
    bool shortForm = (absValue & 0xff) == absValue;
    ArmOpCode opCode = kThumbBkpt;
    switch (op) {
        case kOpAdd:
            return opRegRegImm(cUnit, op, rDestSrc1, rDestSrc1, value);
            break;
        case kOpSub:
            return opRegRegImm(cUnit, op, rDestSrc1, rDestSrc1, value);
            break;
        case kOpCmp:
assert(0);
            if (neg)
               shortForm = false;
            if (LOWREG(rDestSrc1) && shortForm) {
                opCode = kThumbCmpRI8;
            } else if (LOWREG(rDestSrc1)) {
                opCode = kThumbCmpRR;
            } else {
                shortForm = false;
                opCode = kThumbCmpHL;
            }
            break;
        default:
            LOGE("Jit: bad case in opRegImm");
            dvmCompilerAbort(cUnit);
            break;
    }
    if (shortForm)
        res = newLIR2(cUnit, opCode, rDestSrc1, absValue);
    else {
        int rScratch = dvmCompilerAllocTemp(cUnit);
        res = loadConstant(cUnit, rScratch, value);
        if (op == kOpCmp)
            newLIR2(cUnit, opCode, rDestSrc1, rScratch);
        else
            newLIR3(cUnit, opCode, rDestSrc1, rDestSrc1, rScratch);
    }
    return res;
}

static ArmLIR *opRegRegReg(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int rSrc2)
{
assert(1); /* DRP cleanup opRegRegReg() */
    ArmOpCode opCode = kThumbBkpt;
    switch (op) {
        case kOpAdd:
            opCode = kMipsAddu;
            break;
        case kOpSub:
            opCode = kMipsSubu;
            break;
        case kOpAnd:
            opCode = kMipsAnd;
            break;
        case kOpMul:
            opCode = kMipsMul;
            break;
        case kOpOr:
            opCode = kMipsOr;
            break;
        case kOpXor:
            opCode = kMipsXor;
            break;
        case kOpLsl:
            opCode = kMipsSllv;
            break;
        case kOpLsr:
            opCode = kMipsSrlv;
            break;
        case kOpAsr:
            opCode = kMipsSrav;
            break;
        default:
LOGE("Jit: bad op in opRegRegReg: %d", op);
assert(0); /* DRP unsupported op in opRegRegReg() */
            break;
    }
    return newLIR3(cUnit, opCode, rDest, rSrc1, rSrc2);
}

static ArmLIR *opRegRegImm(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int value)
{
assert(1); /* DRP review and verify opRegRegImm() */
    ArmLIR *res;
    ArmOpCode opCode = kThumbBkpt;
    bool shortForm = true;

    switch(op) {
        case kOpAdd:
            if (IS_SIMM16(value)) {
                opCode = kMipsAddiu;
            }
            else {
                shortForm = false;
                opCode = kMipsAddu;
            }
            break;
        case kOpSub:
            if (IS_SIMM16((-value))) {
                value = -value;
                opCode = kMipsAddiu;
            }
            else {
                shortForm = false;
                opCode = kMipsSubu;
            }
            break;
        case kOpLsl:
                assert(value >= 0 && value <= 31);
                opCode = kMipsSll;
                break;
        case kOpLsr:
                assert(value >= 0 && value <= 31);
                opCode = kMipsSrl;
                break;
        case kOpAsr:
                assert(value >= 0 && value <= 31);
                opCode = kMipsSra;
                break;
        case kOpAnd:
            if (IS_UIMM16((value))) {
                opCode = kMipsAndi;
            }
            else {
                shortForm = false;
                opCode = kMipsAnd;
            }
            break;
        case kOpOr:
            if (IS_UIMM16((value))) {
                opCode = kMipsOri;
            }
            else {
                shortForm = false;
                opCode = kMipsOr;
            }
            break;
        case kOpXor:
            if (IS_UIMM16((value))) {
                opCode = kMipsXori;
            }
            else {
                shortForm = false;
                opCode = kMipsXor;
            }
            break;
        case kOpMul:
            shortForm = false;
            opCode = kMipsMul;
            break;
        default:
            LOGE("Jit: bad case in opRegRegImm");
            dvmCompilerAbort(cUnit);
            break;
    }

    if (shortForm)
        res = newLIR3(cUnit, opCode, rDest, rSrc1, value);
    else {
        if (rDest != rSrc1) {
            res = loadConstant(cUnit, rDest, value);
            newLIR3(cUnit, opCode, rDest, rSrc1, rDest);
        } else {
            int rScratch = dvmCompilerAllocTemp(cUnit);
            res = loadConstant(cUnit, rScratch, value);
            newLIR3(cUnit, opCode, rDest, rSrc1, rScratch);
        }
    }
    return res;
}

static ArmLIR *opRegReg(CompilationUnit *cUnit, OpKind op, int rDestSrc1,
                        int rSrc2)
{
assert(1); /* DRP retarg opRegReg() finish cases */
    ArmLIR *res;
    ArmOpCode opCode = kThumbBkpt;
    switch (op) {
        case kOpAdc:
assert(0);
            opCode = kThumbAdcRR;
            break;
        case kOpBic:
assert(0);
            opCode = kThumbBicRR;
            break;
        case kOpCmn:
assert(0);
            opCode = kThumbCmnRR;
            break;
        case kOpCmp:
assert(0);
            opCode = kThumbCmpRR;
            break;
        case kOpMov:
            opCode = kMipsMove;
            break;
        case kOpMvn:
            return newLIR3(cUnit, kMipsNor, rDestSrc1, rSrc2, r_ZERO);
        case kOpNeg:
            return newLIR3(cUnit, kMipsSubu, rDestSrc1, r_ZERO, rSrc2);
        case kOpSbc:
assert(0);
            opCode = kThumbSbc;
            break;
        case kOpTst:
assert(0);
            opCode = kThumbTst;
            break;
        case kOpLsl:
assert(0);
            opCode = kThumbLslRR;
            break;
        case kOpLsr:
assert(0);
            opCode = kThumbLsrRR;
            break;
        case kOpAsr:
assert(0);
            opCode = kThumbAsrRR;
            break;
        case kOpRor:
assert(0);
            opCode = kThumbRorRR;
            break;
        case kOpAdd:
        case kOpAnd:
        case kOpMul:
        case kOpOr:
        case kOpSub:
        case kOpXor:
            return opRegRegReg(cUnit, op, rDestSrc1, rDestSrc1, rSrc2);
        case kOp2Byte: /* DRP use seb instruction */
             res = opRegRegImm(cUnit, kOpLsl, rDestSrc1, rSrc2, 24);
             opRegRegImm(cUnit, kOpAsr, rDestSrc1, rDestSrc1, 24);
             return res;
        case kOp2Short: /* DRP use seh instruction */
             res = opRegRegImm(cUnit, kOpLsl, rDestSrc1, rSrc2, 16);
             opRegRegImm(cUnit, kOpAsr, rDestSrc1, rDestSrc1, 16);
             return res;
        case kOp2Char: /* DRP use andi instruction */
             res = opRegRegImm(cUnit, kOpLsl, rDestSrc1, rSrc2, 16);
             opRegRegImm(cUnit, kOpLsr, rDestSrc1, rDestSrc1, 16);
             return res;
        default:
            LOGE("Jit: bad case in opRegReg");
            dvmCompilerAbort(cUnit);
            break;
    }
    return newLIR2(cUnit, opCode, rDestSrc1, rSrc2);
}

static ArmLIR *loadConstantValueWide(CompilationUnit *cUnit, int rDestLo,
                                     int rDestHi, int valLo, int valHi)
{
assert(1); /* DRP verify loadConstantValueWide() */
    ArmLIR *res;
    res = loadConstantNoClobber(cUnit, rDestLo, valLo);
    loadConstantNoClobber(cUnit, rDestHi, valHi);
    return res;
}

/* Load value from base + scaled index. */
static ArmLIR *loadBaseIndexed(CompilationUnit *cUnit, int rBase,
                               int rIndex, int rDest, int scale, OpSize size)
{
assert(1); /* DRP verify loadBaseIndexed() */
    ArmLIR *first = NULL;
    ArmLIR *res;
    ArmOpCode opCode = kThumbBkpt;
    int tReg = dvmCompilerAllocTemp(cUnit);

    if (!scale) {
        first = newLIR3(cUnit, kMipsAddu, tReg , rBase, rIndex);
    } else {
        first = opRegRegImm(cUnit, kOpLsl, tReg, rIndex, scale);
        newLIR3(cUnit, kMipsAddu, tReg , rBase, tReg);
    }

    switch (size) {
        case kWord:
            opCode = kMipsLw;
            break;
        case kUnsignedHalf:
            opCode = kMipsLhu;
            break;
        case kSignedHalf:
            opCode = kMipsLh;
            break;
        case kUnsignedByte:
            opCode = kMipsLbu;
            break;
        case kSignedByte:
            opCode = kMipsLb;
            break;
        default:
            LOGE("Jit: bad case in loadBaseIndexed");
            dvmCompilerAbort(cUnit);
    }

    res = newLIR3(cUnit, opCode, rDest, 0, tReg);
#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->branchInsertSV = true;
#endif
    dvmCompilerFreeTemp(cUnit, tReg);
    return (first) ? first : res;
}

/* store value base base + scaled index. */
static ArmLIR *storeBaseIndexed(CompilationUnit *cUnit, int rBase,
                                int rIndex, int rSrc, int scale, OpSize size)
{
assert(1); /* DRP verify storeBaseIndexed() */
    ArmLIR *first = NULL;
    ArmLIR *res;
    ArmOpCode opCode = kThumbBkpt;
    int rNewIndex = rIndex;
    int tReg = dvmCompilerAllocTemp(cUnit);

    if (!scale) {
        first = newLIR3(cUnit, kMipsAddu, tReg , rBase, rIndex);
    } else {
        first = opRegRegImm(cUnit, kOpLsl, tReg, rIndex, scale);
        newLIR3(cUnit, kMipsAddu, tReg , rBase, tReg);
    }

    switch (size) {
        case kWord:
            opCode = kMipsSw;
            break;
        case kUnsignedHalf:
        case kSignedHalf:
            opCode = kMipsSh;
            break;
        case kUnsignedByte:
        case kSignedByte:
            opCode = kMipsSb;
            break;
        default:
            LOGE("Jit: bad case in storeBaseIndexed");
            dvmCompilerAbort(cUnit);
    }
    res = newLIR3(cUnit, opCode, rSrc, 0, tReg);
#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->branchInsertSV = true;
#endif
    dvmCompilerFreeTemp(cUnit, rNewIndex);
    return first;
}

static ArmLIR *loadMultiple(CompilationUnit *cUnit, int rBase, int rMask)
{
assert(1); /* DRP verify loadMultiple() */
    int i;
    int loadCnt = 0;
    ArmLIR *res = NULL ;
    genBarrier(cUnit);

    for (i = 0; i < 8; i++, rMask >>= 1) {
        if (rMask & 0x1) { /* map ARM r0 to MIPS r_A0 */
            newLIR3(cUnit, kMipsLw, i+r_A0, loadCnt*4, rBase);
            loadCnt++;
        }
    }

    if (loadCnt) {/* increment after */
        newLIR3(cUnit, kMipsAddiu, rBase, rBase, loadCnt*4);
    }

#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->branchInsertSV = true;
#endif
    genBarrier(cUnit);
    return res; /* NULL always returned which should be ok since no callers use it */
}

static ArmLIR *storeMultiple(CompilationUnit *cUnit, int rBase, int rMask)
{
assert(1); /* DRP verify storeMultiple() */
    int i;
    int storeCnt = 0;
    ArmLIR *res = NULL ;
    genBarrier(cUnit);

    for (i = 0; i < 8; i++, rMask >>= 1) {
        if (rMask & 0x1) { /* map ARM r0 to MIPS r_A0 */
            newLIR3(cUnit, kMipsSw, i+r_A0, storeCnt*4, rBase);
            storeCnt++;
        }
    }
  
    if (storeCnt) { /* increment after */
        newLIR3(cUnit, kMipsAddiu, rBase, rBase, storeCnt*4);
    }

#if defined(WITH_SELF_VERIFICATION)
    if (cUnit->heapMemOp)
        res->branchInsertSV = true;
#endif
    genBarrier(cUnit);
    return res; /* NULL always returned which should be ok since no callers use it */
}

static ArmLIR *loadBaseDispBody(CompilationUnit *cUnit, MIR *mir, int rBase,
                                int displacement, int rDest, int rDestHi,
                                OpSize size, int sReg)
/*
 * Load value from base + displacement.  Optionally perform null check
 * on base (which must have an associated sReg and MIR).  If not
 * performing null check, incoming MIR can be null. IMPORTANT: this
 * code must not allocate any new temps.  If a new register is needed
 * and base and dest are the same, spill some other register to
 * rlp and then restore.
 */
{
assert(1); /* DRP verify loadBaseDispBody() */
    ArmLIR *res;
    ArmLIR *load = NULL;
    ArmLIR *load2 = NULL;
    ArmOpCode opCode = kThumbBkpt;
    bool shortForm = IS_SIMM16(displacement);
    bool pair = false;

    switch (size) {
        case kLong:
        case kDouble:
            pair = true;
            opCode = kMipsLw;
            shortForm = IS_SIMM16_2WORD(displacement);
            assert((displacement & 0x3) == 0);
            break;
        case kWord:
            opCode = kMipsLw;
            assert((displacement & 0x3) == 0);
            break;
        case kUnsignedHalf:
            opCode = kMipsLhu;
            assert((displacement & 0x1) == 0);
            break;
        case kSignedHalf:
            opCode = kMipsLh;
            assert((displacement & 0x1) == 0);
            break;
        case kUnsignedByte:
            opCode = kMipsLbu;
            break;
        case kSignedByte:
            opCode = kMipsLb;
            break;
        default:
            LOGE("Jit: bad case in loadBaseIndexedBody");
            dvmCompilerAbort(cUnit);
    }

    if (shortForm) {
        load = res = newLIR3(cUnit, opCode, rDest, displacement, rBase);
        if (pair) {
            load2 = newLIR3(cUnit, opCode, rDestHi, displacement+4, rBase);
        }
    } else {
        if (pair) {
            int rTmp = dvmCompilerAllocFreeTemp(cUnit);
            res = opRegRegImm(cUnit, kOpAdd, rTmp, rBase, displacement);
            load = newLIR3(cUnit, opCode, rDest, 0, rTmp);
            load2 = newLIR3(cUnit, opCode, rDestHi, 4, rTmp);
            dvmCompilerFreeTemp(cUnit, rTmp);
        } else {
            int rTmp = (rBase == rDest) ? dvmCompilerAllocFreeTemp(cUnit)
                                        : rDest;
            res = loadConstant(cUnit, rTmp, displacement);
            load = newLIR3(cUnit, opCode, rDest, rBase, rTmp);
            if (rTmp != rDest)
                dvmCompilerFreeTemp(cUnit, rTmp);
        }
    }

    if (rBase == rFP) {
        if (load != NULL)
            annotateDalvikRegAccess(load, displacement >> 2,
                                    true /* isLoad */);
        if (load2 != NULL)
            annotateDalvikRegAccess(load2, (displacement >> 2) + 1,
                                    true /* isLoad */);
    }
#if defined(WITH_SELF_VERIFICATION)
    if (load != NULL && cUnit->heapMemOp)
        load->branchInsertSV = true;
    if (load2 != NULL && cUnit->heapMemOp)
        load2->branchInsertSV = true;
#endif
    return res;
}

static ArmLIR *loadBaseDisp(CompilationUnit *cUnit, MIR *mir, int rBase,
                            int displacement, int rDest, OpSize size,
                            int sReg)
{
assert(1); /* DRP verify loadBaseDisp() */
    return loadBaseDispBody(cUnit, mir, rBase, displacement, rDest, -1,
                            size, sReg);
}

static ArmLIR *loadBaseDispWide(CompilationUnit *cUnit, MIR *mir, int rBase,
                                int displacement, int rDestLo, int rDestHi,
                                int sReg)
{
assert(1); /* DRP verify loadBaseDispWide() */
    return loadBaseDispBody(cUnit, mir, rBase, displacement, rDestLo, rDestHi,
                            kLong, sReg);
}

static ArmLIR *storeBaseDispBody(CompilationUnit *cUnit, int rBase,
                                 int displacement, int rSrc, int rSrcHi,
                                 OpSize size)
{
assert(1); /* DRP verify storeBaseDispBody() */
    ArmLIR *res;
    ArmLIR *store = NULL;
    ArmLIR *store2 = NULL;
    ArmOpCode opCode = kThumbBkpt;
    bool shortForm = IS_SIMM16(displacement);
    bool pair = false;

    switch (size) {
        case kLong:
        case kDouble:
            pair = true;
            opCode = kMipsSw;
            shortForm = IS_SIMM16_2WORD(displacement);
            assert((displacement & 0x3) == 0);
            break;
        case kWord:
            opCode = kMipsSw;
            assert((displacement & 0x3) == 0);
            break;
        case kUnsignedHalf:
        case kSignedHalf:
            opCode = kMipsSh;
            assert((displacement & 0x1) == 0);
            break;
        case kUnsignedByte:
        case kSignedByte:
            opCode = kMipsSb;
            break;
        default:
            LOGE("Jit: bad case in storeBaseIndexedBody");
            dvmCompilerAbort(cUnit);
    }

    if (shortForm) {
        store = res = newLIR3(cUnit, opCode, rSrc, displacement, rBase);
        if (pair) {
            store2 = newLIR3(cUnit, opCode, rSrcHi, displacement+4, rBase);
        }
    } else {
        int rScratch = dvmCompilerAllocTemp(cUnit);
        res = opRegRegImm(cUnit, kOpAdd, rScratch, rBase, displacement);
        store =  newLIR3(cUnit, opCode, rSrc, 0, rScratch);
        if (pair) {
            store2 = newLIR3(cUnit, opCode, rSrcHi, 4, rScratch);
        }
        dvmCompilerFreeTemp(cUnit, rScratch);
    }

    if (rBase == rFP) {
        if (store != NULL)
            annotateDalvikRegAccess(store, displacement >> 2,
                                    false /* isLoad */);
        if (store2 != NULL)
            annotateDalvikRegAccess(store2, (displacement >> 2) + 1,
                                    false /* isLoad */);
    }

#if defined(WITH_SELF_VERIFICATION)
    if (store != NULL && cUnit->heapMemOp)
        store->branchInsertSV = true;
    if (store2 != NULL && cUnit->heapMemOp)
        store2->branchInsertSV = true;
#endif
    return res;
}

static ArmLIR *storeBaseDisp(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc, OpSize size)
{
assert(1); /* DRP verify storeBaseDisp() */
    return storeBaseDispBody(cUnit, rBase, displacement, rSrc, -1, size);
}

static ArmLIR *storeBaseDispWide(CompilationUnit *cUnit, int rBase,
                                 int displacement, int rSrcLo, int rSrcHi)
{
assert(1); /* DRP verify storeBaseDispWide() */
    return storeBaseDispBody(cUnit, rBase, displacement, rSrcLo, rSrcHi, kLong);
}

static void storePair(CompilationUnit *cUnit, int base, int lowReg, int highReg)
{
assert(1); /* DRP cleanup storePair() */
    if (0 && lowReg < highReg) {
        storeMultiple(cUnit, base, (1 << lowReg) | (1 << highReg));
    } else {
        storeWordDisp(cUnit, base, 0, lowReg);
        storeWordDisp(cUnit, base, 4, highReg);
    }
}

static void loadPair(CompilationUnit *cUnit, int base, int lowReg, int highReg)
{
assert(1); /* DRP cleanup loadPair() */
    if (0 && lowReg < highReg) {
        loadMultiple(cUnit, base, (1 << lowReg) | (1 << highReg));
    } else {
        loadWordDisp(cUnit, base, 0 , lowReg);
        loadWordDisp(cUnit, base, 4 , highReg);
    }
}

static ArmLIR* genRegCopyNoInsert(CompilationUnit *cUnit, int rDest, int rSrc)
{
assert(1); /* DRP verify genRegCopyNoInsert() */
    ArmLIR* res;
    ArmOpCode opCode;
    res = dvmCompilerNew(sizeof(ArmLIR), true);
#ifdef OLD_ARM 
    if (LOWREG(rDest) && LOWREG(rSrc))
        opCode = kThumbMovRR;
    else if (!LOWREG(rDest) && !LOWREG(rSrc))
         opCode = kThumbMovRR_H2H;
    else if (LOWREG(rDest))
         opCode = kThumbMovRR_H2L;
    else
         opCode = kThumbMovRR_L2H;
#else
    opCode = kMipsMove;
    if (!LOWREG(rDest) || !LOWREG(rSrc)) {
        assert(0); /* DRP cleanup this LOWREG stuff after testing */
    }
#endif
    res->operands[0] = rDest;
    res->operands[1] = rSrc;
    res->opCode = opCode;
    setupResourceMasks(res);
    if (rDest == rSrc) {
        res->isNop = true;
    }
    return res;
}

static ArmLIR* genRegCopy(CompilationUnit *cUnit, int rDest, int rSrc)
{
assert(1); /* DRP verify genRegCopy() */
    ArmLIR *res = genRegCopyNoInsert(cUnit, rDest, rSrc);
    dvmCompilerAppendLIR(cUnit, (LIR*)res);
    return res;
}

static void genRegCopyWide(CompilationUnit *cUnit, int destLo, int destHi,
                           int srcLo, int srcHi)
{
assert(1); /* DRP verify genRegCopyWide() */
    // Handle overlap
    if (srcHi == destLo) {
        genRegCopy(cUnit, destHi, srcHi);
        genRegCopy(cUnit, destLo, srcLo);
    } else {
        genRegCopy(cUnit, destLo, srcLo);
        genRegCopy(cUnit, destHi, srcHi);
    }
}

static inline ArmLIR *genRegImmCheck(CompilationUnit *cUnit,
                                     ArmConditionCode cond, int reg,
                                     int checkValue, int dOffset,
                                     ArmLIR *pcrLabel)
{
assert(1); /* DRP finish retarg genRegImmCheck() */
#ifdef OLD_ARM
    int tReg;
    ArmLIR *res;
    if ((checkValue & 0xff) != checkValue) {
        tReg = dvmCompilerAllocTemp(cUnit);
        loadConstant(cUnit, tReg, checkValue);
        res = genRegRegCheck(cUnit, cond, reg, tReg, dOffset, pcrLabel);
        dvmCompilerFreeTemp(cUnit, tReg);
        return res;
    }
    newLIR2(cUnit, kThumbCmpRI8, reg, checkValue);
    ArmLIR *branch = newLIR2(cUnit, kThumbBCond, 0, cond);
    return genCheckCommon(cUnit, dOffset, branch, pcrLabel);
#else
    ArmLIR *branch;

    if (checkValue == 0) {
        ArmOpCode opc = kMipsNop;
        if (cond == kArmCondEq) {
            opc = kMipsBeqz;
        } else if (cond == kArmCondLt || cond == kArmCondMi) {
            opc = kMipsBltz;
        } else if (cond == kArmCondLe) {
            opc = kMipsBlez;
        } else if (cond == kArmCondGt) {
            opc = kMipsBgtz;
        } else if (cond == kArmCondGe) {
            opc = kMipsBgez;
        } else {
            assert(0); /* add cases to genRegImmCheck() */
        }
        branch = opCondBranchMips(cUnit, opc, reg, -1);
    } else if (IS_SIMM16(checkValue)) {
        if (cond == kArmCondLt) {
            int tReg = dvmCompilerAllocTemp(cUnit);
            newLIR3(cUnit, kMipsSlti, tReg, reg, checkValue);       
            branch = opCondBranchMips(cUnit, kMipsBne, tReg, r_ZERO);
            dvmCompilerFreeTemp(cUnit, tReg);
        } else {
            assert(0); /* DRP add cases to genRegImmCheck() */
        } 
    } else {
        assert(0); /* DRP add cases to genRegImmCheck() */
    }

    return genCheckCommon(cUnit, dOffset, branch, pcrLabel);
#endif
}
