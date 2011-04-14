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

#include "Dalvik.h"
#include "libdex/OpCode.h"
#include "dexdump/OpCodeNames.h"

#include "../../CompilerInternals.h"
#include "MipsLIR.h"
#include <unistd.h>             /* for cacheflush */

/*
 * opcode: ArmOpCode enum
 * skeleton: pre-designated bit-pattern for this opcode
 * k0: key to applying ds/de
 * ds: dest start bit position
 * de: dest end bit position
 * k1: key to applying s1s/s1e
 * s1s: src1 start bit position
 * s1e: src1 end bit position
 * k2: key to applying s2s/s2e
 * s2s: src2 start bit position
 * s2e: src2 end bit position
 * operands: number of operands (for sanity check purposes)
 * name: mnemonic name
 * fmt: for pretty-printing
 */
#define ENCODING_MAP(opcode, skeleton, k0, ds, de, k1, s1s, s1e, k2, s2s, s2e, \
                     k3, k3s, k3e, flags, name, fmt, size) \
        {skeleton, {{k0, ds, de}, {k1, s1s, s1e}, {k2, s2s, s2e}, \
                    {k3, k3s, k3e}}, opcode, flags, name, fmt, size}

/* Instruction dump string format keys: !pf, where "!" is the start
 * of the key, "p" is which numeric operand to use and "f" is the
 * print format.
 *
 * [p]ositions:
 *     0 -> operands[0] (dest)
 *     1 -> operands[1] (src1)
 *     2 -> operands[2] (src2)
 *     3 -> operands[3] (extra)
 *
 * [f]ormats:
 *     h -> 4-digit hex
 *     d -> decimal
 *     E -> decimal*4
 *     F -> decimal*2
 *     c -> branch condition (beq, bne, etc.)
 *     t -> pc-relative target
 *     T -> pc-region target
 *     u -> 1st half of bl[x] target
 *     v -> 2nd half ob bl[x] target
 *     R -> register list
 *     s -> single precision floating point register
 *     S -> double precision floating point register
 *     m -> Thumb2 modified immediate
 *     n -> complimented Thumb2 modified immediate
 *     M -> Thumb2 16-bit zero-extended immediate
 *     b -> 4-digit binary
 *
 *  [!] escape.  To insert "!", use "!!"
 */
/* NOTE: must be kept in sync with enum ArmOpcode from ArmLIR.h */
ArmEncodingMap EncodingMap[kArmLast] = {
    ENCODING_MAP(kArm16BitData,    0x0000,
                 kFmtBitBlt, 15, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP, "data", "0x!0h(!0d)", 1),
    ENCODING_MAP(kThumbAdcRR,        0x4140,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | SETS_CCODES | USES_CCODES,
                 "adcs", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbAddRRI3,      0x1c00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "adds", "r!0d, r!1d, #!2d", 1),
    ENCODING_MAP(kThumbAddRI8,       0x3000,
                 kFmtBitBlt, 10, 8, kFmtBitBlt, 7, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE0 | SETS_CCODES,
                 "adds", "r!0d, r!0d, #!1d", 1),
    ENCODING_MAP(kThumbAddRRR,       0x1800,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE12 | SETS_CCODES,
                 "adds", "r!0d, r!1d, r!2d", 1),
    ENCODING_MAP(kThumbAddRRLH,     0x4440,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE01,
                 "add", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbAddRRHL,     0x4480,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE01,
                 "add", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbAddRRHH,     0x44c0,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE01,
                 "add", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbAddPcRel,    0xa000,
                 kFmtBitBlt, 10, 8, kFmtBitBlt, 7, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | IS_BRANCH,
                 "add", "r!0d, pc, #!1E", 1),
    ENCODING_MAP(kThumbAddSpRel,    0xa800,
                 kFmtBitBlt, 10, 8, kFmtUnused, -1, -1, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF_SP | REG_USE_SP,
                 "add", "r!0d, sp, #!2E", 1),
    ENCODING_MAP(kThumbAddSpI7,      0xb000,
                 kFmtBitBlt, 6, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | REG_DEF_SP | REG_USE_SP,
                 "add", "sp, #!0d*4", 1),
    ENCODING_MAP(kThumbAndRR,        0x4000,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | SETS_CCODES,
                 "ands", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbAsrRRI5,      0x1000,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "asrs", "r!0d, r!1d, #!2d", 1),
    ENCODING_MAP(kThumbAsrRR,        0x4100,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | SETS_CCODES,
                 "asrs", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbBCond,        0xd000,
                 kFmtBitBlt, 7, 0, kFmtBitBlt, 11, 8, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | USES_CCODES,
                 "b!1c", "!0t", 1),
    ENCODING_MAP(kThumbBUncond,      0xe000,
                 kFmtBitBlt, 10, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND | IS_BRANCH,
                 "b", "!0t", 1),
    ENCODING_MAP(kThumbBicRR,        0x4380,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | SETS_CCODES,
                 "bics", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbBkpt,          0xbe00,
                 kFmtBitBlt, 7, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH,
                 "bkpt", "!0d", 1),
    ENCODING_MAP(kThumbBlx1,         0xf000,
                 kFmtBitBlt, 10, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_DEF_LR,
                 "blx_1", "!0u", 1),
    ENCODING_MAP(kThumbBlx2,         0xe800,
                 kFmtBitBlt, 10, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_DEF_LR,
                 "blx_2", "!0v", 1),
    ENCODING_MAP(kThumbBl1,          0xf000,
                 kFmtBitBlt, 10, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_DEF_LR,
                 "bl_1", "!0u", 1),
    ENCODING_MAP(kThumbBl2,          0xf800,
                 kFmtBitBlt, 10, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_DEF_LR,
                 "bl_2", "!0v", 1),
    ENCODING_MAP(kThumbBlxR,         0x4780,
                 kFmtBitBlt, 6, 3, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_USE0 | IS_BRANCH | REG_DEF_LR,
                 "blx", "r!0d", 1),
    ENCODING_MAP(kThumbBx,            0x4700,
                 kFmtBitBlt, 6, 3, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH,
                 "bx", "r!0d", 1),
    ENCODING_MAP(kThumbCmnRR,        0x42c0,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE01 | SETS_CCODES,
                 "cmn", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbCmpRI8,       0x2800,
                 kFmtBitBlt, 10, 8, kFmtBitBlt, 7, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE0 | SETS_CCODES,
                 "cmp", "r!0d, #!1d", 1),
    ENCODING_MAP(kThumbCmpRR,        0x4280,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE01 | SETS_CCODES,
                 "cmp", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbCmpLH,        0x4540,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE01 | SETS_CCODES,
                 "cmp", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbCmpHL,        0x4580,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE01 | SETS_CCODES,
                 "cmp", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbCmpHH,        0x45c0,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE01 | SETS_CCODES,
                 "cmp", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbEorRR,        0x4040,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | SETS_CCODES,
                 "eors", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbLdmia,         0xc800,
                 kFmtBitBlt, 10, 8, kFmtBitBlt, 7, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE0 | REG_DEF_LIST1 | IS_LOAD,
                 "ldmia", "r!0d!!, <!1R>", 1),
    ENCODING_MAP(kThumbLdrRRI5,      0x6800,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldr", "r!0d, [r!1d, #!2E]", 1),
    ENCODING_MAP(kThumbLdrRRR,       0x5800,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldr", "r!0d, [r!1d, r!2d]", 1),
    ENCODING_MAP(kThumbLdrPcRel,    0x4800,
                 kFmtBitBlt, 10, 8, kFmtBitBlt, 7, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0 | REG_USE_PC
                 | IS_LOAD, "ldr", "r!0d, [pc, #!1E]", 1),
    ENCODING_MAP(kThumbLdrSpRel,    0x9800,
                 kFmtBitBlt, 10, 8, kFmtUnused, -1, -1, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0 | REG_USE_SP
                 | IS_LOAD, "ldr", "r!0d, [sp, #!2E]", 1),
    ENCODING_MAP(kThumbLdrbRRI5,     0x7800,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrb", "r!0d, [r!1d, #2d]", 1),
    ENCODING_MAP(kThumbLdrbRRR,      0x5c00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrb", "r!0d, [r!1d, r!2d]", 1),
    ENCODING_MAP(kThumbLdrhRRI5,     0x8800,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrh", "r!0d, [r!1d, #!2F]", 1),
    ENCODING_MAP(kThumbLdrhRRR,      0x5a00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrh", "r!0d, [r!1d, r!2d]", 1),
    ENCODING_MAP(kThumbLdrsbRRR,     0x5600,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrsb", "r!0d, [r!1d, r!2d]", 1),
    ENCODING_MAP(kThumbLdrshRRR,     0x5e00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrsh", "r!0d, [r!1d, r!2d]", 1),
    ENCODING_MAP(kThumbLslRRI5,      0x0000,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "lsls", "r!0d, r!1d, #!2d", 1),
    ENCODING_MAP(kThumbLslRR,        0x4080,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | SETS_CCODES,
                 "lsls", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbLsrRRI5,      0x0800,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "lsrs", "r!0d, r!1d, #!2d", 1),
    ENCODING_MAP(kThumbLsrRR,        0x40c0,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | SETS_CCODES,
                 "lsrs", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbMovImm,       0x2000,
                 kFmtBitBlt, 10, 8, kFmtBitBlt, 7, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0 | SETS_CCODES,
                 "movs", "r!0d, #!1d", 1),
    ENCODING_MAP(kThumbMovRR,        0x1c00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "movs", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbMovRR_H2H,    0x46c0,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mov", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbMovRR_H2L,    0x4640,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mov", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbMovRR_L2H,    0x4680,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mov", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbMul,           0x4340,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | SETS_CCODES,
                 "muls", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbMvn,           0x43c0,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "mvns", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbNeg,           0x4240,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "negs", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbOrr,           0x4300,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | SETS_CCODES,
                 "orrs", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbPop,           0xbc00,
                 kFmtBitBlt, 8, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_DEF_SP | REG_USE_SP | REG_DEF_LIST0
                 | IS_LOAD, "pop", "<!0R>", 1),
    ENCODING_MAP(kThumbPush,          0xb400,
                 kFmtBitBlt, 8, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_DEF_SP | REG_USE_SP | REG_USE_LIST0
                 | IS_STORE, "push", "<!0R>", 1),
    ENCODING_MAP(kThumbRorRR,        0x41c0,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | SETS_CCODES,
                 "rors", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbSbc,           0x4180,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE01 | USES_CCODES | SETS_CCODES,
                 "sbcs", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumbStmia,         0xc000,
                 kFmtBitBlt, 10, 8, kFmtBitBlt, 7, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0 | REG_USE0 | REG_USE_LIST1 | IS_STORE,
                 "stmia", "r!0d!!, <!1R>", 1),
    ENCODING_MAP(kThumbStrRRI5,      0x6000,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "str", "r!0d, [r!1d, #!2E]", 1),
    ENCODING_MAP(kThumbStrRRR,       0x5000,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE012 | IS_STORE,
                 "str", "r!0d, [r!1d, r!2d]", 1),
    ENCODING_MAP(kThumbStrSpRel,    0x9000,
                 kFmtBitBlt, 10, 8, kFmtUnused, -1, -1, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE0 | REG_USE_SP
                 | IS_STORE, "str", "r!0d, [sp, #!2E]", 1),
    ENCODING_MAP(kThumbStrbRRI5,     0x7000,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "strb", "r!0d, [r!1d, #!2d]", 1),
    ENCODING_MAP(kThumbStrbRRR,      0x5400,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE012 | IS_STORE,
                 "strb", "r!0d, [r!1d, r!2d]", 1),
    ENCODING_MAP(kThumbStrhRRI5,     0x8000,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "strh", "r!0d, [r!1d, #!2F]", 1),
    ENCODING_MAP(kThumbStrhRRR,      0x5200,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE012 | IS_STORE,
                 "strh", "r!0d, [r!1d, r!2d]", 1),
    ENCODING_MAP(kThumbSubRRI3,      0x1e00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "subs", "r!0d, r!1d, #!2d]", 1),
    ENCODING_MAP(kThumbSubRI8,       0x3800,
                 kFmtBitBlt, 10, 8, kFmtBitBlt, 7, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE0 | SETS_CCODES,
                 "subs", "r!0d, #!1d", 1),
    ENCODING_MAP(kThumbSubRRR,       0x1a00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE12 | SETS_CCODES,
                 "subs", "r!0d, r!1d, r!2d", 1),
    ENCODING_MAP(kThumbSubSpI7,      0xb080,
                 kFmtBitBlt, 6, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_DEF_SP | REG_USE_SP,
                 "sub", "sp, #!0d", 1),
    ENCODING_MAP(kThumbSwi,           0xdf00,
                 kFmtBitBlt, 7, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH,
                 "swi", "!0d", 1),
    ENCODING_MAP(kThumbTst,           0x4200,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | REG_USE01 | SETS_CCODES,
                 "tst", "r!0d, r!1d", 1),
    ENCODING_MAP(kThumb2Vldrs,       0xed900a00,
                 kFmtSfp, 22, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "vldr", "!0s, [r!1d, #!2E]", 2),
    ENCODING_MAP(kThumb2Vldrd,       0xed900b00,
                 kFmtDfp, 22, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "vldr", "!0S, [r!1d, #!2E]", 2),
    ENCODING_MAP(kThumb2Vmuls,        0xee200a00,
                 kFmtSfp, 22, 12, kFmtSfp, 7, 16, kFmtSfp, 5, 0,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE12,
                 "vmuls", "!0s, !1s, !2s", 2),
    ENCODING_MAP(kThumb2Vmuld,        0xee200b00,
                 kFmtDfp, 22, 12, kFmtDfp, 7, 16, kFmtDfp, 5, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "vmuld", "!0S, !1S, !2S", 2),
    ENCODING_MAP(kThumb2Vstrs,       0xed800a00,
                 kFmtSfp, 22, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "vstr", "!0s, [r!1d, #!2E]", 2),
    ENCODING_MAP(kThumb2Vstrd,       0xed800b00,
                 kFmtDfp, 22, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "vstr", "!0S, [r!1d, #!2E]", 2),
    ENCODING_MAP(kThumb2Vsubs,        0xee300a40,
                 kFmtSfp, 22, 12, kFmtSfp, 7, 16, kFmtSfp, 5, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "vsub", "!0s, !1s, !2s", 2),
    ENCODING_MAP(kThumb2Vsubd,        0xee300b40,
                 kFmtDfp, 22, 12, kFmtDfp, 7, 16, kFmtDfp, 5, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "vsub", "!0S, !1S, !2S", 2),
    ENCODING_MAP(kThumb2Vadds,        0xee300a00,
                 kFmtSfp, 22, 12, kFmtSfp, 7, 16, kFmtSfp, 5, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "vadd", "!0s, !1s, !2s", 2),
    ENCODING_MAP(kThumb2Vaddd,        0xee300b00,
                 kFmtDfp, 22, 12, kFmtDfp, 7, 16, kFmtDfp, 5, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "vadd", "!0S, !1S, !2S", 2),
    ENCODING_MAP(kThumb2Vdivs,        0xee800a00,
                 kFmtSfp, 22, 12, kFmtSfp, 7, 16, kFmtSfp, 5, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "vdivs", "!0s, !1s, !2s", 2),
    ENCODING_MAP(kThumb2Vdivd,        0xee800b00,
                 kFmtDfp, 22, 12, kFmtDfp, 7, 16, kFmtDfp, 5, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "vdivd", "!0S, !1S, !2S", 2),
    ENCODING_MAP(kThumb2VcvtIF,       0xeeb80ac0,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.f32", "!0s, !1s", 2),
    ENCODING_MAP(kThumb2VcvtID,       0xeeb80bc0,
                 kFmtDfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.f64", "!0S, !1s", 2),
    ENCODING_MAP(kThumb2VcvtFI,       0xeebd0ac0,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.s32.f32 ", "!0s, !1s", 2),
    ENCODING_MAP(kThumb2VcvtDI,       0xeebd0bc0,
                 kFmtSfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.s32.f64 ", "!0s, !1S", 2),
    ENCODING_MAP(kThumb2VcvtFd,       0xeeb70ac0,
                 kFmtDfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.f64.f32 ", "!0S, !1s", 2),
    ENCODING_MAP(kThumb2VcvtDF,       0xeeb70bc0,
                 kFmtSfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.f32.f64 ", "!0s, !1S", 2),
    ENCODING_MAP(kThumb2Vsqrts,       0xeeb10ac0,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vsqrt.f32 ", "!0s, !1s", 2),
    ENCODING_MAP(kThumb2Vsqrtd,       0xeeb10bc0,
                 kFmtDfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vsqrt.f64 ", "!0S, !1S", 2),
    ENCODING_MAP(kThumb2MovImmShift, 0xf04f0000, /* no setflags encoding */
                 kFmtBitBlt, 11, 8, kFmtModImm, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "mov", "r!0d, #!1m", 2),
    ENCODING_MAP(kThumb2MovImm16,       0xf2400000,
                 kFmtBitBlt, 11, 8, kFmtImm16, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "mov", "r!0d, #!1M", 2),
    ENCODING_MAP(kThumb2StrRRI12,       0xf8c00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "str", "r!0d,[r!1d, #!2d", 2),
    ENCODING_MAP(kThumb2LdrRRI12,       0xf8d00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldr", "r!0d,[r!1d, #!2d", 2),
    ENCODING_MAP(kThumb2StrRRI8Predec,       0xf8400c00,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 8, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "str", "r!0d,[r!1d, #-!2d]", 2),
    ENCODING_MAP(kThumb2LdrRRI8Predec,       0xf8500c00,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 8, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldr", "r!0d,[r!1d, #-!2d]", 2),
    ENCODING_MAP(kThumb2Cbnz,       0xb900, /* Note: does not affect flags */
                 kFmtBitBlt, 2, 0, kFmtImm6, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE0 | IS_BRANCH,
                 "cbnz", "r!0d,!1t", 1),
    ENCODING_MAP(kThumb2Cbz,       0xb100, /* Note: does not affect flags */
                 kFmtBitBlt, 2, 0, kFmtImm6, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE0 | IS_BRANCH,
                 "cbz", "r!0d,!1t", 1),
    ENCODING_MAP(kThumb2AddRRI12,       0xf2000000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtImm12, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1,/* Note: doesn't affect flags */
                 "add", "r!0d,r!1d,#!2d", 2),
    ENCODING_MAP(kThumb2MovRR,       0xea4f0000, /* no setflags encoding */
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 3, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mov", "r!0d, r!1d", 2),
    ENCODING_MAP(kThumb2Vmovs,       0xeeb00a40,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vmov.f32 ", " !0s, !1s", 2),
    ENCODING_MAP(kThumb2Vmovd,       0xeeb00b40,
                 kFmtDfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vmov.f64 ", " !0S, !1S", 2),
    ENCODING_MAP(kThumb2Ldmia,         0xe8900000,
                 kFmtBitBlt, 19, 16, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE0 | REG_DEF_LIST1 | IS_LOAD,
                 "ldmia", "r!0d!!, <!1R>", 2),
    ENCODING_MAP(kThumb2Stmia,         0xe8800000,
                 kFmtBitBlt, 19, 16, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE0 | REG_USE_LIST1 | IS_STORE,
                 "stmia", "r!0d!!, <!1R>", 2),
    ENCODING_MAP(kThumb2AddRRR,  0xeb100000, /* setflags encoding */
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtShift, -1, -1,
                 IS_QUAD_OP | REG_DEF0_USE12 | SETS_CCODES,
                 "adds", "r!0d, r!1d, r!2d", 2),
    ENCODING_MAP(kThumb2SubRRR,       0xebb00000, /* setflags enconding */
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtShift, -1, -1,
                 IS_QUAD_OP | REG_DEF0_USE12 | SETS_CCODES,
                 "subs", "r!0d, r!1d, r!2d", 2),
    ENCODING_MAP(kThumb2SbcRRR,       0xeb700000, /* setflags encoding */
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtShift, -1, -1,
                 IS_QUAD_OP | REG_DEF0_USE12 | USES_CCODES | SETS_CCODES,
                 "sbcs", "r!0d, r!1d, r!2d", 2),
    ENCODING_MAP(kThumb2CmpRR,       0xebb00f00,
                 kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0, kFmtShift, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_USE01 | SETS_CCODES,
                 "cmp", "r!0d, r!1d", 2),
    ENCODING_MAP(kThumb2SubRRI12,       0xf2a00000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtImm12, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1,/* Note: doesn't affect flags */
                 "sub", "r!0d,r!1d,#!2d", 2),
    ENCODING_MAP(kThumb2MvnImmShift,  0xf06f0000, /* no setflags encoding */
                 kFmtBitBlt, 11, 8, kFmtModImm, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "mvn", "r!0d, #!1n", 2),
    ENCODING_MAP(kThumb2Sel,       0xfaa0f080,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE12 | USES_CCODES,
                 "sel", "r!0d, r!1d, r!2d", 2),
    ENCODING_MAP(kThumb2Ubfx,       0xf3c00000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtLsb, -1, -1,
                 kFmtBWidth, 4, 0, IS_QUAD_OP | REG_DEF0_USE1,
                 "ubfx", "r!0d, r!1d, #!2d, #!3d", 2),
    ENCODING_MAP(kThumb2Sbfx,       0xf3400000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtLsb, -1, -1,
                 kFmtBWidth, 4, 0, IS_QUAD_OP | REG_DEF0_USE1,
                 "sbfx", "r!0d, r!1d, #!2d, #!3d", 2),
    ENCODING_MAP(kThumb2LdrRRR,    0xf8500000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldr", "r!0d,[r!1d, r!2d, LSL #!3d]", 2),
    ENCODING_MAP(kThumb2LdrhRRR,    0xf8300000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrh", "r!0d,[r!1d, r!2d, LSL #!3d]", 2),
    ENCODING_MAP(kThumb2LdrshRRR,    0xf9300000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrsh", "r!0d,[r!1d, r!2d, LSL #!3d]", 2),
    ENCODING_MAP(kThumb2LdrbRRR,    0xf8100000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrb", "r!0d,[r!1d, r!2d, LSL #!3d]", 2),
    ENCODING_MAP(kThumb2LdrsbRRR,    0xf9100000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrsb", "r!0d,[r!1d, r!2d, LSL #!3d]", 2),
    ENCODING_MAP(kThumb2StrRRR,    0xf8400000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_USE012 | IS_STORE,
                 "str", "r!0d,[r!1d, r!2d, LSL #!3d]", 2),
    ENCODING_MAP(kThumb2StrhRRR,    0xf8200000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_USE012 | IS_STORE,
                 "strh", "r!0d,[r!1d, r!2d, LSL #!3d]", 2),
    ENCODING_MAP(kThumb2StrbRRR,    0xf8000000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_USE012 | IS_STORE,
                 "strb", "r!0d,[r!1d, r!2d, LSL #!3d]", 2),
    ENCODING_MAP(kThumb2LdrhRRI12,       0xf8b00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrh", "r!0d,[r!1d, #!2d]", 2),
    ENCODING_MAP(kThumb2LdrshRRI12,       0xf9b00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrsh", "r!0d,[r!1d, #!2d]", 2),
    ENCODING_MAP(kThumb2LdrbRRI12,       0xf8900000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrb", "r!0d,[r!1d, #!2d]", 2),
    ENCODING_MAP(kThumb2LdrsbRRI12,       0xf9900000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrsb", "r!0d,[r!1d, #!2d]", 2),
    ENCODING_MAP(kThumb2StrhRRI12,       0xf8a00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "strh", "r!0d,[r!1d, #!2d]", 2),
    ENCODING_MAP(kThumb2StrbRRI12,       0xf8800000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "strb", "r!0d,[r!1d, #!2d]", 2),
    ENCODING_MAP(kThumb2Pop,           0xe8bd0000,
                 kFmtBitBlt, 15, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_DEF_SP | REG_USE_SP | REG_DEF_LIST0
                 | IS_LOAD, "pop", "<!0R>", 2),
    ENCODING_MAP(kThumb2Push,          0xe8ad0000,
                 kFmtBitBlt, 15, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_DEF_SP | REG_USE_SP | REG_USE_LIST0
                 | IS_STORE, "push", "<!0R>", 2),
    ENCODING_MAP(kThumb2CmpRI8, 0xf1b00f00,
                 kFmtBitBlt, 19, 16, kFmtModImm, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_USE0 | SETS_CCODES,
                 "cmp", "r!0d, #!1m", 2),
    ENCODING_MAP(kThumb2AdcRRR,  0xeb500000, /* setflags encoding */
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtShift, -1, -1,
                 IS_QUAD_OP | REG_DEF0_USE12 | SETS_CCODES,
                 "acds", "r!0d, r!1d, r!2d, shift !3d", 2),
    ENCODING_MAP(kThumb2AndRRR,  0xea000000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtShift, -1, -1, IS_QUAD_OP | REG_DEF0_USE12,
                 "and", "r!0d, r!1d, r!2d, shift !3d", 2),
    ENCODING_MAP(kThumb2BicRRR,  0xea200000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtShift, -1, -1, IS_QUAD_OP | REG_DEF0_USE12,
                 "bic", "r!0d, r!1d, r!2d, shift !3d", 2),
    ENCODING_MAP(kThumb2CmnRR,  0xeb000000,
                 kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0, kFmtShift, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "cmn", "r!0d, r!1d, shift !2d", 2),
    ENCODING_MAP(kThumb2EorRRR,  0xea800000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtShift, -1, -1, IS_QUAD_OP | REG_DEF0_USE12,
                 "eor", "r!0d, r!1d, r!2d, shift !3d", 2),
    ENCODING_MAP(kThumb2MulRRR,  0xfb00f000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "mul", "r!0d, r!1d, r!2d", 2),
    ENCODING_MAP(kThumb2MnvRR,  0xea6f0000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 3, 0, kFmtShift, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "mvn", "r!0d, r!1d, shift !2d", 2),
    ENCODING_MAP(kThumb2RsubRRI8,       0xf1d00000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "rsb", "r!0d,r!1d,#!2m", 2),
    ENCODING_MAP(kThumb2NegRR,       0xf1d00000, /* instance of rsub */
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "neg", "r!0d,r!1d", 2),
    ENCODING_MAP(kThumb2OrrRRR,  0xea400000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtShift, -1, -1, IS_QUAD_OP | REG_DEF0_USE12,
                 "orr", "r!0d, r!1d, r!2d, shift !3d", 2),
    ENCODING_MAP(kThumb2TstRR,       0xea100f00,
                 kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0, kFmtShift, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_USE01 | SETS_CCODES,
                 "tst", "r!0d, r!1d, shift !2d", 2),
    ENCODING_MAP(kThumb2LslRRR,  0xfa00f000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "lsl", "r!0d, r!1d, r!2d", 2),
    ENCODING_MAP(kThumb2LsrRRR,  0xfa20f000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "lsr", "r!0d, r!1d, r!2d", 2),
    ENCODING_MAP(kThumb2AsrRRR,  0xfa40f000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "asr", "r!0d, r!1d, r!2d", 2),
    ENCODING_MAP(kThumb2RorRRR,  0xfa60f000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "ror", "r!0d, r!1d, r!2d", 2),
    ENCODING_MAP(kThumb2LslRRI5,  0xea4f0000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 3, 0, kFmtShift5, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "lsl", "r!0d, r!1d, #!2d", 2),
    ENCODING_MAP(kThumb2LsrRRI5,  0xea4f0010,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 3, 0, kFmtShift5, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "lsr", "r!0d, r!1d, #!2d", 2),
    ENCODING_MAP(kThumb2AsrRRI5,  0xea4f0020,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 3, 0, kFmtShift5, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "asr", "r!0d, r!1d, #!2d", 2),
    ENCODING_MAP(kThumb2RorRRI5,  0xea4f0030,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 3, 0, kFmtShift5, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "ror", "r!0d, r!1d, #!2d", 2),
    ENCODING_MAP(kThumb2BicRRI8,  0xf0200000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "bic", "r!0d, r!1d, #!2m", 2),
    ENCODING_MAP(kThumb2AndRRI8,  0xf0000000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "and", "r!0d, r!1d, #!2m", 2),
    ENCODING_MAP(kThumb2OrrRRI8,  0xf0400000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "orr", "r!0d, r!1d, #!2m", 2),
    ENCODING_MAP(kThumb2EorRRI8,  0xf0800000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "eor", "r!0d, r!1d, #!2m", 2),
    ENCODING_MAP(kThumb2AddRRI8,  0xf1100000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "adds", "r!0d, r!1d, #!2m", 2),
    ENCODING_MAP(kThumb2AdcRRI8,  0xf1500000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES | USES_CCODES,
                 "adcs", "r!0d, r!1d, #!2m", 2),
    ENCODING_MAP(kThumb2SubRRI8,  0xf1b00000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "subs", "r!0d, r!1d, #!2m", 2),
    ENCODING_MAP(kThumb2SbcRRI8,  0xf1700000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES | USES_CCODES,
                 "sbcs", "r!0d, r!1d, #!2m", 2),
    ENCODING_MAP(kThumb2It,  0xbf00,
                 kFmtBitBlt, 7, 4, kFmtBitBlt, 3, 0, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_IT | USES_CCODES,
                 "it:!1b", "!0c", 1),
    ENCODING_MAP(kThumb2Fmstat,  0xeef1fa10,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND | SETS_CCODES,
                 "fmstat", "", 2),
    ENCODING_MAP(kThumb2Vcmpd,        0xeeb40b40,
                 kFmtDfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE01,
                 "vcmp.f64", "!0S, !1S", 2),
    ENCODING_MAP(kThumb2Vcmps,        0xeeb40a40,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE01,
                 "vcmp.f32", "!0s, !1s", 2),
    ENCODING_MAP(kThumb2LdrPcRel12,       0xf8df0000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 11, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0 | REG_USE_PC | IS_LOAD,
                 "ldr", "r!0d,[rpc, #!1d]", 2),
    ENCODING_MAP(kThumb2BCond,        0xf0008000,
                 kFmtBrOffset, -1, -1, kFmtBitBlt, 25, 22, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | IS_BRANCH | USES_CCODES,
                 "b!1c", "!0t", 2),
    ENCODING_MAP(kThumb2Vmovd_RR,       0xeeb00b40,
                 kFmtDfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vmov.f64", "!0S, !1S", 2),
    ENCODING_MAP(kThumb2Vmovs_RR,       0xeeb00a40,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vmov.f32", "!0s, !1s", 2),
    ENCODING_MAP(kThumb2Fmrs,       0xee100a10,
                 kFmtBitBlt, 15, 12, kFmtSfp, 7, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "fmrs", "r!0d, !1s", 2),
    ENCODING_MAP(kThumb2Fmsr,       0xee000a10,
                 kFmtSfp, 7, 16, kFmtBitBlt, 15, 12, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "fmsr", "!0s, r!1d", 2),
    ENCODING_MAP(kThumb2Fmrrd,       0xec500b10,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtDfp, 5, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF01_USE2,
                 "fmrrd", "r!0d, r!1d, !2S", 2),
    ENCODING_MAP(kThumb2Fmdrr,       0xec400b10,
                 kFmtDfp, 5, 0, kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "fmdrr", "!0S, r!1d, r!2d", 2),
    ENCODING_MAP(kThumb2Vabsd,       0xeeb00bc0,
                 kFmtDfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vabs.f64", "!0S, !1S", 2),
    ENCODING_MAP(kThumb2Vabss,       0xeeb00ac0,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vabs.f32", "!0s, !1s", 2),
    ENCODING_MAP(kThumb2Vnegd,       0xeeb10b40,
                 kFmtDfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vneg.f64", "!0S, !1S", 2),
    ENCODING_MAP(kThumb2Vnegs,       0xeeb10a40,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vneg.f32", "!0s, !1s", 2),
    ENCODING_MAP(kThumb2Vmovs_IMM8,       0xeeb00a00,
                 kFmtSfp, 22, 12, kFmtFPImm, 16, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "vmov.f32", "!0s, #0x!1h", 2),
    ENCODING_MAP(kThumb2Vmovd_IMM8,       0xeeb00b00,
                 kFmtDfp, 22, 12, kFmtFPImm, 16, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "vmov.f64", "!0S, #0x!1h", 2),
    ENCODING_MAP(kThumb2Mla,  0xfb000000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 15, 12,
                 IS_QUAD_OP | REG_DEF0 | REG_USE1 | REG_USE2 | REG_USE3,
                 "mla", "r!0d, r!1d, r!2d, r!3d", 2),
    ENCODING_MAP(kThumb2Umull,  0xfba00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16,
                 kFmtBitBlt, 3, 0,
                 IS_QUAD_OP | REG_DEF0 | REG_DEF1 | REG_USE2 | REG_USE3,
                 "umull", "r!0d, r!1d, r!2d, r!3d", 2),
    ENCODING_MAP(kThumb2Ldrex,       0xe8500f00,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrex", "r!0d,[r!1d, #!2E]", 2),
    ENCODING_MAP(kThumb2Strex,       0xe8400000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16,
                 kFmtBitBlt, 7, 0, IS_QUAD_OP | REG_DEF0_USE12 | IS_STORE,
                 "strex", "r!0d,r!1d, [r!2d, #!2E]", 2),
    ENCODING_MAP(kThumb2Clrex,       0xf3bf8f2f,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND,
                 "clrex", "", 2),
    ENCODING_MAP(kThumb2Bfi,         0xf3600000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtShift5, -1, -1,
                 kFmtBitBlt, 4, 0, IS_QUAD_OP | REG_DEF0_USE1,
                 "bfi", "r!0d,r!1d,#!2d,#!3d", 2),
    ENCODING_MAP(kThumb2Bfc,         0xf36f0000,
                 kFmtBitBlt, 11, 8, kFmtShift5, -1, -1, kFmtBitBlt, 4, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0,
                 "bfc", "r!0d,#!1d,#!2d", 2),

    ENCODING_MAP(kMips32BitData, 0x00000000,
                 kFmtBitBlt, 31, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP,
                 "data", "0x!0h(!0d)", 2),
    ENCODING_MAP(kMipsAddiu, 0x24000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "addiu", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsAddu, 0x00000021,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "addu", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsAnd, 0x00000024,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "and", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsAndi, 0x30000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "andi", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsB, 0x10000000,
                 kFmtBitBlt, 15, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND | IS_BRANCH,
                 "b", "!0t", 2),
    ENCODING_MAP(kMipsBal, 0x04110000,
                 kFmtBitBlt, 15, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND | IS_BRANCH | REG_DEF_LR, 
                 "bal", "!0t", 2),
    ENCODING_MAP(kMipsBeq, 0x10000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, 
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_USE01,
                 "beq", "!0r,!1r,!2t", 2),
    ENCODING_MAP(kMipsBeqz, 0x10000000, /* same as beq above with t = $zero */
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "beqz", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBgez, 0x04010000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "bgez", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBgtz, 0x1C000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "bgtz", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBlez, 0x18000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "blez", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBltz, 0x04000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0,
                 "bltz", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBltzal, 0x04100000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0 | REG_DEF_LR,
                 "bltzal", "!0r,!1t", 2),
    ENCODING_MAP(kMipsBne, 0x14000000,
                 kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, 
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_USE01,
                 "bne", "!0r,!1r,!2t", 2),
    ENCODING_MAP(kMipsDiv, 0x0000001a,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtBitBlt, 25, 21,
                 kFmtBitBlt, 20, 16, IS_QUAD_OP | REG_DEF01 | REG_USE23,
                 "div", "!2r,!3r", 2),
    ENCODING_MAP(kMipsExt, 0x7c000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 10, 6,
                 kFmtBitBlt, 15, 11, IS_QUAD_OP | REG_DEF0 | REG_USE1,
                 "ext", "!0r,!1r,!2d,!3D", 2),
    ENCODING_MAP(kMipsJal, 0x0c000000,
                 kFmtBitBlt, 25, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_DEF_LR,
                 "jal", "!0T(!0E)", 2),
    ENCODING_MAP(kMipsJalr, 0x00000009,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | REG_DEF0_USE1,
                 "jalr", "!0r,!1r", 2),
    ENCODING_MAP(kMipsJr, 0x00000008,
                 kFmtBitBlt, 25, 21, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH | REG_USE0 | REG_DEF_LR,
                 "jr", "!0r", 2),
    ENCODING_MAP(kMipsLahi, 0x3C000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "lahi/lui", "!0r,0x!1h(!1d)", 2),
    ENCODING_MAP(kMipsLalo, 0x34000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "lalo/ori", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsLui, 0x3C000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "lui", "!0r,0x!1h(!1d)", 2),
    ENCODING_MAP(kMipsLb, 0x80000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lb", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsLbu, 0x90000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lbu", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsLh, 0x84000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lh", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsLhu, 0x94000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lhu", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsLw, 0x8C000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE2 | IS_LOAD,
                 "lw", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsMfhi, 0x00000010,
                 kFmtBitBlt, 15, 11, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mfhi", "!0r", 2),
    ENCODING_MAP(kMipsMflo, 0x00000012,
                 kFmtBitBlt, 15, 11, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mflo", "!0r", 2),
    ENCODING_MAP(kMipsMove, 0x00000025, /* or using zero reg */
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "move", "!0r,!1r", 2),
    ENCODING_MAP(kMipsMovz, 0x0000000a,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "movz", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsMul, 0x70000002,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "mul", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsNop, 0x00000000,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND,
                 "nop", "", 2),
    ENCODING_MAP(kMipsNor, 0x00000027, /* used for "not" too */
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "nor", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsOr, 0x00000025,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "or", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsOri, 0x34000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "ori", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsSb, 0xA0000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sb", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsSh, 0xA4000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sh", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsSll, 0x00000000,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "sll", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsSllv, 0x00000004,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sllv", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSlt, 0x0000002a,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "slt", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSlti, 0x28000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "slti", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsSltu, 0x0000002b,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sltu", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSra, 0x00000003,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "sra", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsSrav, 0x00000007,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "srav", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSrl, 0x00000002,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "srl", "!0r,!1r,0x!2h(!2d)", 2),
    ENCODING_MAP(kMipsSrlv, 0x00000006,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "srlv", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSubu, 0x00000023, /* used for "neg" too */
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "subu", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsSw, 0xAC000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 15, 0, kFmtBitBlt, 25, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE02 | IS_STORE,
                 "sw", "!0r,!1d(!2r)", 2),
    ENCODING_MAP(kMipsXor, 0x00000026,
                 kFmtBitBlt, 15, 11, kFmtBitBlt, 25, 21, kFmtBitBlt, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "xor", "!0r,!1r,!2r", 2),
    ENCODING_MAP(kMipsXori, 0x38000000,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 25, 21, kFmtBitBlt, 15, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "xori", "!0r,!1r,0x!2h(!2d)", 2),
};

/*
 * The fake NOP of moving r0 to r0 actually will incur data stalls if r0 is
 * not ready. Since r5 (rFP) is not updated often, it is less likely to
 * generate unnecessary stall cycles.
 */
#define PADDING_MOV_R5_R5               0x1C2D

/* Write the numbers in the literal pool to the codegen stream */
static void installDataContent(CompilationUnit *cUnit)
{
    int *dataPtr = (int *) ((char *) cUnit->baseAddr + cUnit->dataOffset);
    ArmLIR *dataLIR = (ArmLIR *) cUnit->wordList;
assert(!dataLIR); /* DRP review installDataContent() */
    while (dataLIR) {
        *dataPtr++ = dataLIR->operands[0];
        dataLIR = NEXT_LIR(dataLIR);
    }
}

/* Returns the size of a Jit trace description */
static int jitTraceDescriptionSize(const JitTraceDescription *desc)
{
assert(1); /* DRP verify jitTraceDescriptionSize() */
    int runCount;
    for (runCount = 0; ; runCount++) {
        if (desc->trace[runCount].frag.runEnd)
           break;
    }
    return sizeof(JitCodeDesc) + ((runCount+1) * sizeof(JitTraceRun));
}

/* Return TRUE if error happens */
static bool assembleInstructions(CompilationUnit *cUnit, intptr_t startAddr)
{
assert(1); /* DRP cleanup assembleInstructions() */
    int *bufferAddr = (int *) cUnit->codeBuffer;
    ArmLIR *lir;

    for (lir = (ArmLIR *) cUnit->firstLIRInsn; lir; lir = NEXT_LIR(lir)) {
if (lir->opCode > kMipsPseudoNormalBlockLabel && lir->opCode < kMipsFirst) {
   LOGD("DRP assembleInstruction lir->opCode (arm opcode) 0x%x", (int)lir->opCode); 
}
        if (lir->opCode < 0) {
assert(lir->opCode <= kMipsPseudoNormalBlockLabel); /* DRP only MIPS ok */
            if ((lir->opCode == kArmPseudoPseudoAlign4) &&
                /* 1 means padding is needed */
                (lir->operands[0] == 1)) {
assert(0); /* DRP cleanup/remove obsolete kArmPseudoAlign4 */
                *bufferAddr++ = PADDING_MOV_R5_R5;
            }
            continue;
        }

assert(lir->opCode >= kMipsFirst); /* DRP only MIPS ok */

        if (lir->isNop) {
            continue;
        }

        if (lir->opCode == kThumbLdrPcRel ||
            lir->opCode == kThumb2LdrPcRel12 ||
            lir->opCode == kThumbAddPcRel ||
            ((lir->opCode == kThumb2Vldrs) && (lir->operands[1] == rpc))) {
            ArmLIR *lirTarget = (ArmLIR *) lir->generic.target;
            intptr_t pc = (lir->generic.offset + 4) & ~3;
            /*
             * Allow an offset (stored in operands[2] to be added to the
             * PC-relative target. Useful to get to a fixed field inside a
             * chaining cell.
             */
            intptr_t target = lirTarget->generic.offset + lir->operands[2];
            int delta = target - pc;
            if (delta & 0x3) {
                LOGE("PC-rel distance is not multiples of 4: %d\n", delta);
                dvmCompilerAbort(cUnit);
            }
            if ((lir->opCode == kThumb2LdrPcRel12) && (delta > 4091)) {
                return true;
            } else if (delta > 1020) {
                return true;
            }
            if (lir->opCode == kThumb2Vldrs) {
                lir->operands[2] = delta >> 2;
            } else {
                lir->operands[1] = (lir->opCode == kThumb2LdrPcRel12) ?
                                    delta : delta >> 2;
            }
        } else if (lir->opCode == kThumb2Cbnz || lir->opCode == kThumb2Cbz) {
            ArmLIR *targetLIR = (ArmLIR *) lir->generic.target;
            intptr_t pc = lir->generic.offset + 4;
            intptr_t target = targetLIR->generic.offset;
            int delta = target - pc;
            if (delta > 126 || delta < 0) {
                /*
                 * TODO: allow multiple kinds of assembler failure to allow
                 * change of code patterns when things don't fit.
                 */
                return true;
            } else {
                lir->operands[1] = delta >> 1;
            }
        } else if (lir->opCode == kThumbBCond ||
                   lir->opCode == kThumb2BCond) {
            ArmLIR *targetLIR = (ArmLIR *) lir->generic.target;
            intptr_t pc = lir->generic.offset + 4;
            intptr_t target = targetLIR->generic.offset;
            int delta = target - pc;
            if ((lir->opCode == kThumbBCond) && (delta > 254 || delta < -256)) {
                return true;
            }
            lir->operands[0] = delta >> 1;
        } else if (lir->opCode == kThumbBUncond) {
            ArmLIR *targetLIR = (ArmLIR *) lir->generic.target;
            intptr_t pc = lir->generic.offset + 4;
            intptr_t target = targetLIR->generic.offset;
            int delta = target - pc;
            if (delta > 2046 || delta < -2048) {
                LOGE("Unconditional branch distance out of range: %d\n", delta);
                dvmCompilerAbort(cUnit);
            }
            lir->operands[0] = delta >> 1;
        } else if (lir->opCode == kMipsB || lir->opCode == kMipsBal) {
            ArmLIR *targetLIR = (ArmLIR *) lir->generic.target;
            intptr_t pc = lir->generic.offset + 4;
            intptr_t target = targetLIR->generic.offset;
            int delta = target - pc;
            if (delta & 0x3) {
                LOGE("PC-rel distance is not multiple of 4: %d\n", delta);
                dvmAbort();
            }
            if (delta > 131068 || delta < -131069) {
                LOGE("Unconditional branch distance out of range: %d\n", delta);
                dvmAbort();
            }
            lir->operands[0] = delta >> 2;
#if 0
        } else if (lir->opCode == kMipsBeqz || lir->opCode == kMipsBeqzal ||
                   lir->opCode == kMipsBgtz || lir->opCode == kMipsBlez ||
                   lir->opCode == kMipsBltz || lir->opCode == kMipsBltzal) {
#endif
        } else if (lir->opCode >= kMipsBeqz && lir->opCode <= kMipsBltzal) {
            ArmLIR *targetLIR = (ArmLIR *) lir->generic.target;
            intptr_t pc = lir->generic.offset + 4;
            intptr_t target = targetLIR->generic.offset;
            int delta = target - pc;
            if (delta & 0x3) {
                LOGE("PC-rel distance is not multiple of 4: %d\n", delta);
                dvmAbort();
            }
            if (delta > 131068 || delta < -131069) {
                LOGE("Conditional branch distance out of range: %d\n", delta);
                dvmAbort();
            }
            lir->operands[1] = delta >> 2;
        } else if (lir->opCode == kMipsBeq || lir->opCode == kMipsBne) {
            ArmLIR *targetLIR = (ArmLIR *) lir->generic.target;
            intptr_t pc = lir->generic.offset + 4;
            intptr_t target = targetLIR->generic.offset;
            int delta = target - pc;
            if (delta & 0x3) {
                LOGE("PC-rel distance is not multiple of 4: %d\n", delta);
                dvmAbort();
            }
            if (delta > 131068 || delta < -131069) {
                LOGE("Conditional branch distance out of range: %d\n", delta);
                dvmAbort();
            }
            lir->operands[2] = delta >> 2;
        } else if (lir->opCode == kMipsJal) {
            intptr_t curPC = (startAddr + lir->generic.offset + 4) & ~3;
            intptr_t target = lir->operands[0];
            /* ensure PC-region branch can be used */
            assert((curPC & 0xF0000000) == (target & 0xF0000000));
            if (target & 0x3) {
                LOGE("Jump target is not multiple of 4: %d\n", target);
                dvmAbort();
            }
            lir->operands[0] =  target >> 2;
        } else if (lir->opCode == kMipsLahi) { /* load address hi (via lui) */
            ArmLIR *targetLIR = (ArmLIR *) lir->generic.target;
            intptr_t target = startAddr + targetLIR->generic.offset;
            lir->operands[1] = target >> 16;
        } else if (lir->opCode == kMipsLalo) { /* load address lo (via ori) */
            ArmLIR *targetLIR = (ArmLIR *) lir->generic.target;
            intptr_t target = startAddr + targetLIR->generic.offset;
            lir->operands[2] = lir->operands[2] + target; 
        } else if (lir->opCode == kThumbBlx1) {
            assert(NEXT_LIR(lir)->opCode == kThumbBlx2);
            /* curPC is Thumb */
            intptr_t curPC = (startAddr + lir->generic.offset + 4) & ~3;
            intptr_t target = lir->operands[1];
            /* Match bit[1] in target with base */
            if (curPC & 0x2) {
                target |= 0x2;
            }
            int delta = target - curPC;
            assert((delta >= -(1<<22)) && (delta <= ((1<<22)-2)));

            lir->operands[0] = (delta >> 12) & 0x7ff;
            NEXT_LIR(lir)->operands[0] = (delta>> 1) & 0x7ff;
        }


        ArmEncodingMap *encoder = &EncodingMap[lir->opCode];
        u4 bits = encoder->skeleton;
        int i;
        for (i = 0; i < 4; i++) {
            u4 operand;
            u4 value;
            operand = lir->operands[i];
            switch(encoder->fieldLoc[i].kind) {
                case kFmtUnused:
                    break;
                case kFmtFPImm:
                    value = ((operand & 0xF0) >> 4) << encoder->fieldLoc[i].end;
                    value |= (operand & 0x0F) << encoder->fieldLoc[i].start;
                    bits |= value;
                    break;
                case kFmtBrOffset:
                    /*
                     * NOTE: branch offsets are not handled here, but
                     * in the main assembly loop (where label values
                     * are known).  For reference, here is what the
                     * encoder handing would be:
                         value = ((operand  & 0x80000) >> 19) << 26;
                         value |= ((operand & 0x40000) >> 18) << 11;
                         value |= ((operand & 0x20000) >> 17) << 13;
                         value |= ((operand & 0x1f800) >> 11) << 16;
                         value |= (operand  & 0x007ff);
                         bits |= value;
                     */
                    break;
                case kFmtShift5:
                    value = ((operand & 0x1c) >> 2) << 12;
                    value |= (operand & 0x03) << 6;
                    bits |= value;
                    break;
                case kFmtShift:
                    value = ((operand & 0x70) >> 4) << 12;
                    value |= (operand & 0x0f) << 4;
                    bits |= value;
                    break;
                case kFmtBWidth:
                    value = operand - 1;
                    bits |= value;
                    break;
                case kFmtLsb:
                    value = ((operand & 0x1c) >> 2) << 12;
                    value |= (operand & 0x03) << 6;
                    bits |= value;
                    break;
                case kFmtImm6:
                    value = ((operand & 0x20) >> 5) << 9;
                    value |= (operand & 0x1f) << 3;
                    bits |= value;
                    break;
                case kFmtBitBlt:
                    /* ??? DRP test 003 004 023 028 030 etc fail w/o this */
                    if (encoder->fieldLoc[i].start == 0 && encoder->fieldLoc[i].end == 31) {
                        value = operand;
                    } else {
                        value = (operand << encoder->fieldLoc[i].start) &
                                ((1 << (encoder->fieldLoc[i].end + 1)) - 1);
                    }
                    bits |= value;
                    break;
                case kFmtDfp: {
                    assert(DOUBLEREG(operand));
                    assert((operand & 0x1) == 0);
                    int regName = (operand & FP_REG_MASK) >> 1;
                    /* Snag the 1-bit slice and position it */
                    value = ((regName & 0x10) >> 4) <<
                            encoder->fieldLoc[i].end;
                    /* Extract and position the 4-bit slice */
                    value |= (regName & 0x0f) <<
                            encoder->fieldLoc[i].start;
                    bits |= value;
                    break;
                }
                case kFmtSfp:
                    assert(SINGLEREG(operand));
                    /* Snag the 1-bit slice and position it */
                    value = (operand & 0x1) <<
                            encoder->fieldLoc[i].end;
                    /* Extract and position the 4-bit slice */
                    value |= ((operand & 0x1e) >> 1) <<
                            encoder->fieldLoc[i].start;
                    bits |= value;
                    break;
                case kFmtImm12:
                case kFmtModImm:
                    value = ((operand & 0x800) >> 11) << 26;
                    value |= ((operand & 0x700) >> 8) << 12;
                    value |= operand & 0x0ff;
                    bits |= value;
                    break;
                case kFmtImm16:
                    value = ((operand & 0x0800) >> 11) << 26;
                    value |= ((operand & 0xf000) >> 12) << 16;
                    value |= ((operand & 0x0700) >> 8) << 12;
                    value |= operand & 0x0ff;
                    bits |= value;
                    break;
                default:
                    assert(0);
            }
        }
        assert(encoder->size == 2);
        *bufferAddr++ = bits;
    }
    return false;
}

#if defined(SIGNATURE_BREAKPOINT)
/* Inspect the assembled instruction stream to find potential matches */
static void matchSignatureBreakpoint(const CompilationUnit *cUnit,
                                     unsigned int size)
{
assert(0); /* DRP retarg func */
    unsigned int i, j;
    u4 *ptr = (u4 *) cUnit->codeBuffer;

    for (i = 0; i < size - gDvmJit.signatureBreakpointSize + 1; i++) {
        if (ptr[i] == gDvmJit.signatureBreakpoint[0]) {
            for (j = 1; j < gDvmJit.signatureBreakpointSize; j++) {
                if (ptr[i+j] != gDvmJit.signatureBreakpoint[j]) {
                    break;
                }
            }
            if (j == gDvmJit.signatureBreakpointSize) {
                LOGD("Signature match starting from offset %#x (%d words)",
                     i*4, gDvmJit.signatureBreakpointSize);
                int descSize = jitTraceDescriptionSize(cUnit->traceDesc);
                JitTraceDescription *newCopy =
                    (JitTraceDescription *) malloc(descSize);
                memcpy(newCopy, cUnit->traceDesc, descSize);
                dvmCompilerWorkEnqueue(NULL, kWorkOrderTraceDebug, newCopy);
                break;
            }
        }
    }
}
#endif

/*
 * Translation layout in the code cache.  Note that the codeAddress pointer
 * in JitTable will point directly to the code body (field codeAddress).  The
 * chain cell offset codeAddress - 4, and (if present) executionCount is at
 * codeAddress - 8.
 *
 *      +----------------------------+
 *      | Execution count            |  -> [Optional] 4 bytes
 *      +----------------------------+
 *   +--| Offset to chain cell counts|  -> 4 bytes
 *   |  +----------------------------+
 *   |  | Code body                  |  -> Start address for translation
 *   |  |                            |     variable in 2-byte chunks
 *   |  .                            .     (JitTable's codeAddress points here)
 *   |  .                            .
 *   |  |                            |
 *   |  +----------------------------+
 *   |  | Chaining Cells             |  -> 16 bytes each, must be 4 byte aligned
 *   |  .                            .
 *   |  .                            .
 *   |  |                            |
 *   |  +----------------------------+
 *   |  | Gap for large switch stmt  |  -> # cases >= MAX_CHAINED_SWITCH_CASES
 *   |  +----------------------------+
 *   +->| Chaining cell counts       |  -> 8 bytes, chain cell counts by type
 *      +----------------------------+
 *      | Trace description          |  -> variable sized
 *      .                            .
 *      |                            |
 *      +----------------------------+
 *      | Literal pool               |  -> 4-byte aligned, variable size
 *      .                            .
 *      .                            .
 *      |                            |
 *      +----------------------------+
 *
 * Go over each instruction in the list and calculate the offset from the top
 * before sending them off to the assembler. If out-of-range branch distance is
 * seen rearrange the instructions a bit to correct it.
 */
void dvmCompilerAssembleLIR(CompilationUnit *cUnit, JitTranslationInfo *info)
{
assert(1); /* DRP verify dvmCompilerAssembleLIR() */
    LIR *lir;
    ArmLIR *armLIR;
    int offset = 0;
    int i;
    ChainCellCounts chainCellCounts;
    int descSize = jitTraceDescriptionSize(cUnit->traceDesc);
    int chainingCellGap;

    info->instructionSet = cUnit->instructionSet;

    /* Beginning offset needs to allow space for chain cell offset */
    for (armLIR = (ArmLIR *) cUnit->firstLIRInsn;
         armLIR;
         armLIR = NEXT_LIR(armLIR)) {
        armLIR->generic.offset = offset;
        if (armLIR->opCode >= 0 && !armLIR->isNop) {
            armLIR->size = EncodingMap[armLIR->opCode].size * 2;
            offset += armLIR->size;
        } else if (armLIR->opCode == kArmPseudoPseudoAlign4) {
assert(0); /* DRP cleanup/remove obsolete pseudo align */
            if (offset & 0x2) {
                offset += 2;
                armLIR->operands[0] = 1;
            } else {
                armLIR->operands[0] = 0;
            }
        }
        /* Pseudo opcodes don't consume space */
    }

    /* Const values have to be word aligned */
    offset = (offset + 3) & ~3;

    /*
     * Get the gap (# of u4) between the offset of chaining cell count and
     * the bottom of real chaining cells. If the translation has chaining
     * cells, the gap is guaranteed to be multiples of 4.
     */
    chainingCellGap = (offset - cUnit->chainingCellBottom->offset) >> 2;

    /* Add space for chain cell counts & trace description */
    u4 chainCellOffset = offset;
    ArmLIR *chainCellOffsetLIR = (ArmLIR *) cUnit->chainCellOffsetLIR;
    assert(chainCellOffsetLIR);
    assert(chainCellOffset < 0x10000U);
    assert(chainCellOffsetLIR->opCode == kMips32BitData &&
           chainCellOffsetLIR->operands[0] == CHAIN_CELL_OFFSET_TAG);

    /*
     * Replace the CHAIN_CELL_OFFSET_TAG with the real value. If trace
     * profiling is enabled, subtract 4 (occupied by the counter word) from
     * the absolute offset as the value stored in chainCellOffsetLIR is the
     * delta from &chainCellOffsetLIR to &ChainCellCounts.
     */
    chainCellOffsetLIR->operands[0] =
        gDvmJit.profile ? (chainCellOffset - 4) : chainCellOffset;

    offset += sizeof(chainCellCounts) + descSize;

    assert((offset & 0x3) == 0);  /* Should still be word aligned */

    /* Set up offsets for literals */
    cUnit->dataOffset = offset;

    for (lir = cUnit->wordList; lir; lir = lir->next) {
        lir->offset = offset;
        offset += 4;
    }

    cUnit->totalSize = offset;

    if (gDvmJit.codeCacheByteUsed + cUnit->totalSize > gDvmJit.codeCacheSize) {
        gDvmJit.codeCacheFull = true;
        cUnit->baseAddr = NULL;
        return;
    }

    /* Allocate enough space for the code block */
    cUnit->codeBuffer = dvmCompilerNew(chainCellOffset, true);
    if (cUnit->codeBuffer == NULL) {
        LOGE("Code buffer allocation failure\n");
        cUnit->baseAddr = NULL;
        return;
    }

    bool assemblerFailure = assembleInstructions(
        cUnit, (intptr_t) gDvmJit.codeCache + gDvmJit.codeCacheByteUsed);

    /*
     * Currently the only reason that can cause the assembler to fail is due to
     * trace length - cut it in half and retry.
     */
    if (assemblerFailure) {
        cUnit->halveInstCount = true;
        return;
    }

#if defined(SIGNATURE_BREAKPOINT)
    if (info->discardResult == false && gDvmJit.signatureBreakpoint != NULL &&
        chainCellOffset/4 >= gDvmJit.signatureBreakpointSize) {
        matchSignatureBreakpoint(cUnit, chainCellOffset/4);
    }
#endif

    /* Don't go all the way if the goal is just to get the verbose output */
    if (info->discardResult) return;

    cUnit->baseAddr = (char *) gDvmJit.codeCache + gDvmJit.codeCacheByteUsed;
    gDvmJit.codeCacheByteUsed += offset;

    /* Install the code block */
    memcpy((char*)cUnit->baseAddr, cUnit->codeBuffer, chainCellOffset);
    gDvmJit.numCompilations++;

    /* Install the chaining cell counts */
    for (i=0; i< kChainingCellGap; i++) {
        chainCellCounts.u.count[i] = cUnit->numChainingCells[i];
    }

    /* Set the gap number in the chaining cell count structure */
    chainCellCounts.u.count[kChainingCellGap] = chainingCellGap;

    memcpy((char*)cUnit->baseAddr + chainCellOffset, &chainCellCounts,
           sizeof(chainCellCounts));

    /* Install the trace description */
    memcpy((char*)cUnit->baseAddr + chainCellOffset + sizeof(chainCellCounts),
           cUnit->traceDesc, descSize);

    /* Write the literals directly into the code cache */
    installDataContent(cUnit);

    /* Flush dcache and invalidate the icache to maintain coherence */
    cacheflush((long)cUnit->baseAddr,
               (long)((char *) cUnit->baseAddr + offset), 0);

    /* Record code entry point and instruction set */
    info->codeAddress = (char*)cUnit->baseAddr + cUnit->headerSize;
}

/*
 * Returns the skeleton bit pattern associated with an opcode.  All
 * variable fields are zeroed.
 */
static u4 getSkeleton(ArmOpCode op)
{
assert(1); /* DRP verify getSkeleton() */
    return EncodingMap[op].skeleton;
}

static u4 assembleChainingBranch(int branchOffset, bool thumbTarget)
{
assert(1); /* DRP verify assembleChainingBranch() */
    return getSkeleton(kMipsJal) | ((branchOffset & 0x0FFFFFFF) >> 2);
}

/*
 * Perform translation chain operation.
 * For ARM, we'll use a pair of thumb instructions to generate
 * an unconditional chaining branch of up to 4MB in distance.
 * Use a BL, because the generic "interpret" translation needs
 * the link register to find the dalvik pc of teh target.
 *     111HHooooooooooo
 * Where HH is 10 for the 1st inst, and 11 for the second and
 * the "o" field is each instruction's 11-bit contribution to the
 * 22-bit branch offset.
 * If the target is nearby, use a single-instruction bl.
 * If one or more threads is suspended, don't chain.
 */
void* dvmJitChain(void* tgtAddr, u4* branchAddr)
{
assert(1); /* DRP verify dvmJitChain() */
    u4 newInst;

    /*
     * Only chain translations when there is no urge to ask all threads to
     * suspend themselves via the interpreter.
     */
    if ((gDvmJit.pProfTable != NULL) && (gDvm.sumThreadSuspendCount == 0) &&
        (gDvmJit.codeCacheFull == false)) {
        /* ensure PC-region branch can be used */
        assert((((intptr_t) tgtAddr) & 0xF0000000) == 
               (((intptr_t) branchAddr) & 0xF0000000));

        gDvmJit.translationChains++;

        COMPILER_TRACE_CHAINING(
            LOGD("Jit Runtime: chaining 0x%x to 0x%x\n",
                 (int) branchAddr, (int) tgtAddr & -2));

        newInst = assembleChainingBranch((int) tgtAddr & -2, 0);
        *branchAddr = newInst;
        cacheflush((long)branchAddr, (long)branchAddr + 4, 0);
        gDvmJit.hasNewChain = true;
    }

    return tgtAddr;
}

/*
 * Attempt to enqueue a work order to patch an inline cache for a predicted
 * chaining cell for virtual/interface calls.
 */
static bool inlineCachePatchEnqueue(PredictedChainingCell *cellAddr,
                                    PredictedChainingCell *newContent)
{
assert(1); /* DRP verify inlineCachePatchEnqueue() */
    bool result = true;

    /*
     * Make sure only one thread gets here since updating the cell (ie fast
     * path and queueing the request (ie the queued path) have to be done
     * in an atomic fashion.
     */
    dvmLockMutex(&gDvmJit.compilerICPatchLock);

    /* Fast path for uninitialized chaining cell */
    if (cellAddr->clazz == NULL &&
        cellAddr->branch == PREDICTED_CHAIN_BX_PAIR_INIT) {
        cellAddr->method = newContent->method;
        cellAddr->branch = newContent->branch;
        cellAddr->delay_slot = newContent->delay_slot;
        cellAddr->counter = newContent->counter;
        /*
         * The update order matters - make sure clazz is updated last since it
         * will bring the uninitialized chaining cell to life.
         */
        MEM_BARRIER();
        cellAddr->clazz = newContent->clazz;
        cacheflush((intptr_t) cellAddr, (intptr_t) (cellAddr+1), 0);
#if defined(WITH_JIT_TUNING)
        gDvmJit.icPatchFast++;
#endif
    }
    /*
     * Otherwise the patch request will be queued and handled in the next
     * GC cycle. At that time all other mutator threads are suspended so
     * there will be no partial update in the inline cache state.
     */
    else if (gDvmJit.compilerICPatchIndex < COMPILER_IC_PATCH_QUEUE_SIZE)  {
        int index = gDvmJit.compilerICPatchIndex++;
        gDvmJit.compilerICPatchQueue[index].cellAddr = cellAddr;
        gDvmJit.compilerICPatchQueue[index].cellContent = *newContent;
#if defined(WITH_JIT_TUNING)
        gDvmJit.icPatchQueued++;
#endif
    }
    /* Queue is full - just drop this patch request */
    else {
        result = false;
#if defined(WITH_JIT_TUNING)
        gDvmJit.icPatchDropped++;
#endif
    }

    dvmUnlockMutex(&gDvmJit.compilerICPatchLock);
    return result;
}

/*
 * This method is called from the invoke templates for virtual and interface
 * methods to speculatively setup a chain to the callee. The templates are
 * written in assembly and have setup method, cell, and clazz at r0, r2, and
 * r3 respectively, so there is a unused argument in the list. Upon return one
 * of the following three results may happen:
 *   1) Chain is not setup because the callee is native. Reset the rechain
 *      count to a big number so that it will take a long time before the next
 *      rechain attempt to happen.
 *   2) Chain is not setup because the callee has not been created yet. Reset
 *      the rechain count to a small number and retry in the near future.
 *   3) Ask all other threads to stop before patching this chaining cell.
 *      This is required because another thread may have passed the class check
 *      but hasn't reached the chaining cell yet to follow the chain. If we
 *      patch the content before halting the other thread, there could be a
 *      small window for race conditions to happen that it may follow the new
 *      but wrong chain to invoke a different method.
 */
const Method *dvmJitToPatchPredictedChain(const Method *method,
                                          void *unused,
                                          PredictedChainingCell *cell,
                                          const ClassObject *clazz)
{
assert(1); /* DRP in progress dvmJitToPatchPredictedChain() */
#if defined(WITH_SELF_VERIFICATION)
    /* Disable chaining and prevent this from triggering again for a while */
    cell->counter = PREDICTED_CHAIN_COUNTER_AVOID;
    cacheflush((long) cell, (long) (cell+1), 0);
    goto done;
#else
    /* Don't come back here for a long time if the method is native */
    if (dvmIsNativeMethod(method)) {
        cell->counter = PREDICTED_CHAIN_COUNTER_AVOID;
        cacheflush((long) cell, (long) (cell+1), 0);
        COMPILER_TRACE_CHAINING(
            LOGD("Jit Runtime: predicted chain %p to native method %s ignored",
                 cell, method->name));
        goto done;
    }
    int tgtAddr = (int) dvmJitGetCodeAddr(method->insns);

    /*
     * Compilation not made yet for the callee. Reset the counter to a small
     * value and come back to check soon.
     */
    if ((tgtAddr == 0) || ((void*)tgtAddr == gDvmJit.interpretTemplate)) {
        /*
         * Wait for a few invocations (currently set to be 16) before trying
         * to setup the chain again.
         */
        cell->counter = PREDICTED_CHAIN_COUNTER_DELAY;
        cacheflush((long) cell, (long) (cell+1), 0);
        COMPILER_TRACE_CHAINING(
            LOGD("Jit Runtime: predicted chain %p to method %s%s delayed",
                 cell, method->clazz->descriptor, method->name));
        goto done;
    }

    PredictedChainingCell newCell;

    /* Avoid back-to-back orders to the same cell */
    cell->counter = PREDICTED_CHAIN_COUNTER_AVOID;

    int baseAddr = (int) cell + 4;   // PC is cur_addr + 4
    /* ensure PC-region branch can be used */
    assert((baseAddr & 0xF0000000) == (tgtAddr & 0xF0000000));
    newCell.branch = assembleChainingBranch(tgtAddr, 0);
    newCell.delay_slot = getSkeleton(kMipsNop);
    newCell.clazz = clazz;
    newCell.method = method;
    newCell.counter = PREDICTED_CHAIN_COUNTER_RECHAIN;

    /*
     * Enter the work order to the queue and the chaining cell will be patched
     * the next time a safe point is entered.
     *
     * If the enqueuing fails reset the rechain count to a normal value so that
     * it won't get indefinitely delayed.
     */
    if (!inlineCachePatchEnqueue(cell, &newCell)) {
        cell->counter = PREDICTED_CHAIN_COUNTER_RECHAIN;
    }
#endif
done:
    return method;
}

/*
 * Patch the inline cache content based on the content passed from the work
 * order.
 */
void dvmCompilerPatchInlineCache(void)
{
assert(1); /* DRP retarg dvmCompilerPatchInlineCache() */
    int i;
    PredictedChainingCell *minAddr, *maxAddr;

    /* Nothing to be done */
    if (gDvmJit.compilerICPatchIndex == 0) return;

    /*
     * Since all threads are already stopped we don't really need to acquire
     * the lock. But race condition can be easily introduced in the future w/o
     * paying attention so we still acquire the lock here.
     */
    dvmLockMutex(&gDvmJit.compilerICPatchLock);

    //LOGD("Number of IC patch work orders: %d", gDvmJit.compilerICPatchIndex);

    /* Initialize the min/max address range */
    minAddr = (PredictedChainingCell *)
        ((char *) gDvmJit.codeCache + gDvmJit.codeCacheSize);
    maxAddr = (PredictedChainingCell *) gDvmJit.codeCache;

    for (i = 0; i < gDvmJit.compilerICPatchIndex; i++) {
        PredictedChainingCell *cellAddr =
            gDvmJit.compilerICPatchQueue[i].cellAddr;
        PredictedChainingCell *cellContent =
            &gDvmJit.compilerICPatchQueue[i].cellContent;

        if (cellAddr->clazz == NULL) {
            COMPILER_TRACE_CHAINING(
                LOGD("Jit Runtime: predicted chain %p to %s (%s) initialized",
                     cellAddr,
                     cellContent->clazz->descriptor,
                     cellContent->method->name));
        } else {
            COMPILER_TRACE_CHAINING(
                LOGD("Jit Runtime: predicted chain %p from %s to %s (%s) "
                     "patched",
                     cellAddr,
                     cellAddr->clazz->descriptor,
                     cellContent->clazz->descriptor,
                     cellContent->method->name));
        }

        /* Patch the chaining cell */
        *cellAddr = *cellContent;
        minAddr = (cellAddr < minAddr) ? cellAddr : minAddr;
        maxAddr = (cellAddr > maxAddr) ? cellAddr : maxAddr;
    }

    /* Then synchronize the I/D cache */
    cacheflush((long) minAddr, (long) (maxAddr+1), 0);

    gDvmJit.compilerICPatchIndex = 0;
    dvmUnlockMutex(&gDvmJit.compilerICPatchLock);
}

/*
 * Unchain a trace given the starting address of the translation
 * in the code cache.  Refer to the diagram in dvmCompilerAssembleLIR.
 * Returns the address following the last cell unchained.  Note that
 * the incoming codeAddr is a thumb code address, and therefore has
 * the low bit set.
 */
u4* dvmJitUnchain(void* codeAddr)
{
assert(1); /* DRP in progress verify dvmJitUnchain() */
    u4* pChainCellOffset = (u2*)((char*)codeAddr - 4);
    u4 chainCellOffset = *pChainCellOffset;
    ChainCellCounts *pChainCellCounts =
          (ChainCellCounts*)((char*)codeAddr + chainCellOffset - 4);
    int cellSize;
    u4* pChainCells;
    u4* pStart;
    u4 thumb1;
    u4 thumb2;
    u4 newInst;
    int i,j;
    PredictedChainingCell *predChainCell;

    /* Get total count of chain cells */
    for (i = 0, cellSize = 0; i < kChainingCellGap; i++) {
        if (i != kChainingCellInvokePredicted) {
            cellSize += pChainCellCounts->u.count[i] * 4;
        } else {
            cellSize += pChainCellCounts->u.count[i] * 5;
        }
    }

    if (cellSize == 0)
        return (u4 *) pChainCellCounts;

    /* Locate the beginning of the chain cell region */
    pStart = pChainCells = ((u4 *) pChainCellCounts) - cellSize -
             pChainCellCounts->u.count[kChainingCellGap];

    /* The cells are sorted in order - walk through them and reset */
    for (i = 0; i < kChainingCellGap; i++) {
        int elemSize = 4; /* Most chaining cell has four words */
        if (i == kChainingCellInvokePredicted) {
            elemSize = 5;
        }

        for (j = 0; j < pChainCellCounts->u.count[i]; j++) {
            int targetOffset;
            switch(i) {
                case kChainingCellNormal:
                    targetOffset = offsetof(InterpState,
                          jitToInterpEntries.dvmJitToInterpNormal);
                    break;
                case kChainingCellHot:
                case kChainingCellInvokeSingleton:
                    targetOffset = offsetof(InterpState,
                          jitToInterpEntries.dvmJitToInterpTraceSelect);
                    break;
                case kChainingCellInvokePredicted:
                    targetOffset = 0;
                    predChainCell = (PredictedChainingCell *) pChainCells;
                    /*
                     * There could be a race on another mutator thread to use
                     * this particular predicted cell and the check has passed
                     * the clazz comparison. So we cannot safely wipe the
                     * method and branch but it is safe to clear the clazz,
                     * which serves as the key.
                     */
                    predChainCell->clazz = PREDICTED_CHAIN_CLAZZ_INIT;
                    break;
#if defined(WITH_SELF_VERIFICATION)
                case kChainingCellBackwardBranch:
                    targetOffset = offsetof(InterpState,
                          jitToInterpEntries.dvmJitToInterpBackwardBranch);
                    break;
#elif defined(WITH_JIT_TUNING)
                case kChainingCellBackwardBranch:
                    targetOffset = offsetof(InterpState,
                          jitToInterpEntries.dvmJitToInterpNormal);
                    break;
#endif
                default:
                    targetOffset = 0; // make gcc happy
                    LOGE("Unexpected chaining type: %d", i);
                    dvmAbort();  // dvmAbort OK here - can't safely recover
            }
            COMPILER_TRACE_CHAINING(
                LOGD("Jit Runtime: unchaining 0x%x", (int)pChainCells));
            /*
             * Code sequence for a chaining cell is:
             *     lw   a0, offset(rGLUE) 
             *     jalr ra, a0
             */
            if (i != kChainingCellInvokePredicted) {
                *pChainCells = getSkeleton(kMipsLw) | (r_A0 << 16) | 
                               targetOffset | (rGLUE << 21);
                *(pChainCells+1) = getSkeleton(kMipsJalr) | (r_RA << 11) | 
                                   (r_A0 << 21);
            }
            pChainCells += elemSize;  /* Advance by a fixed number of words */
        }
    }
    return pChainCells;
}

/* Unchain all translation in the cache. */
void dvmJitUnchainAll()
{
assert(1); /* DRP verify dvmJitUnchainAll() */
    u4* lowAddress = NULL;
    u4* highAddress = NULL;
    unsigned int i;
    if (gDvmJit.pJitEntryTable != NULL) {
        COMPILER_TRACE_CHAINING(LOGD("Jit Runtime: unchaining all"));
        dvmLockMutex(&gDvmJit.tableLock);
        for (i = 0; i < gDvmJit.jitTableSize; i++) {
            if (gDvmJit.pJitEntryTable[i].dPC &&
                   gDvmJit.pJitEntryTable[i].codeAddress &&
                   (gDvmJit.pJitEntryTable[i].codeAddress !=
                    gDvmJit.interpretTemplate)) {
                u4* lastAddress;
                lastAddress =
                      dvmJitUnchain(gDvmJit.pJitEntryTable[i].codeAddress);
                if (lowAddress == NULL ||
                      (u4*)gDvmJit.pJitEntryTable[i].codeAddress < lowAddress)
                    lowAddress = lastAddress;
                if (lastAddress > highAddress)
                    highAddress = lastAddress;
            }
        }
        cacheflush((long)lowAddress, (long)highAddress, 0);
        dvmUnlockMutex(&gDvmJit.tableLock);
        gDvmJit.translationChains = 0;
    }
    gDvmJit.hasNewChain = false;
}

typedef struct jitProfileAddrToLine {
    u4 lineNum;
    u4 bytecodeOffset;
} jitProfileAddrToLine;


/* Callback function to track the bytecode offset/line number relationiship */
static int addrToLineCb (void *cnxt, u4 bytecodeOffset, u4 lineNum)
{
assert(0); /* DRP retarg func */
    jitProfileAddrToLine *addrToLine = (jitProfileAddrToLine *) cnxt;

    /* Best match so far for this offset */
    if (addrToLine->bytecodeOffset >= bytecodeOffset) {
        addrToLine->lineNum = lineNum;
    }
    return 0;
}

char *getTraceBase(const JitEntry *p)
{
assert(0); /* DRP retarg func */
    return (char*)p->codeAddress -
        (6 + (p->u.info.instructionSet == DALVIK_JIT_ARM ? 0 : 1));
}

/* Dumps profile info for a single trace */
static int dumpTraceProfile(JitEntry *p, bool silent, bool reset,
                            unsigned long sum)
{
assert(0); /* DRP retarg func */
    ChainCellCounts* pCellCounts;
    char* traceBase;
    u4* pExecutionCount;
    u4 executionCount;
    u2* pCellOffset;
    JitTraceDescription *desc;
    const Method* method;

    traceBase = getTraceBase(p);

    if (p->codeAddress == NULL) {
        if (!silent)
            LOGD("TRACEPROFILE 0x%08x 0 NULL 0 0", (int)traceBase);
        return 0;
    }
    if (p->codeAddress == gDvmJit.interpretTemplate) {
        if (!silent)
            LOGD("TRACEPROFILE 0x%08x 0 INTERPRET_ONLY  0 0", (int)traceBase);
        return 0;
    }

    pExecutionCount = (u4*) (traceBase);
    executionCount = *pExecutionCount;
    if (reset) {
        *pExecutionCount =0;
    }
    if (silent) {
        return executionCount;
    }
    pCellOffset = (u2*) (traceBase + 4);
    pCellCounts = (ChainCellCounts*) ((char *)pCellOffset + *pCellOffset);
    desc = (JitTraceDescription*) ((char*)pCellCounts + sizeof(*pCellCounts));
    method = desc->method;
    char *methodDesc = dexProtoCopyMethodDescriptor(&method->prototype);
    jitProfileAddrToLine addrToLine = {0, desc->trace[0].frag.startOffset};

    /*
     * We may end up decoding the debug information for the same method
     * multiple times, but the tradeoff is we don't need to allocate extra
     * space to store the addr/line mapping. Since this is a debugging feature
     * and done infrequently so the slower but simpler mechanism should work
     * just fine.
     */
    dexDecodeDebugInfo(method->clazz->pDvmDex->pDexFile,
                       dvmGetMethodCode(method),
                       method->clazz->descriptor,
                       method->prototype.protoIdx,
                       method->accessFlags,
                       addrToLineCb, NULL, &addrToLine);

    LOGD("TRACEPROFILE 0x%08x % 10d %5.2f%% [%#x(+%d), %d] %s%s;%s",
         (int)traceBase,
         executionCount,
         ((float ) executionCount) / sum * 100.0,
         desc->trace[0].frag.startOffset,
         desc->trace[0].frag.numInsts,
         addrToLine.lineNum,
         method->clazz->descriptor, method->name, methodDesc);
    free(methodDesc);

    return executionCount;
}

/* Create a copy of the trace descriptor of an existing compilation */
JitTraceDescription *dvmCopyTraceDescriptor(const u2 *pc,
                                            const JitEntry *knownEntry)
{
assert(0); /* DRP retarg func */
    const JitEntry *jitEntry = knownEntry ? knownEntry : dvmFindJitEntry(pc);
    if (jitEntry == NULL) return NULL;

    /* Find out the startint point */
    char *traceBase = getTraceBase(jitEntry);

    /* Then find out the starting point of the chaining cell */
    u2 *pCellOffset = (u2*) (traceBase + 4);
    ChainCellCounts *pCellCounts =
        (ChainCellCounts*) ((char *)pCellOffset + *pCellOffset);

    /* From there we can find out the starting point of the trace descriptor */
    JitTraceDescription *desc =
        (JitTraceDescription*) ((char*)pCellCounts + sizeof(*pCellCounts));

    /* Now make a copy and return */
    int descSize = jitTraceDescriptionSize(desc);
    JitTraceDescription *newCopy = (JitTraceDescription *) malloc(descSize);
    memcpy(newCopy, desc, descSize);
    return newCopy;
}

/* Handy function to retrieve the profile count */
static inline int getProfileCount(const JitEntry *entry)
{
assert(0); /* DRP retarg func */
    if (entry->dPC == 0 || entry->codeAddress == 0)
        return 0;
    u4 *pExecutionCount = (u4 *) getTraceBase(entry);

    return *pExecutionCount;
}


/* qsort callback function */
static int sortTraceProfileCount(const void *entry1, const void *entry2)
{
assert(0); /* DRP retarg func */
    const JitEntry *jitEntry1 = entry1;
    const JitEntry *jitEntry2 = entry2;

    int count1 = getProfileCount(jitEntry1);
    int count2 = getProfileCount(jitEntry2);
    return (count1 == count2) ? 0 : ((count1 > count2) ? -1 : 1);
}

/* Sort the trace profile counts and dump them */
void dvmCompilerSortAndPrintTraceProfiles()
{
assert(0); /* DRP retarg func */
    JitEntry *sortedEntries;
    int numTraces = 0;
    unsigned long sum = 0;
    unsigned int i;

    /* Make sure that the table is not changing */
    dvmLockMutex(&gDvmJit.tableLock);

    /* Sort the entries by descending order */
    sortedEntries = malloc(sizeof(JitEntry) * gDvmJit.jitTableSize);
    if (sortedEntries == NULL)
        goto done;
    memcpy(sortedEntries, gDvmJit.pJitEntryTable,
           sizeof(JitEntry) * gDvmJit.jitTableSize);
    qsort(sortedEntries, gDvmJit.jitTableSize, sizeof(JitEntry),
          sortTraceProfileCount);

    /* Analyze the sorted entries */
    for (i=0; i < gDvmJit.jitTableSize; i++) {
        if (sortedEntries[i].dPC != 0) {
            sum += dumpTraceProfile(&sortedEntries[i],
                                       true /* silent */,
                                       false /* reset */,
                                       0);
            numTraces++;
        }
    }
    if (numTraces == 0)
        numTraces = 1;
    if (sum == 0) {
        sum = 1;
    }

    LOGD("JIT: Average execution count -> %d",(int)(sum / numTraces));

    /* Dump the sorted entries. The count of each trace will be reset to 0. */
    for (i=0; i < gDvmJit.jitTableSize; i++) {
        if (sortedEntries[i].dPC != 0) {
            dumpTraceProfile(&sortedEntries[i],
                             false /* silent */,
                             true /* reset */,
                             sum);
        }
    }

    for (i=0; i < gDvmJit.jitTableSize && i < 10; i++) {
        JitTraceDescription* desc =
            dvmCopyTraceDescriptor(NULL, &sortedEntries[i]);
        dvmCompilerWorkEnqueue(sortedEntries[i].dPC,
                               kWorkOrderTraceDebug, desc);
    }

    free(sortedEntries);
done:
    dvmUnlockMutex(&gDvmJit.tableLock);
    return;
}

#if defined(WITH_SELF_VERIFICATION)
/*
 * The following are used to keep compiled loads and stores from modifying
 * memory during self verification mode.
 *
 * Stores do not modify memory. Instead, the address and value pair are stored
 * into heapSpace. Addresses within heapSpace are unique. For accesses smaller
 * than a word, the word containing the address is loaded first before being
 * updated.
 *
 * Loads check heapSpace first and return data from there if an entry exists.
 * Otherwise, data is loaded from memory as usual.
 */

/* Used to specify sizes of memory operations */
enum {
    kSVByte,
    kSVSignedByte,
    kSVHalfword,
    kSVSignedHalfword,
    kSVWord,
    kSVDoubleword,
    kSVVariable,
};

/* Load the value of a decoded register from the stack */
static int selfVerificationMemRegLoad(int* sp, int reg)
{
assert(0); /* DRP retarg func */
    return *(sp + reg);
}

/* Load the value of a decoded doubleword register from the stack */
static s8 selfVerificationMemRegLoadDouble(int* sp, int reg)
{
assert(0); /* DRP retarg func */
    return *((s8*)(sp + reg));
}

/* Store the value of a decoded register out to the stack */
static void selfVerificationMemRegStore(int* sp, int data, int reg)
{
assert(0); /* DRP retarg func */
    *(sp + reg) = data;
}

/* Store the value of a decoded doubleword register out to the stack */
static void selfVerificationMemRegStoreDouble(int* sp, s8 data, int reg)
{
assert(0); /* DRP retarg func */
    *((s8*)(sp + reg)) = data;
}

/*
 * Load the specified size of data from the specified address, checking
 * heapSpace first if Self Verification mode wrote to it previously, and
 * falling back to actual memory otherwise.
 */
static int selfVerificationLoad(int addr, int size)
{
assert(0); /* DRP retarg func */
    Thread *self = dvmThreadSelf();
    ShadowSpace *shadowSpace = self->shadowSpace;
    ShadowHeap *heapSpacePtr;

    int data;
    int maskedAddr = addr & 0xFFFFFFFC;
    int alignment = addr & 0x3;

    for (heapSpacePtr = shadowSpace->heapSpace;
         heapSpacePtr != shadowSpace->heapSpaceTail; heapSpacePtr++) {
        if (heapSpacePtr->addr == maskedAddr) {
            addr = ((unsigned int) &(heapSpacePtr->data)) | alignment;
            break;
        }
    }

    switch (size) {
        case kSVByte:
            data = *((u1*) addr);
            break;
        case kSVSignedByte:
            data = *((s1*) addr);
            break;
        case kSVHalfword:
            data = *((u2*) addr);
            break;
        case kSVSignedHalfword:
            data = *((s2*) addr);
            break;
        case kSVWord:
            data = *((u4*) addr);
            break;
        default:
            LOGE("*** ERROR: BAD SIZE IN selfVerificationLoad: %d", size);
            data = 0;
            dvmAbort();
    }

    //LOGD("*** HEAP LOAD: Addr: 0x%x Data: 0x%x Size: %d", addr, data, size);
    return data;
}

/* Like selfVerificationLoad, but specifically for doublewords */
static s8 selfVerificationLoadDoubleword(int addr)
{
assert(0); /* DRP retarg func */
    Thread *self = dvmThreadSelf();
    ShadowSpace* shadowSpace = self->shadowSpace;
    ShadowHeap* heapSpacePtr;

    int addr2 = addr+4;
    unsigned int data = *((unsigned int*) addr);
    unsigned int data2 = *((unsigned int*) addr2);

    for (heapSpacePtr = shadowSpace->heapSpace;
         heapSpacePtr != shadowSpace->heapSpaceTail; heapSpacePtr++) {
        if (heapSpacePtr->addr == addr) {
            data = heapSpacePtr->data;
        } else if (heapSpacePtr->addr == addr2) {
            data2 = heapSpacePtr->data;
        }
    }

    //LOGD("*** HEAP LOAD DOUBLEWORD: Addr: 0x%x Data: 0x%x Data2: 0x%x",
    //    addr, data, data2);
    return (((s8) data2) << 32) | data;
}

/*
 * Handles a store of a specified size of data to a specified address.
 * This gets logged as an addr/data pair in heapSpace instead of modifying
 * memory.  Addresses in heapSpace are unique, and accesses smaller than a
 * word pull the entire word from memory first before updating.
 */
static void selfVerificationStore(int addr, int data, int size)
{
assert(0); /* DRP retarg func */
    Thread *self = dvmThreadSelf();
    ShadowSpace *shadowSpace = self->shadowSpace;
    ShadowHeap *heapSpacePtr;

    int maskedAddr = addr & 0xFFFFFFFC;
    int alignment = addr & 0x3;

    //LOGD("*** HEAP STORE: Addr: 0x%x Data: 0x%x Size: %d", addr, data, size);

    for (heapSpacePtr = shadowSpace->heapSpace;
         heapSpacePtr != shadowSpace->heapSpaceTail; heapSpacePtr++) {
        if (heapSpacePtr->addr == maskedAddr) break;
    }

    if (heapSpacePtr == shadowSpace->heapSpaceTail) {
        heapSpacePtr->addr = maskedAddr;
        heapSpacePtr->data = *((unsigned int*) maskedAddr);
        shadowSpace->heapSpaceTail++;
    }

    addr = ((unsigned int) &(heapSpacePtr->data)) | alignment;
    switch (size) {
        case kSVByte:
            *((u1*) addr) = data;
            break;
        case kSVSignedByte:
            *((s1*) addr) = data;
            break;
        case kSVHalfword:
            *((u2*) addr) = data;
            break;
        case kSVSignedHalfword:
            *((s2*) addr) = data;
            break;
        case kSVWord:
            *((u4*) addr) = data;
            break;
        default:
            LOGE("*** ERROR: BAD SIZE IN selfVerificationSave: %d", size);
            dvmAbort();
    }
}

/* Like selfVerificationStore, but specifically for doublewords */
static void selfVerificationStoreDoubleword(int addr, s8 double_data)
{
assert(0); /* DRP retarg func */
    Thread *self = dvmThreadSelf();
    ShadowSpace *shadowSpace = self->shadowSpace;
    ShadowHeap *heapSpacePtr;

    int addr2 = addr+4;
    int data = double_data;
    int data2 = double_data >> 32;
    bool store1 = false, store2 = false;

    //LOGD("*** HEAP STORE DOUBLEWORD: Addr: 0x%x Data: 0x%x, Data2: 0x%x",
    //    addr, data, data2);

    for (heapSpacePtr = shadowSpace->heapSpace;
         heapSpacePtr != shadowSpace->heapSpaceTail; heapSpacePtr++) {
        if (heapSpacePtr->addr == addr) {
            heapSpacePtr->data = data;
            store1 = true;
        } else if (heapSpacePtr->addr == addr2) {
            heapSpacePtr->data = data2;
            store2 = true;
        }
    }

    if (!store1) {
        shadowSpace->heapSpaceTail->addr = addr;
        shadowSpace->heapSpaceTail->data = data;
        shadowSpace->heapSpaceTail++;
    }
    if (!store2) {
        shadowSpace->heapSpaceTail->addr = addr2;
        shadowSpace->heapSpaceTail->data = data2;
        shadowSpace->heapSpaceTail++;
    }
}

/*
 * Decodes the memory instruction at the address specified in the link
 * register. All registers (r0-r12,lr) and fp registers (d0-d15) are stored
 * consecutively on the stack beginning at the specified stack pointer.
 * Calls the proper Self Verification handler for the memory instruction and
 * updates the link register to point past the decoded memory instruction.
 */
void dvmSelfVerificationMemOpDecode(int lr, int* sp)
{
assert(0); /* DRP retarg func */
    enum {
        kMemOpLdrPcRel = 0x09, // ldr(3)  [01001] rd[10..8] imm_8[7..0]
        kMemOpRRR      = 0x0A, // Full opcode is 7 bits
        kMemOp2Single  = 0x0A, // Used for Vstrs and Vldrs
        kMemOpRRR2     = 0x0B, // Full opcode is 7 bits
        kMemOp2Double  = 0x0B, // Used for Vstrd and Vldrd
        kMemOpStrRRI5  = 0x0C, // str(1)  [01100] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpLdrRRI5  = 0x0D, // ldr(1)  [01101] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpStrbRRI5 = 0x0E, // strb(1) [01110] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpLdrbRRI5 = 0x0F, // ldrb(1) [01111] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpStrhRRI5 = 0x10, // strh(1) [10000] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpLdrhRRI5 = 0x11, // ldrh(1) [10001] imm_5[10..6] rn[5..3] rd[2..0]
        kMemOpLdrSpRel = 0x13, // ldr(4)  [10011] rd[10..8] imm_8[7..0]
        kMemOpStmia    = 0x18, // stmia   [11000] rn[10..8] reglist [7..0]
        kMemOpLdmia    = 0x19, // ldmia   [11001] rn[10..8] reglist [7..0]
        kMemOpStrRRR   = 0x28, // str(2)  [0101000] rm[8..6] rn[5..3] rd[2..0]
        kMemOpStrhRRR  = 0x29, // strh(2) [0101001] rm[8..6] rn[5..3] rd[2..0]
        kMemOpStrbRRR  = 0x2A, // strb(2) [0101010] rm[8..6] rn[5..3] rd[2..0]
        kMemOpLdrsbRRR = 0x2B, // ldrsb   [0101011] rm[8..6] rn[5..3] rd[2..0]
        kMemOpLdrRRR   = 0x2C, // ldr(2)  [0101100] rm[8..6] rn[5..3] rd[2..0]
        kMemOpLdrhRRR  = 0x2D, // ldrh(2) [0101101] rm[8..6] rn[5..3] rd[2..0]
        kMemOpLdrbRRR  = 0x2E, // ldrb(2) [0101110] rm[8..6] rn[5..3] rd[2..0]
        kMemOpLdrshRRR = 0x2F, // ldrsh   [0101111] rm[8..6] rn[5..3] rd[2..0]
        kMemOp2Stmia   = 0xE88, // stmia  [111010001000[ rn[19..16] mask[15..0]
        kMemOp2Ldmia   = 0xE89, // ldmia  [111010001001[ rn[19..16] mask[15..0]
        kMemOp2Stmia2  = 0xE8A, // stmia  [111010001010[ rn[19..16] mask[15..0]
        kMemOp2Ldmia2  = 0xE8B, // ldmia  [111010001011[ rn[19..16] mask[15..0]
        kMemOp2Vstr    = 0xED8, // Used for Vstrs and Vstrd
        kMemOp2Vldr    = 0xED9, // Used for Vldrs and Vldrd
        kMemOp2Vstr2   = 0xEDC, // Used for Vstrs and Vstrd
        kMemOp2Vldr2   = 0xEDD, // Used for Vstrs and Vstrd
        kMemOp2StrbRRR = 0xF80, /* str rt,[rn,rm,LSL #imm] [111110000000]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2LdrbRRR = 0xF81, /* ldrb rt,[rn,rm,LSL #imm] [111110000001]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2StrhRRR = 0xF82, /* str rt,[rn,rm,LSL #imm] [111110000010]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2LdrhRRR = 0xF83, /* ldrh rt,[rn,rm,LSL #imm] [111110000011]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2StrRRR  = 0xF84, /* str rt,[rn,rm,LSL #imm] [111110000100]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2LdrRRR  = 0xF85, /* ldr rt,[rn,rm,LSL #imm] [111110000101]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2StrbRRI12 = 0xF88, /* strb rt,[rn,#imm12] [111110001000]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2LdrbRRI12 = 0xF89, /* ldrb rt,[rn,#imm12] [111110001001]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2StrhRRI12 = 0xF8A, /* strh rt,[rn,#imm12] [111110001010]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2LdrhRRI12 = 0xF8B, /* ldrh rt,[rn,#imm12] [111110001011]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2StrRRI12 = 0xF8C, /* str(Imm,T3) rd,[rn,#imm12] [111110001100]
                                       rn[19..16] rt[15..12] imm12[11..0] */
        kMemOp2LdrRRI12 = 0xF8D, /* ldr(Imm,T3) rd,[rn,#imm12] [111110001101]
                                       rn[19..16] rt[15..12] imm12[11..0] */
        kMemOp2LdrsbRRR = 0xF91, /* ldrsb rt,[rn,rm,LSL #imm] [111110010001]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2LdrshRRR = 0xF93, /* ldrsh rt,[rn,rm,LSL #imm] [111110010011]
                                rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0] */
        kMemOp2LdrsbRRI12 = 0xF99, /* ldrsb rt,[rn,#imm12] [111110011001]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2LdrshRRI12 = 0xF9B, /* ldrsh rt,[rn,#imm12] [111110011011]
                                       rt[15..12] rn[19..16] imm12[11..0] */
        kMemOp2        = 0xE000, // top 3 bits set indicates Thumb2
    };

    int addr, offset, data;
    long long double_data;
    int size = kSVWord;
    bool store = false;
    unsigned int *lr_masked = (unsigned int *) (lr & 0xFFFFFFFE);
    unsigned int insn = *lr_masked;

    int old_lr;
    old_lr = selfVerificationMemRegLoad(sp, 13);

    if ((insn & kMemOp2) == kMemOp2) {
        insn = (insn << 16) | (insn >> 16);
        //LOGD("*** THUMB2 - Addr: 0x%x Insn: 0x%x", lr, insn);

        int opcode12 = (insn >> 20) & 0xFFF;
        int opcode6 = (insn >> 6) & 0x3F;
        int opcode4 = (insn >> 8) & 0xF;
        int imm2 = (insn >> 4) & 0x3;
        int imm8 = insn & 0xFF;
        int imm12 = insn & 0xFFF;
        int rd = (insn >> 12) & 0xF;
        int rm = insn & 0xF;
        int rn = (insn >> 16) & 0xF;
        int rt = (insn >> 12) & 0xF;
        bool wBack = true;

        // Update the link register
        selfVerificationMemRegStore(sp, old_lr+4, 13);

        // Determine whether the mem op is a store or load
        switch (opcode12) {
            case kMemOp2Stmia:
            case kMemOp2Stmia2:
            case kMemOp2Vstr:
            case kMemOp2Vstr2:
            case kMemOp2StrbRRR:
            case kMemOp2StrhRRR:
            case kMemOp2StrRRR:
            case kMemOp2StrbRRI12:
            case kMemOp2StrhRRI12:
            case kMemOp2StrRRI12:
                store = true;
        }

        // Determine the size of the mem access
        switch (opcode12) {
            case kMemOp2StrbRRR:
            case kMemOp2LdrbRRR:
            case kMemOp2StrbRRI12:
            case kMemOp2LdrbRRI12:
                size = kSVByte;
                break;
            case kMemOp2LdrsbRRR:
            case kMemOp2LdrsbRRI12:
                size = kSVSignedByte;
                break;
            case kMemOp2StrhRRR:
            case kMemOp2LdrhRRR:
            case kMemOp2StrhRRI12:
            case kMemOp2LdrhRRI12:
                size = kSVHalfword;
                break;
            case kMemOp2LdrshRRR:
            case kMemOp2LdrshRRI12:
                size = kSVSignedHalfword;
                break;
            case kMemOp2Vstr:
            case kMemOp2Vstr2:
            case kMemOp2Vldr:
            case kMemOp2Vldr2:
                if (opcode4 == kMemOp2Double) size = kSVDoubleword;
                break;
            case kMemOp2Stmia:
            case kMemOp2Ldmia:
            case kMemOp2Stmia2:
            case kMemOp2Ldmia2:
                size = kSVVariable;
                break;
        }

        // Load the value of the address
        addr = selfVerificationMemRegLoad(sp, rn);

        // Figure out the offset
        switch (opcode12) {
            case kMemOp2Vstr:
            case kMemOp2Vstr2:
            case kMemOp2Vldr:
            case kMemOp2Vldr2:
                offset = imm8 << 2;
                if (opcode4 == kMemOp2Single) {
                    rt = rd << 1;
                    if (insn & 0x400000) rt |= 0x1;
                } else if (opcode4 == kMemOp2Double) {
                    if (insn & 0x400000) rt |= 0x10;
                    rt = rt << 1;
                } else {
                    LOGE("*** ERROR: UNRECOGNIZED VECTOR MEM OP: %x", opcode4);
                    dvmAbort();
                }
                rt += 14;
                break;
            case kMemOp2StrbRRR:
            case kMemOp2LdrbRRR:
            case kMemOp2StrhRRR:
            case kMemOp2LdrhRRR:
            case kMemOp2StrRRR:
            case kMemOp2LdrRRR:
            case kMemOp2LdrsbRRR:
            case kMemOp2LdrshRRR:
                offset = selfVerificationMemRegLoad(sp, rm) << imm2;
                break;
            case kMemOp2StrbRRI12:
            case kMemOp2LdrbRRI12:
            case kMemOp2StrhRRI12:
            case kMemOp2LdrhRRI12:
            case kMemOp2StrRRI12:
            case kMemOp2LdrRRI12:
            case kMemOp2LdrsbRRI12:
            case kMemOp2LdrshRRI12:
                offset = imm12;
                break;
            case kMemOp2Stmia:
            case kMemOp2Ldmia:
                wBack = false;
            case kMemOp2Stmia2:
            case kMemOp2Ldmia2:
                offset = 0;
                break;
            default:
                LOGE("*** ERROR: UNRECOGNIZED THUMB2 MEM OP: %x", opcode12);
                offset = 0;
                dvmAbort();
        }

        // Handle the decoded mem op accordingly
        if (store) {
            if (size == kSVVariable) {
                LOGD("*** THUMB2 STMIA CURRENTLY UNUSED (AND UNTESTED)");
                int i;
                int regList = insn & 0xFFFF;
                for (i = 0; i < 16; i++) {
                    if (regList & 0x1) {
                        data = selfVerificationMemRegLoad(sp, i);
                        selfVerificationStore(addr, data, kSVWord);
                        addr += 4;
                    }
                    regList = regList >> 1;
                }
                if (wBack) selfVerificationMemRegStore(sp, addr, rn);
            } else if (size == kSVDoubleword) {
                double_data = selfVerificationMemRegLoadDouble(sp, rt);
                selfVerificationStoreDoubleword(addr+offset, double_data);
            } else {
                data = selfVerificationMemRegLoad(sp, rt);
                selfVerificationStore(addr+offset, data, size);
            }
        } else {
            if (size == kSVVariable) {
                LOGD("*** THUMB2 LDMIA CURRENTLY UNUSED (AND UNTESTED)");
                int i;
                int regList = insn & 0xFFFF;
                for (i = 0; i < 16; i++) {
                    if (regList & 0x1) {
                        data = selfVerificationLoad(addr, kSVWord);
                        selfVerificationMemRegStore(sp, data, i);
                        addr += 4;
                    }
                    regList = regList >> 1;
                }
                if (wBack) selfVerificationMemRegStore(sp, addr, rn);
            } else if (size == kSVDoubleword) {
                double_data = selfVerificationLoadDoubleword(addr+offset);
                selfVerificationMemRegStoreDouble(sp, double_data, rt);
            } else {
                data = selfVerificationLoad(addr+offset, size);
                selfVerificationMemRegStore(sp, data, rt);
            }
        }
    } else {
        //LOGD("*** THUMB - Addr: 0x%x Insn: 0x%x", lr, insn);

        // Update the link register
        selfVerificationMemRegStore(sp, old_lr+2, 13);

        int opcode5 = (insn >> 11) & 0x1F;
        int opcode7 = (insn >> 9) & 0x7F;
        int imm = (insn >> 6) & 0x1F;
        int rd = (insn >> 8) & 0x7;
        int rm = (insn >> 6) & 0x7;
        int rn = (insn >> 3) & 0x7;
        int rt = insn & 0x7;

        // Determine whether the mem op is a store or load
        switch (opcode5) {
            case kMemOpRRR:
                switch (opcode7) {
                    case kMemOpStrRRR:
                    case kMemOpStrhRRR:
                    case kMemOpStrbRRR:
                        store = true;
                }
                break;
            case kMemOpStrRRI5:
            case kMemOpStrbRRI5:
            case kMemOpStrhRRI5:
            case kMemOpStmia:
                store = true;
        }

        // Determine the size of the mem access
        switch (opcode5) {
            case kMemOpRRR:
            case kMemOpRRR2:
                switch (opcode7) {
                    case kMemOpStrbRRR:
                    case kMemOpLdrbRRR:
                        size = kSVByte;
                        break;
                    case kMemOpLdrsbRRR:
                        size = kSVSignedByte;
                        break;
                    case kMemOpStrhRRR:
                    case kMemOpLdrhRRR:
                        size = kSVHalfword;
                        break;
                    case kMemOpLdrshRRR:
                        size = kSVSignedHalfword;
                        break;
                }
                break;
            case kMemOpStrbRRI5:
            case kMemOpLdrbRRI5:
                size = kSVByte;
                break;
            case kMemOpStrhRRI5:
            case kMemOpLdrhRRI5:
                size = kSVHalfword;
                break;
            case kMemOpStmia:
            case kMemOpLdmia:
                size = kSVVariable;
                break;
        }

        // Load the value of the address
        if (opcode5 == kMemOpLdrPcRel)
            addr = selfVerificationMemRegLoad(sp, 4);
        else if (opcode5 == kMemOpStmia || opcode5 == kMemOpLdmia)
            addr = selfVerificationMemRegLoad(sp, rd);
        else
            addr = selfVerificationMemRegLoad(sp, rn);

        // Figure out the offset
        switch (opcode5) {
            case kMemOpLdrPcRel:
                offset = (insn & 0xFF) << 2;
                rt = rd;
                break;
            case kMemOpRRR:
            case kMemOpRRR2:
                offset = selfVerificationMemRegLoad(sp, rm);
                break;
            case kMemOpStrRRI5:
            case kMemOpLdrRRI5:
                offset = imm << 2;
                break;
            case kMemOpStrhRRI5:
            case kMemOpLdrhRRI5:
                offset = imm << 1;
                break;
            case kMemOpStrbRRI5:
            case kMemOpLdrbRRI5:
                offset = imm;
                break;
            case kMemOpStmia:
            case kMemOpLdmia:
                offset = 0;
                break;
            default:
                LOGE("*** ERROR: UNRECOGNIZED THUMB MEM OP: %x", opcode5);
                offset = 0;
                dvmAbort();
        }

        // Handle the decoded mem op accordingly
        if (store) {
            if (size == kSVVariable) {
                int i;
                int regList = insn & 0xFF;
                for (i = 0; i < 8; i++) {
                    if (regList & 0x1) {
                        data = selfVerificationMemRegLoad(sp, i);
                        selfVerificationStore(addr, data, kSVWord);
                        addr += 4;
                    }
                    regList = regList >> 1;
                }
                selfVerificationMemRegStore(sp, addr, rd);
            } else {
                data = selfVerificationMemRegLoad(sp, rt);
                selfVerificationStore(addr+offset, data, size);
            }
        } else {
            if (size == kSVVariable) {
                bool wBack = true;
                int i;
                int regList = insn & 0xFF;
                for (i = 0; i < 8; i++) {
                    if (regList & 0x1) {
                        if (i == rd) wBack = false;
                        data = selfVerificationLoad(addr, kSVWord);
                        selfVerificationMemRegStore(sp, data, i);
                        addr += 4;
                    }
                    regList = regList >> 1;
                }
                if (wBack) selfVerificationMemRegStore(sp, addr, rd);
            } else {
                data = selfVerificationLoad(addr+offset, size);
                selfVerificationMemRegStore(sp, data, rt);
            }
        }
    }
}
#endif
