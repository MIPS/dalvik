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
 * Traverse the instruction trace backwards from the branch looking for an 
 * instruction to move into the branch delay slot.  The attempt is currently 
 * not as aggressive as possible, and it may be possible to tune and improve 
 * a bit if performance studies suggest a worthwhile cost/benefit.
 */
static MipsLIR *delaySlotLIR(MipsLIR *firstLIR, MipsLIR *branchLIR)
{
    int isLoad;
    int loadVisited = 0;
    int isStore;
    int storeVisited = 0;
    u8 useMask = branchLIR->useMask;
    u8 defMask = branchLIR->defMask;
    MipsLIR *thisLIR;
    MipsLIR *newLIR = dvmCompilerNew(sizeof(MipsLIR), true);

    for (thisLIR = PREV_LIR(branchLIR);
         thisLIR != firstLIR;
         thisLIR = PREV_LIR(thisLIR)) {
        if (thisLIR->isNop)
            continue;

        /* give up and insert a NOP */
        if (isPseudoOpCode(thisLIR->opCode) ||
            thisLIR->opCode == kMipsNop ||
            thisLIR->opCode == kMips32BitData ||
            EncodingMap[thisLIR->opCode].flags & IS_BRANCH)
            break;

        /* don't reorder loads/stores (the alias info could
           possibly be used to allow as a future enhancement) */
        isLoad = EncodingMap[thisLIR->opCode].flags & IS_LOAD;
        isStore = EncodingMap[thisLIR->opCode].flags & IS_STORE;

        if (!(thisLIR->useMask & defMask) &&
            !(thisLIR->defMask & useMask) &&
            !(thisLIR->defMask & defMask) &&
            !(isLoad && storeVisited) &&
            !(isStore && loadVisited) &&
            !(isStore && storeVisited)) {
            *newLIR = *thisLIR;
            thisLIR->isNop = true;
            return newLIR; /* move into delay slot succeeded */
        }

        loadVisited |= isLoad;
        storeVisited |= isStore;

        /* accumulate def/use constraints */
        useMask |= thisLIR->useMask;
        defMask |= thisLIR->defMask;
    }

    newLIR->opCode = kMipsNop;
    return newLIR;
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
    MipsLIR *firstLIR =(MipsLIR *) cUnit->firstLIRInsn; 
    MipsLIR *lastLIR =(MipsLIR *) cUnit->lastLIRInsn; 
   
    for (thisLIR = lastLIR; thisLIR != firstLIR; thisLIR = PREV_LIR(thisLIR)) {
        if (thisLIR->isNop ||
            isPseudoOpCode(thisLIR->opCode) ||
            !(EncodingMap[thisLIR->opCode].flags & IS_BRANCH)) {
            continue;
        } else if (thisLIR == lastLIR) {
            dvmCompilerAppendLIR(cUnit,
                (LIR *) delaySlotLIR(firstLIR, thisLIR));
        } else if (NEXT_LIR(thisLIR)->opCode != kMipsNop) {
            dvmCompilerInsertLIRAfter((LIR *) thisLIR,
                (LIR *) delaySlotLIR(firstLIR, thisLIR));
        }
    }

    if (!thisLIR->isNop &&
        !isPseudoOpCode(thisLIR->opCode) &&
        EncodingMap[thisLIR->opCode].flags & IS_BRANCH) {
        /* nothing available to move, so insert nop */
        MipsLIR *nopLIR = dvmCompilerNew(sizeof(MipsLIR), true);
        nopLIR->opCode = kMipsNop;
        dvmCompilerInsertLIRAfter((LIR *) thisLIR, (LIR *) nopLIR);
    }
}

void dvmCompilerApplyGlobalOptimizations(CompilationUnit *cUnit)
{
    applyRedundantBranchElimination(cUnit);
    introduceBranchDelaySlot(cUnit);
}
