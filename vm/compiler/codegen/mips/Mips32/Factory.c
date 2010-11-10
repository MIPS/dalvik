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

static int coreTemps[] = {r_V0, r_V1, r_A0, r_A1, r_A2, r_A3, r_T0, r_T1, r_T2,
                          r_T3, r_T4, r_T5, r_T6, r_T7, r_T8, r_T9, r_S0, r_S4};
#ifdef __mips_hard_float
static int fpTemps[] = {r_F0, r_F1, r_F2, r_F3, r_F4, r_F5, r_F6, r_F7,
                        r_F8, r_F9, r_F10, r_F11, r_F12, r_F13, r_F14, r_F15};
#endif
static int corePreserved[] = {};

static void storePair(CompilationUnit *cUnit, int base, int lowReg,
                      int highReg);
static void loadPair(CompilationUnit *cUnit, int base, int lowReg, int highReg);
static MipsLIR *loadWordDisp(CompilationUnit *cUnit, int rBase, int displacement,
                            int rDest);
static MipsLIR *storeWordDisp(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc);
static MipsLIR *genRegRegCheck(CompilationUnit *cUnit,
                              MipsConditionCode cond,
                              int reg1, int reg2, int dOffset,
                              MipsLIR *pcrLabel);
static MipsLIR *loadConstant(CompilationUnit *cUnit, int rDest, int value);

#ifdef __mips_hard_float
static MipsLIR *fpRegCopy(CompilationUnit *cUnit, int rDest, int rSrc)
{
    MipsLIR* res = dvmCompilerNew(sizeof(MipsLIR), true);
    res->operands[0] = rDest;
    res->operands[1] = rSrc;
    if (rDest == rSrc) {
        res->isNop = true;
    } else {
        /* must be both DOUBLE or both not DOUBLE */
        assert(DOUBLEREG(rDest) == DOUBLEREG(rSrc));
        if (DOUBLEREG(rDest)) {
            res->opCode = kMipsFmovd;
        } else {
            if (SINGLEREG(rDest)) {
                if (SINGLEREG(rSrc)) {
                    res->opCode = kMipsFmovs;
                } else {
                    /* note the operands are swapped for the mtc1 instr */
                    res->opCode = kMipsMtc1;
                    res->operands[0] = rSrc;
                    res->operands[1] = rDest;
                }
            } else {
                assert(SINGLEREG(rSrc));
                res->opCode = kMipsMfc1;
            }
        }
    }
    setupResourceMasks(res);
    return res;
}
#endif

/*
 * Load a immediate using a shortcut if possible; otherwise
 * grab from the per-translation literal pool.  If target is
 * a high register, build constant into a low register and copy.
 *
 * No additional register clobbering operation performed. Use this version when
 * 1) rDest is freshly returned from dvmCompilerAllocTemp or
 * 2) The codegen is under fixed register usage
 */
static MipsLIR *loadConstantNoClobber(CompilationUnit *cUnit, int rDest,
                                     int value)
{
    MipsLIR *res;

#ifdef __mips_hard_float
    int rDestSave = rDest;
    int isFpReg = FPREG(rDest);
    if (isFpReg) {
        assert(SINGLEREG(rDest));
        rDest = dvmCompilerAllocTemp(cUnit);
    }
#endif

    /* See if the value can be constructed cheaply */
    if ((value >= 0) && (value <= 65535)) {
        res = newLIR3(cUnit, kMipsOri, rDest, r_ZERO, value); 
    } else if ((value < 0) && (value >= -32768)) {
        res = newLIR3(cUnit, kMipsAddiu, rDest, r_ZERO, value); 
    } else {
        res = newLIR2(cUnit, kMipsLui, rDest, value>>16);
        newLIR3(cUnit, kMipsOri, rDest, rDest, value); 
    }

#ifdef __mips_hard_float
    if (isFpReg) {
        newLIR2(cUnit, kMipsMtc1, rDest, rDestSave);
        dvmCompilerFreeTemp(cUnit, rDest);
    }
#endif

    return res; 
}

/*
 * Load an immediate value into a fixed or temp register.  Target
 * register is clobbered, and marked inUse.
 */
static MipsLIR *loadConstant(CompilationUnit *cUnit, int rDest, int value)
{
    if (dvmCompilerIsTemp(cUnit, rDest)) {
        dvmCompilerClobber(cUnit, rDest);
        dvmCompilerMarkInUse(cUnit, rDest);
    }
    return loadConstantNoClobber(cUnit, rDest, value);
}

static MipsLIR *opNone(CompilationUnit *cUnit, OpKind op)
{
    MipsLIR *res;
    MipsOpCode opCode = kMipsNop;
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

static MipsLIR *opCondBranchMips(CompilationUnit *cUnit, MipsOpCode opc, int rs, int rt)
{
    MipsLIR *res;
    if (rt < 0) {
      assert(opc >= kMipsBeqz && opc <= kMipsBltzal);
      res = newLIR1(cUnit, opc, rs);
    } else  {
      assert(opc == kMipsBeq || opc == kMipsBne);
      res = newLIR2(cUnit, opc, rs, rt);
    }
    return res;
}

static MipsLIR *loadMultiple(CompilationUnit *cUnit, int rBase, int rMask);

static MipsLIR *opReg(CompilationUnit *cUnit, OpKind op, int rDestSrc)
{
    MipsOpCode opCode = kMipsNop;
    switch (op) {
        case kOpBlx:
            opCode = kMipsJalr;
            break;
        default:
            assert(0);
    }
    return newLIR2(cUnit, opCode, r_RA, rDestSrc);
}

static MipsLIR *opRegRegImm(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int value);
static MipsLIR *opRegImm(CompilationUnit *cUnit, OpKind op, int rDestSrc1,
                        int value)
{
    MipsLIR *res;
    bool neg = (value < 0);
    int absValue = (neg) ? -value : value;
    bool shortForm = (absValue & 0xff) == absValue;
    MipsOpCode opCode = kMipsNop;
    switch (op) {
        case kOpAdd:
            return opRegRegImm(cUnit, op, rDestSrc1, rDestSrc1, value);
            break;
        case kOpSub:
            return opRegRegImm(cUnit, op, rDestSrc1, rDestSrc1, value);
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

static MipsLIR *opRegRegReg(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int rSrc2)
{
    MipsOpCode opCode = kMipsNop;
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
            LOGE("Jit: bad case in opRegRegReg");
            dvmCompilerAbort(cUnit);
            break;
    }
    return newLIR3(cUnit, opCode, rDest, rSrc1, rSrc2);
}

static MipsLIR *opRegRegImm(CompilationUnit *cUnit, OpKind op, int rDest,
                           int rSrc1, int value)
{
    MipsLIR *res;
    MipsOpCode opCode = kMipsNop;
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

static MipsLIR *opRegReg(CompilationUnit *cUnit, OpKind op, int rDestSrc1,
                        int rSrc2)
{
    MipsLIR *res;
    MipsOpCode opCode = kMipsNop;
    switch (op) {
        case kOpMov:
            opCode = kMipsMove;
            break;
        case kOpMvn:
            return newLIR3(cUnit, kMipsNor, rDestSrc1, rSrc2, r_ZERO);
        case kOpNeg:
            return newLIR3(cUnit, kMipsSubu, rDestSrc1, r_ZERO, rSrc2);
        case kOpAdd:
        case kOpAnd:
        case kOpMul:
        case kOpOr:
        case kOpSub:
        case kOpXor:
            return opRegRegReg(cUnit, op, rDestSrc1, rDestSrc1, rSrc2);
        case kOp2Byte:
             return newLIR2(cUnit, kMipsSeb, rDestSrc1, rSrc2);
        case kOp2Short:
             return newLIR2(cUnit, kMipsSeh, rDestSrc1, rSrc2);
        case kOp2Char:
             return newLIR3(cUnit, kMipsAndi, rDestSrc1, rSrc2, 0xFFFF);
        default:
            LOGE("Jit: bad case in opRegReg");
            dvmCompilerAbort(cUnit);
            break;
    }
    return newLIR2(cUnit, opCode, rDestSrc1, rSrc2);
}

static MipsLIR *loadConstantValueWide(CompilationUnit *cUnit, int rDestLo,
                                     int rDestHi, int valLo, int valHi)
{
    MipsLIR *res;
    res = loadConstantNoClobber(cUnit, rDestLo, valLo);
    loadConstantNoClobber(cUnit, rDestHi, valHi);
    return res;
}

/* Load value from base + scaled index. */
static MipsLIR *loadBaseIndexed(CompilationUnit *cUnit, int rBase,
                               int rIndex, int rDest, int scale, OpSize size)
{
    MipsLIR *first = NULL;
    MipsLIR *res;
    MipsOpCode opCode = kMipsNop;
    int tReg = dvmCompilerAllocTemp(cUnit);

#ifdef __mips_hard_float
    if (FPREG(rDest)) {
        assert(SINGLEREG(rDest));
        assert((size == kWord) || (size == kSingle));
        size = kSingle;
    } else {
        if (size == kSingle)
            size = kWord;
    }
#endif

    if (!scale) {
        first = newLIR3(cUnit, kMipsAddu, tReg , rBase, rIndex);
    } else {
        first = opRegRegImm(cUnit, kOpLsl, tReg, rIndex, scale);
        newLIR3(cUnit, kMipsAddu, tReg , rBase, tReg);
    }

    switch (size) {
#ifdef __mips_hard_float
        case kSingle:
            opCode = kMipsFlwc1;
            break;
#endif
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
static MipsLIR *storeBaseIndexed(CompilationUnit *cUnit, int rBase,
                                int rIndex, int rSrc, int scale, OpSize size)
{
    MipsLIR *first = NULL;
    MipsLIR *res;
    MipsOpCode opCode = kMipsNop;
    int rNewIndex = rIndex;
    int tReg = dvmCompilerAllocTemp(cUnit);

#ifdef __mips_hard_float
    if (FPREG(rSrc)) {
        assert(SINGLEREG(rSrc));
        assert((size == kWord) || (size == kSingle));
        size = kSingle;
    } else {
        if (size == kSingle)
            size = kWord;
    }
#endif

    if (!scale) {
        first = newLIR3(cUnit, kMipsAddu, tReg , rBase, rIndex);
    } else {
        first = opRegRegImm(cUnit, kOpLsl, tReg, rIndex, scale);
        newLIR3(cUnit, kMipsAddu, tReg , rBase, tReg);
    }

    switch (size) {
#ifdef __mips_hard_float
        case kSingle:
            opCode = kMipsFswc1;
            break;
#endif
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

static MipsLIR *loadMultiple(CompilationUnit *cUnit, int rBase, int rMask)
{
    int i;
    int loadCnt = 0;
    MipsLIR *res = NULL ;
    genBarrier(cUnit);

    for (i = 0; i < 8; i++, rMask >>= 1) {
        if (rMask & 0x1) { /* map r0 to MIPS r_A0 */
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

static MipsLIR *storeMultiple(CompilationUnit *cUnit, int rBase, int rMask)
{
    int i;
    int storeCnt = 0;
    MipsLIR *res = NULL ;
    genBarrier(cUnit);

    for (i = 0; i < 8; i++, rMask >>= 1) {
        if (rMask & 0x1) { /* map r0 to MIPS r_A0 */
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

static MipsLIR *loadBaseDispBody(CompilationUnit *cUnit, MIR *mir, int rBase,
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
    MipsLIR *res;
    MipsLIR *load = NULL;
    MipsLIR *load2 = NULL;
    MipsOpCode opCode = kMipsNop;
    bool shortForm = IS_SIMM16(displacement);
    bool pair = false;

    switch (size) {
        case kLong:
        case kDouble:
            pair = true;
            opCode = kMipsLw;
#ifdef __mips_hard_float
            if (FPREG(rDest)) {
                opCode = kMipsFlwc1;
                if (DOUBLEREG(rDest)) {
                    rDest = rDest - FP_DOUBLE;
                } else {
                    assert(FPREG(rDestHi));
                    assert(rDest == (rDestHi - 1));
                }
                rDestHi = rDest + 1;
            }
#endif
            shortForm = IS_SIMM16_2WORD(displacement);
            assert((displacement & 0x3) == 0);
            break;
        case kWord:
        case kSingle:
            opCode = kMipsLw;
#ifdef __mips_hard_float
            if (FPREG(rDest)) {
                opCode = kMipsFlwc1;
                assert(SINGLEREG(rDest));
            }
#endif
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

static MipsLIR *loadBaseDisp(CompilationUnit *cUnit, MIR *mir, int rBase,
                            int displacement, int rDest, OpSize size,
                            int sReg)
{
    return loadBaseDispBody(cUnit, mir, rBase, displacement, rDest, -1,
                            size, sReg);
}

static MipsLIR *loadBaseDispWide(CompilationUnit *cUnit, MIR *mir, int rBase,
                                int displacement, int rDestLo, int rDestHi,
                                int sReg)
{
    return loadBaseDispBody(cUnit, mir, rBase, displacement, rDestLo, rDestHi,
                            kLong, sReg);
}

static MipsLIR *storeBaseDispBody(CompilationUnit *cUnit, int rBase,
                                 int displacement, int rSrc, int rSrcHi,
                                 OpSize size)
{
    MipsLIR *res;
    MipsLIR *store = NULL;
    MipsLIR *store2 = NULL;
    MipsOpCode opCode = kMipsNop;
    bool shortForm = IS_SIMM16(displacement);
    bool pair = false;

    switch (size) {
        case kLong:
        case kDouble:
            pair = true;
            opCode = kMipsSw;
#ifdef __mips_hard_float
            if (FPREG(rSrc)) {
                opCode = kMipsFswc1;
                if (DOUBLEREG(rSrc)) {
                    rSrc = rSrc - FP_DOUBLE;
                } else {
                    assert(FPREG(rSrcHi));
                    assert(rSrc == (rSrcHi - 1));
                }
                rSrcHi = rSrc + 1;
            }
#endif
            shortForm = IS_SIMM16_2WORD(displacement);
            assert((displacement & 0x3) == 0);
            break;
        case kWord:
        case kSingle:
            opCode = kMipsSw;
#ifdef __mips_hard_float
            if (FPREG(rSrc)) {
                opCode = kMipsFswc1;
                assert(SINGLEREG(rSrc));
            }
#endif
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

static MipsLIR *storeBaseDisp(CompilationUnit *cUnit, int rBase,
                             int displacement, int rSrc, OpSize size)
{
    return storeBaseDispBody(cUnit, rBase, displacement, rSrc, -1, size);
}

static MipsLIR *storeBaseDispWide(CompilationUnit *cUnit, int rBase,
                                 int displacement, int rSrcLo, int rSrcHi)
{
    return storeBaseDispBody(cUnit, rBase, displacement, rSrcLo, rSrcHi, kLong);
}

static void storePair(CompilationUnit *cUnit, int base, int lowReg, int highReg)
{
    storeWordDisp(cUnit, base, 0, lowReg);
    storeWordDisp(cUnit, base, 4, highReg);
}

static void loadPair(CompilationUnit *cUnit, int base, int lowReg, int highReg)
{
    loadWordDisp(cUnit, base, 0 , lowReg);
    loadWordDisp(cUnit, base, 4 , highReg);
}

static MipsLIR* genRegCopyNoInsert(CompilationUnit *cUnit, int rDest, int rSrc)
{
    MipsLIR* res;
    MipsOpCode opCode;
#ifdef __mips_hard_float
    if (FPREG(rDest) || FPREG(rSrc))
        return fpRegCopy(cUnit, rDest, rSrc);
#endif
    res = dvmCompilerNew(sizeof(MipsLIR), true);
    opCode = kMipsMove;
    assert(LOWREG(rDest) && LOWREG(rSrc));
    res->operands[0] = rDest;
    res->operands[1] = rSrc;
    res->opCode = opCode;
    setupResourceMasks(res);
    if (rDest == rSrc) {
        res->isNop = true;
    }
    return res;
}

static MipsLIR* genRegCopy(CompilationUnit *cUnit, int rDest, int rSrc)
{
    MipsLIR *res = genRegCopyNoInsert(cUnit, rDest, rSrc);
    dvmCompilerAppendLIR(cUnit, (LIR*)res);
    return res;
}

static void genRegCopyWide(CompilationUnit *cUnit, int destLo, int destHi,
                           int srcLo, int srcHi)
{
#ifdef __mips_hard_float
    bool destFP = FPREG(destLo) && FPREG(destHi);
    bool srcFP = FPREG(srcLo) && FPREG(srcHi);
    assert(FPREG(srcLo) == FPREG(srcHi));
    assert(FPREG(destLo) == FPREG(destHi));
    if (destFP) {
        if (srcFP) {
            genRegCopy(cUnit, S2D(destLo, destHi), S2D(srcLo, srcHi));
        } else {
           /* note the operands are swapped for the mtc1 instr */
            newLIR2(cUnit, kMipsMtc1, srcLo, destLo);
            newLIR2(cUnit, kMipsMtc1, srcHi, destHi);
        }
    } else {
        if (srcFP) {
            newLIR2(cUnit, kMipsMfc1, destLo, srcLo);
            newLIR2(cUnit, kMipsMfc1, destHi, srcHi);
        } else {
            // Handle overlap
            if (srcHi == destLo) {
                genRegCopy(cUnit, destHi, srcHi);
                genRegCopy(cUnit, destLo, srcLo);
            } else {
                genRegCopy(cUnit, destLo, srcLo);
                genRegCopy(cUnit, destHi, srcHi);
            }
        }
    }
#else
    // Handle overlap
    if (srcHi == destLo) {
        genRegCopy(cUnit, destHi, srcHi);
        genRegCopy(cUnit, destLo, srcLo);
    } else {
        genRegCopy(cUnit, destLo, srcLo);
        genRegCopy(cUnit, destHi, srcHi);
    }
#endif
}

static inline MipsLIR *genRegImmCheck(CompilationUnit *cUnit,
                                     MipsConditionCode cond, int reg,
                                     int checkValue, int dOffset,
                                     MipsLIR *pcrLabel)
{
    MipsLIR *branch = NULL;

    if (checkValue == 0) {
        MipsOpCode opc = kMipsNop;
        if (cond == kMipsCondEq) {
            opc = kMipsBeqz;
        } else if (cond == kMipsCondLt || cond == kMipsCondMi) {
            opc = kMipsBltz;
        } else if (cond == kMipsCondLe) {
            opc = kMipsBlez;
        } else if (cond == kMipsCondGt) {
            opc = kMipsBgtz;
        } else if (cond == kMipsCondGe) {
            opc = kMipsBgez;
        } else {
            LOGE("Jit: bad case in genRegImmCheck");
            dvmCompilerAbort(cUnit);
        }
        branch = opCondBranchMips(cUnit, opc, reg, -1);
    } else if (IS_SIMM16(checkValue)) {
        if (cond == kMipsCondLt) {
            int tReg = dvmCompilerAllocTemp(cUnit);
            newLIR3(cUnit, kMipsSlti, tReg, reg, checkValue);       
            branch = opCondBranchMips(cUnit, kMipsBne, tReg, r_ZERO);
            dvmCompilerFreeTemp(cUnit, tReg);
        } else {
            LOGE("Jit: bad case in genRegImmCheck");
            dvmCompilerAbort(cUnit);
        } 
    } else {
        LOGE("Jit: bad case in genRegImmCheck");
        dvmCompilerAbort(cUnit);
    }

    return genCheckCommon(cUnit, dOffset, branch, pcrLabel);
}
