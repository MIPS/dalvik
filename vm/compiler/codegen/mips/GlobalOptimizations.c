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
        if (!thisLIR->isNop && thisLIR->opCode == kMipsB) {
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
 * Look back first and then ahead to try to find an instruction to move into
 * the branch delay slot.  If the analysis can be done cheaply enough, it may be
 * be possible to tune this routine to be more beneficial (e.g., being more 
 * particular about what instruction is speculated).
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

        if (isPseudoOpCode(thisLIR->opCode)) {
            if (thisLIR->opCode == kMipsPseudoDalvikByteCodeBoundary ||
                thisLIR->opCode == kMipsPseudoExtended ||
                thisLIR->opCode == kMipsPseudoSSARep)
                continue;  /* ok to move across these pseudos */
            break; /* don't move across all other pseudos */
        }

        /* give up on moving previous instruction down into slot */
        if (thisLIR->opCode == kMipsNop ||
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
            newLIR->defMask = thisLIR->defMask;
            newLIR->useMask = thisLIR->useMask;
            thisLIR->isNop = true;
            return newLIR; /* move into delay slot succeeded */
        }

        loadVisited |= isLoad;
        storeVisited |= isStore;

        /* accumulate def/use constraints */
        useMask |= thisLIR->useMask;
        defMask |= thisLIR->defMask;
    }

    /* for unconditional branches try to copy the instruction at the
       branch target up into the delay slot and adjust the branch */
    if (branchLIR->opCode == kMipsB) {
        MipsLIR *targetLIR;  
        for (targetLIR = (MipsLIR *) branchLIR->generic.target;
             targetLIR;
             targetLIR = NEXT_LIR(targetLIR)) {
            if (!targetLIR->isNop &&
                (!isPseudoOpCode(targetLIR->opCode) || /* can't pull predicted up */
                 targetLIR->opCode == kMipsPseudoChainingCellInvokePredicted))
                break; /* try to get to next real op at branch target */
        }
        if (targetLIR && !isPseudoOpCode(targetLIR->opCode) &&
            !(EncodingMap[targetLIR->opCode].flags & IS_BRANCH)) {
            *newLIR = *targetLIR;
            branchLIR->generic.target = (LIR *) NEXT_LIR(targetLIR);
            return newLIR;
        }
    } else if (branchLIR->opCode >= kMipsBeq && branchLIR->opCode <= kMipsBne) {
        /* for conditional branches try to fill branch delay slot
           via speculative execution when safe */
        MipsLIR *targetLIR;  
        for (targetLIR = (MipsLIR *) branchLIR->generic.target;
             targetLIR;
             targetLIR = NEXT_LIR(targetLIR)) {
            if (!targetLIR->isNop && !isPseudoOpCode(targetLIR->opCode))
                break; /* try to get to next real op at branch target */
        }

        MipsLIR *nextLIR;  
        for (nextLIR = NEXT_LIR(branchLIR);
             nextLIR;
             nextLIR = NEXT_LIR(nextLIR)) {
            if (!nextLIR->isNop && !isPseudoOpCode(nextLIR->opCode))
                break; /* try to get to next real op for fall thru */
        }

        if (nextLIR && targetLIR) {
            int flags = EncodingMap[nextLIR->opCode].flags;
            int isLoad = flags & IS_LOAD;

            /* common branch and fall thru to normal chaining cells case */
            if (isLoad && nextLIR->opCode == targetLIR->opCode &&
                nextLIR->operands[0] == targetLIR->operands[0]) {
                *newLIR = *targetLIR;
                newLIR->defMask = targetLIR->defMask;
                newLIR->useMask = targetLIR->useMask;
                branchLIR->generic.target = (LIR *) NEXT_LIR(targetLIR);
                return newLIR;
            }

            /* might try prefetching or speculating other safe instructions along
               the trace here (e.g., dalvik frame load is common and may be safe) */
        }
    }

    /* couldn't find a useful instruction to move into the delay slot */
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
