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
#include "vm/compiler/CompilerInternals.h"
#include "MipsLIR.h"

/*
 * Identify unconditional branches that jump to the immediate successor of the
 * branch itself.
 */
static void applyRedundantBranchElimination(CompilationUnit *cUnit)
{
    MipsLIR *thisLIR;

    for (thisLIR = (MipsLIR *) cUnit->firstLIRInsn;
         thisLIR != (MipsLIR *) cUnit->lastLIRInsn;
         thisLIR = NEXT_LIR(thisLIR)) {

        /* Branch to the next instruction */
        if (thisLIR->opCode == kMipsB) {
            MipsLIR *nextLIR = thisLIR;

            while (true) {
                nextLIR = NEXT_LIR(nextLIR);

                /*
                 * Is the branch target the next instruction?
                 */
                if (nextLIR == (MipsLIR *) thisLIR->generic.target) {
                    thisLIR->isNop = true;
                    break;
                }

                /*
                 * Found real useful stuff between the branch and the target
                 */
                if (!isPseudoOpCode(nextLIR->opCode))
                    break;
            }
        }
    }
}

/*
 * The branch delay slot has been ignored thus far.  This is the point where
 * a useful instruction is moved into it or a nop is inserted.  Leave existing
 * NOPs alone -- these came from sparse and packed switch ops and are needed
 * to maintain the proper offset to the jump table.
 */
static void introduceBranchDelaySlot(CompilationUnit *cUnit)
{
    MipsLIR *thisLIR;

    for (thisLIR = (MipsLIR *) cUnit->firstLIRInsn;
         thisLIR != (MipsLIR *) cUnit->lastLIRInsn;
         thisLIR = NEXT_LIR(thisLIR)) {
        if (thisLIR->isNop)
            continue;
            
        if (EncodingMap[thisLIR->opCode].flags & IS_BRANCH &&
            NEXT_LIR(thisLIR)->opCode != kMipsNop) {
            MipsLIR *nopLIR = dvmCompilerNew(sizeof(MipsLIR), true);
            nopLIR->opCode = kMipsNop;
            nopLIR->defMask = 0;
            nopLIR->useMask = 0;
            dvmCompilerInsertLIRAfter((LIR *) thisLIR, (LIR *) nopLIR);
        }
    }

    if (EncodingMap[thisLIR->opCode].flags & IS_BRANCH) {
        MipsLIR *nopLIR = dvmCompilerNew(sizeof(MipsLIR), true);
        nopLIR->opCode = kMipsNop;
        nopLIR->defMask = 0;
        nopLIR->useMask = 0;
        dvmCompilerAppendLIR(cUnit, (LIR *) nopLIR);
    }
}

void dvmCompilerApplyGlobalOptimizations(CompilationUnit *cUnit)
{
    applyRedundantBranchElimination(cUnit);
    introduceBranchDelaySlot(cUnit);
}
