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

#include "../../CompilerInternals.h"
#include "dexdump/OpCodeNames.h"
#include "MipsLIR.h"

/* For dumping instructions */
#define MIPS_REG_COUNT 32
static const char *mipsRegName[MIPS_REG_COUNT] = { 
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

/*
 * Interpret a format string and build a string no longer than size
 * See format key in Assemble.c.
 */
static void buildInsnString(char *fmt, MipsLIR *lir, char* buf,
                            unsigned char *baseAddr, int size)
{
    int i;
    char *bufEnd = &buf[size-1];
    char *fmtEnd = &fmt[strlen(fmt)];
    char tbuf[256];
    char nc;
    while (fmt < fmtEnd) {
        int operand;
        if (*fmt == '!') {
            fmt++;
            assert(fmt < fmtEnd);
            nc = *fmt++;
            if (nc=='!') {
                strcpy(tbuf, "!");
            } else {
               assert(fmt < fmtEnd);
               assert((unsigned)(nc-'0') < 4);
               operand = lir->operands[nc-'0'];
               switch(*fmt++) {
                   case 'b':
                       strcpy(tbuf,"0000");
                       for (i=3; i>= 0; i--) {
                           tbuf[i] += operand & 1;
                           operand >>= 1;
                       }
                       break;
                   case 's':
                       sprintf(tbuf,"f%d",operand & FP_REG_MASK);
                       break;
                   case 'S':
                       sprintf(tbuf,"df%d",(operand & FP_REG_MASK) >> 1);
                       break;
                   case 'h':
                       sprintf(tbuf,"%04x", operand);
                       break;
                   case 'M':
                   case 'd':
                       sprintf(tbuf,"%d", operand);
                       break;
                   case 'D':
                       sprintf(tbuf,"%d", operand+1);
                       break;
                   case 'E':
                       sprintf(tbuf,"%d", operand*4);
                       break;
                   case 'F':
                       sprintf(tbuf,"%d", operand*2);
                       break;
                   case 'c':
                       switch (operand) {
                           case kMipsCondEq:
                               strcpy(tbuf, "eq");
                               break;
                           case kMipsCondNe:
                               strcpy(tbuf, "ne");
                               break;
                           case kMipsCondLt:
                               strcpy(tbuf, "lt");
                               break;
                           case kMipsCondGe:
                               strcpy(tbuf, "ge");
                               break;
                           case kMipsCondGt:
                               strcpy(tbuf, "gt");
                               break;
                           case kMipsCondLe:
                               strcpy(tbuf, "le");
                               break;
                           case kMipsCondCs:
                               strcpy(tbuf, "cs");
                               break;
                           case kMipsCondMi:
                               strcpy(tbuf, "mi");
                               break;
                           default:
                               strcpy(tbuf, "");
                               break;
                       }
                       break;
                   case 't':
                       sprintf(tbuf,"0x%08x",
                               (int) baseAddr + lir->generic.offset + 4 +
                               (operand << 2));
                       break;
                   case 'T':
                       sprintf(tbuf,"0x%08x",
                               (int) (operand << 2));
                       break;
                   case 'u': {
                       int offset_1 = lir->operands[0];
                       int offset_2 = NEXT_LIR(lir)->operands[0];
                       intptr_t target =
                           ((((intptr_t) baseAddr + lir->generic.offset + 4) &
                            ~3) + (offset_1 << 21 >> 9) + (offset_2 << 1)) &
                           0xfffffffc;
                       sprintf(tbuf, "%p", (void *) target);
                       break;
                    }

                   /* Nothing to print for BLX_2 */
                   case 'v':
                       strcpy(tbuf, "see above");
                       break;
                   case 'r':
                       assert(operand >= 0 && operand < MIPS_REG_COUNT);
                       strcpy(tbuf, mipsRegName[operand]);
                       break;
                   default:
                       strcpy(tbuf,"DecodeError");
                       break;
               }
               if (buf+strlen(tbuf) <= bufEnd) {
                   strcpy(buf, tbuf);
                   buf += strlen(tbuf);
               } else {
                   break;
               }
            }
        } else {
           *buf++ = *fmt++;
        }
        if (buf == bufEnd)
            break;
    }
    *buf = 0;
}

void dvmDumpResourceMask(LIR *lir, u8 mask, const char *prefix)
{
    char buf[256];
    buf[0] = 0;
    MipsLIR *mipsLIR = (MipsLIR *) lir;

    if (mask == ENCODE_ALL) {
        strcpy(buf, "all");
    } else {
        char num[8];
        int i;

        for (i = 0; i < kRegEnd; i++) {
            if (mask & (1ULL << i)) {
                sprintf(num, "%d ", i);
                strcat(buf, num);
            }
        }

        if (mask & ENCODE_CCODE) {
            strcat(buf, "cc ");
        }
        if (mask & ENCODE_FP_STATUS) {
            strcat(buf, "fpcc ");
        }
        if (mipsLIR && (mask & ENCODE_DALVIK_REG)) {
            sprintf(buf + strlen(buf), "dr%d%s", mipsLIR->aliasInfo & 0xffff,
                    (mipsLIR->aliasInfo & 0x80000000) ? "(+1)" : "");
        }
    }
    if (buf[0]) {
        LOGD("%s: %s", prefix, buf);
    }
}

/*
 * Debugging macros
 */
#define DUMP_RESOURCE_MASK(X)
#define DUMP_SSA_REP(X)

/* Pretty-print a LIR instruction */
void dvmDumpLIRInsn(LIR *arg, unsigned char *baseAddr)
{
    MipsLIR *lir = (MipsLIR *) arg;
    char buf[256];
    char opName[256];
    int offset = lir->generic.offset;
    int dest = lir->operands[0];
    u2 *cPtr = (u2*)baseAddr;
    const bool dumpNop = false;

    /* Handle pseudo-ops individually, and all regular insns as a group */
    switch(lir->opCode) {
        case kMipsChainingCellBottom:
            LOGD("-------- end of chaining cells (0x%04x)\n", offset);
            break;
        case kMipsPseudoBarrier:
            LOGD("-------- BARRIER");
            break;
        case kMipsPseudoExtended:
            /* intentional fallthrough */
        case kMipsPseudoSSARep:
            DUMP_SSA_REP(LOGD("-------- %s\n", (char *) dest));
            break;
        case kMipsPseudoTargetLabel:
            break;
        case kMipsPseudoChainingCellBackwardBranch:
            LOGD("-------- chaining cell (backward branch): 0x%04x\n", dest);
            break;
        case kMipsPseudoChainingCellNormal:
            LOGD("-------- chaining cell (normal): 0x%04x\n", dest);
            break;
        case kMipsPseudoChainingCellHot:
            LOGD("-------- chaining cell (hot): 0x%04x\n", dest);
            break;
        case kMipsPseudoChainingCellInvokePredicted:
            LOGD("-------- chaining cell (predicted)\n");
            break;
        case kMipsPseudoChainingCellInvokeSingleton:
            LOGD("-------- chaining cell (invoke singleton): %s/%p\n",
                 ((Method *)dest)->name,
                 ((Method *)dest)->insns);
            break;
        case kMipsPseudoEntryBlock:
            LOGD("-------- entry offset: 0x%04x\n", dest);
            break;
        case kMipsPseudoDalvikByteCodeBoundary:
            LOGD("-------- dalvik offset: 0x%04x @ %s\n", dest,
                 (char *) lir->operands[1]);
            break;
        case kMipsPseudoExitBlock:
            LOGD("-------- exit offset: 0x%04x\n", dest);
            break;
        case kMipsPseudoPseudoAlign4:
            LOGD("%p (%04x): .align4\n", baseAddr + offset, offset);
            break;
        case kMipsPseudoPCReconstructionCell:
            LOGD("-------- reconstruct dalvik PC : 0x%04x @ +0x%04x\n", dest,
                 lir->operands[1]);
            break;
        case kMipsPseudoPCReconstructionBlockLabel:
            /* Do nothing */
            break;
        case kMipsPseudoEHBlockLabel:
            LOGD("Exception_Handling:\n");
            break;
        case kMipsPseudoNormalBlockLabel:
            LOGD("L%#06x:\n", dest);
            break;
        default:
            if (lir->isNop && !dumpNop) {
                break;
            }
            buildInsnString(EncodingMap[lir->opCode].name, lir, opName,
                            baseAddr, 256);
            buildInsnString(EncodingMap[lir->opCode].fmt, lir, buf, baseAddr,
                            256);
            LOGD("%p (%04x): %-8s%s%s\n",
                 baseAddr + offset, offset, opName, buf,
                 lir->isNop ? "(nop)" : "");
            break;
    }

    if (lir->useMask && (!lir->isNop || dumpNop)) {
        DUMP_RESOURCE_MASK(dvmDumpResourceMask((LIR *) lir,
                                               lir->useMask, "use"));
    }
    if (lir->defMask && (!lir->isNop || dumpNop)) {
        DUMP_RESOURCE_MASK(dvmDumpResourceMask((LIR *) lir,
                                               lir->defMask, "def"));
    }
}

/* Dump instructions and constant pool contents */
void dvmCompilerCodegenDump(CompilationUnit *cUnit)
{
    LOGD("Dumping LIR insns\n");
    LIR *lirInsn;
    MipsLIR *mipsLIR;

    LOGD("installed code is at %p\n", cUnit->baseAddr);
    LOGD("total size is %d bytes\n", cUnit->totalSize);
    for (lirInsn = cUnit->firstLIRInsn; lirInsn; lirInsn = lirInsn->next) {
        dvmDumpLIRInsn(lirInsn, cUnit->baseAddr);
    }
    for (lirInsn = cUnit->wordList; lirInsn; lirInsn = lirInsn->next) {
        mipsLIR = (MipsLIR *) lirInsn;
        LOGD("%p (%04x): .word (0x%x)\n",
             (char*)cUnit->baseAddr + mipsLIR->generic.offset,
             mipsLIR->generic.offset,
             mipsLIR->operands[0]);
    }
}
