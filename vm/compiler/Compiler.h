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

#include <Thread.h>
#include <setjmp.h>

#ifndef _DALVIK_VM_COMPILER
#define _DALVIK_VM_COMPILER

/*
 * Uncomment the following to enable JIT signature breakpoint
 * #define SIGNATURE_BREAKPOINT
 */

#define MAX_JIT_RUN_LEN                 64
#define COMPILER_WORK_QUEUE_SIZE        100
#define COMPILER_IC_PATCH_QUEUE_SIZE    64

#define COMPILER_TRACED(X)
#define COMPILER_TRACEE(X)
#define COMPILER_TRACE_CHAINING(X)

typedef enum JitInstructionSetType {
    DALVIK_JIT_NONE = 0,
    DALVIK_JIT_ARM,
    DALVIK_JIT_THUMB,
    DALVIK_JIT_THUMB2,
    DALVIK_JIT_THUMB2EE,
    DALVIK_JIT_X86,
    DALVIK_JIT_MIPS,
} JitInstructionSetType;

/* Description of a compiled trace. */
typedef struct JitTranslationInfo {
    void *codeAddress;
    JitInstructionSetType instructionSet;
    bool discardResult;         // Used for debugging divergence and IC patching
    Thread *requestingThread;   // For debugging purpose
} JitTranslationInfo;

typedef enum WorkOrderKind {
    kWorkOrderInvalid = 0,      // Should never see by the backend
    kWorkOrderMethod = 1,       // Work is to compile a whole method
    kWorkOrderTrace = 2,        // Work is to compile code fragment(s)
    kWorkOrderTraceDebug = 3,   // Work is to compile/debug code fragment(s)
} WorkOrderKind;

typedef struct CompilerWorkOrder {
    const u2* pc;
    WorkOrderKind kind;
    void* info;
    JitTranslationInfo result;
    jmp_buf *bailPtr;
} CompilerWorkOrder;

/* Chain cell for predicted method invocation */
typedef struct PredictedChainingCell {
    u4 branch;                  /* Branch to chained destination */
#ifdef __mips__
    u4 delay_slot;              /* nop goes here */
#endif
    const ClassObject *clazz;   /* key #1 for prediction */
    const Method *method;       /* key #2 to lookup native PC from dalvik PC */
    u4 counter;                 /* counter to patch the chaining cell */
} PredictedChainingCell;

/* Work order for inline cache patching */
typedef struct ICPatchWorkOrder {
    PredictedChainingCell *cellAddr;    /* Address to be patched */
    PredictedChainingCell cellContent;  /* content of the new cell */
} ICPatchWorkOrder;

/* States of the dbg interpreter when serving a JIT-related request */
typedef enum JitState {
    /* Entering states in the debug interpreter */
    kJitNot = 0,               // Non-JIT related reasons */
    kJitTSelectRequest = 1,    // Request a trace (subject to filtering)
    kJitTSelectRequestHot = 2, // Request a hot trace (bypass the filter)
    kJitSelfVerification = 3,  // Self Verification Mode

    /* Operational states in the debug interpreter */
    kJitTSelect = 4,           // Actively selecting a trace
    kJitTSelectEnd = 5,        // Done with the trace - wrap it up
    kJitSingleStep = 6,        // Single step interpretation
    kJitSingleStepEnd = 7,     // Done with single step, ready return to mterp
    kJitDone = 8,              // Ready to leave the debug interpreter
} JitState;

#if defined(WITH_SELF_VERIFICATION)
typedef enum SelfVerificationState {
    kSVSIdle = 0,           // Idle
    kSVSStart = 1,          // Shadow space set up, running compiled code
    kSVSPunt = 2,           // Exiting compiled code by punting
    kSVSSingleStep = 3,     // Exiting compiled code by single stepping
    kSVSTraceSelectNoChain = 4,// Exiting compiled code by trace select no chain
    kSVSTraceSelect = 5,    // Exiting compiled code by trace select
    kSVSNormal = 6,         // Exiting compiled code normally
    kSVSNoChain = 7,        // Exiting compiled code by no chain
    kSVSBackwardBranch = 8, // Exiting compiled code with backward branch trace
    kSVSDebugInterp = 9,    // Normal state restored, running debug interpreter
} SelfVerificationState;
#endif

typedef enum JitHint {
   kJitHintNone = 0,
   kJitHintTaken = 1,         // Last inst in run was taken branch
   kJitHintNotTaken = 2,      // Last inst in run was not taken branch
   kJitHintNoBias = 3,        // Last inst in run was unbiased branch
} jitHint;

/*
 * Element of a Jit trace description.  Describes a contiguous
 * sequence of Dalvik byte codes, the last of which can be
 * associated with a hint.
 * Dalvik byte code
 */
typedef struct {
    u2    startOffset;       // Starting offset for trace run
    unsigned numInsts:8;     // Number of Byte codes in run
    unsigned runEnd:1;       // Run ends with last byte code
    jitHint  hint:7;         // Hint to apply to final code of run
} JitCodeDesc;

typedef union {
    JitCodeDesc frag;
    void*       hint;
} JitTraceRun;

/*
 * Trace description as will appear in the translation cache.  Note
 * flexible array at end, as these will be of variable size.  To
 * conserve space in the translation cache, total length of JitTraceRun
 * array must be recomputed via seqential scan if needed.
 */
typedef struct {
    const Method* method;
    JitTraceRun trace[];
} JitTraceDescription;

typedef struct CompilerMethodStats {
    const Method *method;       // Used as hash entry signature
    int dalvikSize;             // # of bytes for dalvik bytecodes
    int compiledDalvikSize;     // # of compiled dalvik bytecodes
    int nativeSize;             // # of bytes for produced native code
} CompilerMethodStats;

bool dvmCompilerSetupCodeCache(void);
bool dvmCompilerArchInit(void);
void dvmCompilerArchDump(void);
bool dvmCompilerStartup(void);
void dvmCompilerShutdown(void);
bool dvmCompilerWorkEnqueue(const u2* pc, WorkOrderKind kind, void* info);
void *dvmCheckCodeCache(void *method);
bool dvmCompileMethod(const Method *method, JitTranslationInfo *info);
bool dvmCompileTrace(JitTraceDescription *trace, int numMaxInsts,
                     JitTranslationInfo *info, jmp_buf *bailPtr);
void dvmCompilerDumpStats(void);
void dvmCompilerDrainQueue(void);
void dvmJitUnchainAll(void);
void dvmCompilerSortAndPrintTraceProfiles(void);

struct CompilationUnit;
struct BasicBlock;
struct SSARepresentation;
struct GrowableList;
struct JitEntry;

void dvmInitializeSSAConversion(struct CompilationUnit *cUnit);
int dvmConvertSSARegToDalvik(struct CompilationUnit *cUnit, int ssaReg);
void dvmCompilerLoopOpt(struct CompilationUnit *cUnit);
void dvmCompilerNonLoopAnalysis(struct CompilationUnit *cUnit);
void dvmCompilerFindLiveIn(struct CompilationUnit *cUnit,
                           struct BasicBlock *bb);
void dvmCompilerDoSSAConversion(struct CompilationUnit *cUnit,
                                struct BasicBlock *bb);
void dvmCompilerDoConstantPropagation(struct CompilationUnit *cUnit,
                                      struct BasicBlock *bb);
void dvmCompilerFindInductionVariables(struct CompilationUnit *cUnit,
                                       struct BasicBlock *bb);
char *dvmCompilerGetDalvikDisassembly(DecodedInstruction *insn);
char *dvmCompilerGetSSAString(struct CompilationUnit *cUnit,
                              struct SSARepresentation *ssaRep);
void dvmCompilerDataFlowAnalysisDispatcher(struct CompilationUnit *cUnit,
                void (*func)(struct CompilationUnit *, struct BasicBlock *));
void dvmCompilerStateRefresh(void);
JitTraceDescription *dvmCopyTraceDescriptor(const u2 *pc,
                                            const struct JitEntry *desc);
#endif /* _DALVIK_VM_COMPILER */
