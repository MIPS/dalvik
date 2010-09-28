/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .text
    .align  2

#if defined(WITH_JIT)
#if defined(WITH_SELF_VERIFICATION)

# MIPSTODO  xlate self verification from arm to mips

    .global dvmJitToInterpPunt
dvmJitToInterpPunt:
    ldr    r10, [rGLUE, #offGlue_self]  @ callee saved r10 <- glue->self
    mov    r2,#kSVSPunt                 @ r2<- interpreter entry point
    mov    r3, #0
    str    r3, [r10, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpSingleStep
dvmJitToInterpSingleStep:
    str    lr,[rGLUE,#offGlue_jitResumeNPC]
    str    r1,[rGLUE,#offGlue_jitResumeDPC]
    mov    r2,#kSVSSingleStep           @ r2<- interpreter entry point
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpTraceSelectNoChain
dvmJitToInterpTraceSelectNoChain:
    ldr    r10, [rGLUE, #offGlue_self]  @ callee saved r10 <- glue->self
    mov    r0,rPC                       @ pass our target PC
    mov    r2,#kSVSTraceSelectNoChain   @ r2<- interpreter entry point
    mov    r3, #0
    str    r3, [r10, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpTraceSelect
dvmJitToInterpTraceSelect:
    ldr    r10, [rGLUE, #offGlue_self]  @ callee saved r10 <- glue->self
    ldr    r0,[lr, #-1]                 @ pass our target PC
    mov    r2,#kSVSTraceSelect          @ r2<- interpreter entry point
    mov    r3, #0
    str    r3, [r10, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpBackwardBranch
dvmJitToInterpBackwardBranch:
    ldr    r10, [rGLUE, #offGlue_self]  @ callee saved r10 <- glue->self
    ldr    r0,[lr, #-1]                 @ pass our target PC
    mov    r2,#kSVSBackwardBranch       @ r2<- interpreter entry point
    mov    r3, #0
    str    r3, [r10, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpNormal
dvmJitToInterpNormal:
    ldr    r10, [rGLUE, #offGlue_self]  @ callee saved r10 <- glue->self
    ldr    r0,[lr, #-1]                 @ pass our target PC
    mov    r2,#kSVSNormal               @ r2<- interpreter entry point
    mov    r3, #0
    str    r3, [r10, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpNoChain
dvmJitToInterpNoChain:
    ldr    r10, [rGLUE, #offGlue_self]  @ callee saved r10 <- glue->self
    mov    r0,rPC                       @ pass our target PC
    mov    r2,#kSVSNoChain              @ r2<- interpreter entry point
    mov    r3, #0
    str    r3, [r10, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return
#else /* WITH_SELF_VERIFICATION */
/*
 * Return from the translation cache to the interpreter when the compiler is
 * having issues translating/executing a Dalvik instruction. We have to skip 
 * the code cache lookup otherwise it is possible to indefinitely bouce
 * between the interpreter and the code cache if the instruction that fails
 * to be compiled happens to be at a trace start.
 */
    .global dvmJitToInterpPunt
dvmJitToInterpPunt:
    lw    gp, STACK_OFFSET_GP(sp)
    move  rPC, a0
#ifdef JIT_STATS
    move  a0, ra
    JAL(dvmBumpPunt)
#endif 
    lw    t0, offGlue_self(rGLUE) # callee saved t0 <- glue->self
    EXPORT_PC()
    sw    zero, offThread_inJitCodeCache(t0) # Back to the interp land
    la    rIBASE, dvmAsmInstructionStart
    FETCH_INST()
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)

/*
 * Return to the interpreter to handle a single instruction.
 * On entry:
 *    a0 <= PC
 *    a1 <= PC of resume instruction
 *    ra <= resume point in translation
 */

    .global dvmJitToInterpSingleStep
dvmJitToInterpSingleStep:
    lw    gp, STACK_OFFSET_GP(sp)
    sw    ra, offGlue_jitResumeNPC(rGLUE)
    sw    a1, offGlue_jitResumeDPC(rGLUE)
    li    a1, kInterpEntryInstr
    sw    a1, offGlue_entryPoint(rGLUE)
    move  rPC, a0
    EXPORT_PC()
    la    rIBASE, dvmAsmInstructionStart
    li    a2, kJitSingleStep  # Ask for single step and then revert
    sw    a2, offGlue_jitState(rGLUE)
    li    a1, 1               # set changeInterp to bail to debug interp
    b     common_gotoBail

/*
 * Return from the translation cache and immediately request
 * a translation for the exit target.  Commonly used for callees.
 */
    .global dvmJitToInterpTraceSelectNoChain
dvmJitToInterpTraceSelectNoChain:
    lw    gp, STACK_OFFSET_GP(sp)
#ifdef JIT_STATS
    JAL(dvmBumpNoChain)
#endif
    move   a0, rPC
    JAL(dvmJitGetCodeAddr)        # Is there a translation?
    lw     t0, offGlue_self(rGLUE) # callee saved t0 <- glue->self
    move   a0, v0
    sw     a0, offThread_inJitCodeCache(t0) # set the inJitCodeCache flag
    move   a1, rPC                  # arg1 of translation may need this
    move   ra, zero                 #  in case target is HANDLER_INTERPRET
    beqz   a0, 2f
    jr     a0                       # continue native execution if so

/*
 * Return from the translation cache and immediately request
 * a translation for the exit target.  Commonly used following
 * invokes.
 */
    .global dvmJitToInterpTraceSelect
dvmJitToInterpTraceSelect:
    lw    gp, STACK_OFFSET_GP(sp)
    lw    rPC, (ra)               # get our target PC
    subu  rINST, ra, 8            # save start of chain branch
    move  a0, rPC
    JAL(dvmJitGetCodeAddr)        # Is there a translation?
    lw    t0, offGlue_self(rGLUE) # callee saved t0 <- glue->self
    sw    v0, offThread_inJitCodeCache(t0) # set the inJitCodeCache flag
    beqz  v0, 2f
    move  a0, v0
    move  a1, rINST
    JAL(dvmJitChain)              # v0<- dvmJitChain(codeAddr,chainAddr)
    move  a1, rPC                 # arg1 of translation may need this
    move  ra, zero                # in case target is HANDLER_INTERPRET
    move  a0, v0
    beqz  a0, toInterpreter       # didn't chain - resume with interpreter 
    jr    a0                      # continue native execution

/* No translation, so request one if profiling isn't disabled */
2:
    la    rIBASE, dvmAsmInstructionStart
    GET_JIT_PROF_TABLE(a0)
    FETCH_INST()
    li    t0, kJitTSelectRequestHot
    movn  a2, t0, a0              # ask for trace selection
    bnez  a0, common_selectTrace
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)

/*
 * Return from the translation cache to the interpreter.
 * The return was done with a BLX from thumb mode, and
 * the following 32-bit word contains the target rPC value.
 * Note that lr (r14) will have its low-order bit set to denote
 * its thumb-mode origin.
 *
 * We'll need to stash our lr origin away, recover the new
 * target and then check to see if there is a translation available
 * for our new target.  If so, we do a translation chain and
 * go back to native execution.  Otherwise, it's back to the
 * interpreter (after treating this entry as a potential
 * trace start).
 */
    .global dvmJitToInterpNormal
dvmJitToInterpNormal:
    lw    gp, STACK_OFFSET_GP(sp)
    lw    rPC, (ra)               # get our target PC
    subu  rINST, ra, 8            # save start of chain branch
#ifdef JIT_STATS
    JAL(dvmBumpNormal)
#endif
    move  a0, rPC
    JAL(dvmJitGetCodeAddr)        # Is there a translation?
    lw    t0, offGlue_self(rGLUE) # callee saved t0 <- glue->self
    move  a0, v0
    sw    a0, offThread_inJitCodeCache(t0) # set the inJitCodeCache flag
    beqz  a0, toInterpreter       # go if not, otherwise do chain
    move  a1, rINST
    JAL(dvmJitChain)              # v0<- dvmJitChain(codeAddr,chainAddr)
    move  a1, rPC                 # arg1 of translation may need this
    move  ra, zero                # in case target is HANDLER_INTERPRET
    move  a0, v0
    beqz  a0, toInterpreter       # didn't chain - resume with interpreter 
    jr    a0                      # continue native execution

/*
 * Return from the translation cache to the interpreter to do method invocation.
 * Check if translation exists for the callee, but don't chain to it.
 */
    .global dvmJitToInterpNoChain
dvmJitToInterpNoChain:
    lw    gp, STACK_OFFSET_GP(sp)
#ifdef JIT_STATS
    JAL(dvmBumpNoChain)
#endif
    move  a0, rPC
    JAL(dvmJitGetCodeAddr)        # Is there a translation?
    lw    t0, offGlue_self(rGLUE) # callee saved t0 <- glue->self
    move  a0, v0
    sw    a0, offThread_inJitCodeCache(t0) # set the inJitCodeCache flag
    move  a1, rPC                 # arg1 of translation may need this
    move  ra, zero                # in case target is HANDLER_INTERPRET
    beqz  a0, 1f 
    jr    a0                      # continue native execution if so
1:
#endif /* WITH_SELF_VERIFICATION */

/*
 * No translation, restore interpreter regs and start interpreting.
 * rGLUE & rFP were preserved in the translated code, and rPC has 
 * already been restored by the time we get here.  We'll need to set
 * up rIBASE & rINST, and load the address of the JitTable into r0.
 */

toInterpreter:
    EXPORT_PC()
    la    rIBASE, dvmAsmInstructionStart
    FETCH_INST()
    GET_JIT_PROF_TABLE(a0)
    # NOTE: intended fallthrough

/*
 * Common code to update potential trace start counter, and initiate
 * a trace-build if appropriate.  On entry, rPC should point to the
 * next instruction to execute, and rINST should be already loaded with
 * the next opcode word, and a0 holds a pointer to the jit profile 
 * table (pJitProfTable).
 */

common_testUpdateProfile:
    GET_INST_OPCODE(t0)
    bnez  a0, common_updateProfile
    GOTO_OPCODE(t0)
    
common_updateProfile:
    srl   a3, rPC, 12          # cheap, but fast hash function
    xor   a3, a3, rPC
    andi  a3, a3, JIT_PROF_SIZE-1 # eliminate excess bits
    addu  t1, a0, a3
    lbu   a1, (t1)             # get counter
    GET_INST_OPCODE(t0)
    subu  a1, a1, 1            # decrement counter
    sb    a1, (t1)             # and store it
    beqz  a1, 1f
    GOTO_OPCODE(t0)            # if not threshold, fallthrough otherwise
1:

/*
 * Here, we switch to the debug interpreter to request
 * trace selection.  First, though, check to see if there
 * is already a native translation in place (and, if so,
 * jump to it now).
 */
    GET_JIT_THRESHOLD(a1)
    sb    a1, (t1)
    EXPORT_PC()
    move  a0, rPC
    JAL(dvmJitGetCodeAddr)       # v0<- dvmJitGetCodeAddr(rPC)
    lw    t0, offGlue_self(rGLUE) # callee saved t0 <- glue->self
    move  a0, v0 
    sw    v0, offThread_inJitCodeCache(t0) # set the inJitCodeCache flag
    move  a1, rPC                 # arg1 of translation may need this
    move  ra, zero                # in case target is HANDLER_INTERPRET

#if !defined(WITH_SELF_VERIFICATION)
    li    t0, kJitTSelectRequest # ask for trace selection 
    movz  a2, t0, a0
    beqz  a0, common_selectTrace
    jr    a0                     # jump to the translation
#else
# MIPSTODO xlate self verification from arm to mips
    moveq   r2,#kJitTSelectRequest      @ ask for trace selection
    beq     common_selectTrace
    /*
     * At this point, we have a target translation.  However, if
     * that translation is actually the interpret-only pseudo-translation
     * we want to treat it the same as no translation.
     */
    mov     r10, r0                     @ save target
    bl      dvmCompilerGetInterpretTemplate
    cmp     r0, r10                     @ special case?
    bne     jitSVShadowRunStart         @ set up self verification shadow space
    GET_INST_OPCODE(ip)
    GOTO_OPCODE(ip)
    /* no return */
#endif

/*
 * On entry:
 *  a2 is jit state, e.g. kJitTSelectRequest or kJitTSelectRequestHot
 */

common_selectTrace:
    sw    a2, offGlue_jitState(rGLUE)
    li    a2, kInterpEntryInstr  # normal entry reason
    sw    a2, offGlue_entryPoint(rGLUE)
    li    a1, 1                  # set changeInterp
    b     common_gotoBail

#if defined(WITH_SELF_VERIFICATION)
# MIPSTODO xlate self verification from arm to mips
/*
 * Save PC and registers to shadow memory for self verification mode
 * before jumping to native translation.
 * On entry:
 *    rPC, rFP, rGLUE: the values that they should contain
 *    r10: the address of the target translation.
 */
jitSVShadowRunStart:
    mov     r0,rPC                      @ r0<- program counter
    mov     r1,rFP                      @ r1<- frame pointer
    mov     r2,rGLUE                    @ r2<- InterpState pointer
    mov     r3,r10                      @ r3<- target translation
    bl      dvmSelfVerificationSaveState @ save registers to shadow space
    ldr     rFP,[r0,#offShadowSpace_shadowFP] @ rFP<- fp in shadow space
    add     rGLUE,r0,#offShadowSpace_interpState @ rGLUE<- rGLUE in shadow space
    bx      r10                         @ jump to the translation

/*
 * Restore PC, registers, and interpState to original values
 * before jumping back to the interpreter.
 */
jitSVShadowRunEnd:
    mov    r1,rFP                        @ pass ending fp
    bl     dvmSelfVerificationRestoreState @ restore pc and fp values
    ldr    rPC,[r0,#offShadowSpace_startPC] @ restore PC
    ldr    rFP,[r0,#offShadowSpace_fp]   @ restore FP
    ldr    rGLUE,[r0,#offShadowSpace_glue] @ restore InterpState
    ldr    r1,[r0,#offShadowSpace_svState] @ get self verification state
    cmp    r1,#0                         @ check for punt condition
    beq    1f
    mov    r2,#kJitSelfVerification      @ ask for self verification
    str    r2,[rGLUE,#offGlue_jitState]
    mov    r2,#kInterpEntryInstr         @ normal entry reason
    str    r2,[rGLUE,#offGlue_entryPoint]
    mov    r1,#1                         @ set changeInterp
    b      common_gotoBail

1:                                       @ exit to interpreter without check
    EXPORT_PC()
    adrl   rIBASE, dvmAsmInstructionStart
    FETCH_INST()
    GET_INST_OPCODE(ip)
    GOTO_OPCODE(ip)
#endif

#endif /* WITH_JIT */

/*
 * Common code when a backward branch is taken.
 *
 * On entry:
 *  rBIX is PC adjustment *in bytes*
 */

common_backwardBranch:
    li     a0, kInterpEntryInstr 
    JAL(common_periodicChecks)
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE(a0)
    FETCH_ADVANCE_INST_RB(rBIX)         # update rPC, load rINST
    bnez    a0, common_updateProfile
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction
#else
    FETCH_ADVANCE_INST_RB(rBIX)         # update rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction
#endif


/*
 * Need to see if the thread needs to be suspended or debugger/profiler
 * activity has begun.
 *
 * On entry:
 *  a0 is reentry type, e.g. kInterpEntryInstr
 *  rBIX is trampoline PC adjustment *in bytes*
 */
	.ent common_periodicChecks
common_periodicChecks:
    lw    a3, offGlue_pSelfSuspendCount(rGLUE) # a3<- &suspendCount
    # speculatively store a0 before it is clobbered by dvmCheckSuspendPending
    sw    a0, offGlue_entryPoint(rGLUE)

#if defined(WITH_DEBUGGER)
    lw    a1,  offGlue_pDebuggerActive(rGLUE)   # a1<- &debuggerActive
#endif
#if defined(WITH_PROFILER)
    lw    a2, offGlue_pActiveProfilers(rGLUE)  # a2<- &activeProfilers
#endif

    lw    a3, (a3)                   # a3<- suspendCount (int)

#if defined(WITH_DEBUGGER)
    lbu   a1, (a1)                   # a1<- debuggerActive (boolean)
#endif
#if defined (WITH_PROFILER)
    lw    a2, (a2)                    # a2<- activeProfilers (int)
#endif

    bnez    a3, 2f                  # yes, check suspend

# if defined(WITH_DEBUGGER) && defined(WITH_PROFILER)
    or      a1, a1, a2                  # a1<- a1 | a2
    bnez    a1, 3f                   # debugger attached or profiler started?
# elif defined(WITH_DEBUGGER)
    bnez    a1, 3f                   # debugger attached?
# elif defined(WITH_PROFILER)
    bnez    a2, 3f                   # profiler started?
# endif

    jr      ra                          # nothing to do, return

3:  # debugger/profiler enabled, bail out
    addu	rPC, rPC, rBIX                # update rPC
    li		a1, 1                       # "want switch" = true
    b       common_gotoBail

2:  # check suspend
#if defined(WITH_JIT)
    /*
     * Refresh the Jit's cached copy of profile table pointer.  This pointer
     * doubles as the Jit's on/off switch.
     */
    lw		a3, offGlue_ppJitProfTable(rGLUE) # a3<-&gDvmJit.pJitProfTable
    lw		a0, offGlue_self(rGLUE)    # a0 <- glue->self
    lw          a3, (a3)                  # a3 <- pJitProfTable
    EXPORT_PC()
    sw          a3, offGlue_pJitProfTable(rGLUE) # refresh Jit's on/off switch
#else
    lw		a0, offGlue_self(rGLUE)    # a0 <- glue->self
    EXPORT_PC() # need for precise GC
#endif
    la		t9, dvmCheckSuspendPending
    jr		t9	# suspend if necessary, then return

    .end	common_periodicChecks

/*
 * The equivalent of "goto bail", this calls through the "bail handler".
 *
 * State registers will be saved to the "glue" area before bailing.
 *
 * On entry:
 *  a1 is "bool changeInterp", indicating if we want to switch to the
 *     other interpreter or just bail all the way out
 */
	.ent	common_gotoBail
common_gotoBail:
	SAVE_PC_TO_GLUE()                # export state to "glue"
	SAVE_FP_TO_GLUE()
	move	a0, rGLUE                   # a0<- glue ptr
    	b       dvmMterpStdBail             # call(glue, changeInterp)
	.end	common_gotoBail
/*
 * Common code for method invocation with range.
 *
 * On entry:
 *  a0 is "Method* methodToCall", the method we're trying to call
 */
common_invokeMethodRange:
.LinvokeNewRange:
    # prepare to copy args to "outs" area of current frame
	GET_OPA(a2)
	SAVEAREA_FROM_FP(t1, rFP)		# t1<- stack save area 	
	beqz	a2, .LinvokeArgsDone
    	FETCH(a1, 2)                        # a1<- CCCC

    # a0=methodToCall, a1=CCCC, a2=count, t1=outs
    # (very few methods have > 10 args; could unroll for common cases)
	EAS2(a3, rFP, a1)
        sll     t0, a2, 2
        subu    t1, t1, t0

1:	lw	a1, 0(a3)
        addu	a3, a3, 4
	subu	a2, a2, 1								
	sw	a1, 0(t1)
        addu    t1, 4
	bnez	a2, 1b
	b		.LinvokeArgsDone

/*
 * Common code for method invocation without range.
 *
 * On entry:
 *  a0 is "Method* methodToCall", the method we're trying to call
 */
common_invokeMethodNoRange:
.LinvokeNewNoRange:
	# prepare to copy args to "outs" area of current frame
	GET_OPB(a2)
	SAVEAREA_FROM_FP(t1, rFP)		
	blez	a2, .LinvokeArgsDone
	FETCH(a1, 2)

    # a0=methodToCall, a1=GFED, a2=count, t10=outs
.LinvokeNonRange:
	beq	        a2, 0, 0f
	beq		a2, 1, 1f
	beq		a2, 2, 2f
	beq		a2, 3, 3f
	beq		a2, 4, 4f
	beq		a2, 5, 5f

5:	and		t0, rINST, 0x0f00
	ESRN(t2, rFP, t0, 6)
	lw		a3, (t2)
	subu		t1, 4
	sw		a3, 0(t1)

4:	and		t0, a1, 0xf000
	ESRN(t2, rFP, t0, 10)
	lw		a3, (t2)
	subu		t1, 4
	sw		a3, 0(t1)

3:	and     t0, a1, 0x0f00	
	ESRN(t2, rFP, t0, 6)
	lw		a3, (t2)
	subu		t1, 4
	sw		a3, 0(t1)

2:	and     t0, a1, 0x00f0
	ESRN(t2, rFP, t0, 2)
	lw		a3, (t2)
	subu		t1, 4
	sw		a3, 0(t1)
	
1:	and		t0, a1, 0x000f
	EASN(t2, rFP, t0, 2)
	lw		a3, (t2)
	subu		t1, 4
	sw		a3, 0(t1)

0:	#fall through .LinvokeArgsDone


.LinvokeArgsDone: # a0=methodToCall
	# find space for the new stack frame, check for overflow 
	SAVEAREA_FROM_FP(a1, rFP)		#a1<- stack save area
	lhu	a2, offMethod_registersSize(a0) # a2<- methodToCall->regsSize
	lhu	a3, offMethod_outsSize(a0)		# a3<- methodToCall->outsSize
	sll	t0, a2, 2				# a1<- newFp (old savearea - regsSize)
	subu	a1, a1, t0
	SAVEAREA_FROM_FP(t1, a1)
	lw	t3, offGlue_interpStackEnd(rGLUE)	# t3<- interpStackEnd
	sll	t2, a3, 2
	subu	a3, t1, t2
	bltu	a3, t3, .LstackOverflow		# yes, this frame will overflow stack 

		
    # set up newSaveArea
#ifdef EASY_GDB
	SAVEAREA_FROM_FP(t0, rFP)
	sw		t0, offStackSaveArea_prevSave(t1)
#endif
	sw		rFP, (offStackSaveArea_prevFrame)(t1)
	sw		rPC, (offStackSaveArea_savedPc)(t1)
#if defined(WITH_JIT)
	sw		zero, (offStackSaveArea_returnAddr)(t1)
#endif
	sw		a0, (offStackSaveArea_method)(t1)
	
	lw		a3, offMethod_accessFlags(a0) # a3<- methodToCall->accessFlags  
	and		t2, a3, ACC_NATIVE
	bnez		t2, .LinvokeNative	
    # Update "glue" values for the new method
    # a0=methodToCall, a1=newFp
	lw		a3,	offMethod_clazz(a0)
 	sw		a0, offGlue_method(rGLUE)
	lw		a3, offClassObject_pDvmDex(a3)
	lw		rPC, offMethod_insns(a0)
	sw		a3, offGlue_methodClassDex(rGLUE)
	lw		a2, offGlue_self(rGLUE)
#if defined(WITH_JIT)
	GET_JIT_PROF_TABLE(a0) 
	FETCH_INST()
	move     rFP, a1
	GET_INST_OPCODE(t0)
	sw		a1, offThread_curFrame(a2)
	bnez		a0, common_updateProfile
	GOTO_OPCODE(t0)
#else
	FETCH_INST()
	move     rFP, a1
	GET_INST_OPCODE(t0)
	sw		a1, offThread_curFrame(a2)
	GOTO_OPCODE(t0)
#endif

.LinvokeNative:
    # Prep for the native call
    # a0=methodToCall, a1=newFp, t1=newSaveArea
	lw	a3, offGlue_self(rGLUE)
	lw	t3, offThread_jniLocal_topCookie(a3) # t3 <- thread->localRef->...
	sw	a1, offThread_curFrame(a3)
	sw	t3, offStackSaveArea_localRefCookie(t1) #newFp->localRefCookie=top
        move    rOBJ, t1
	move	rBIX, a3
	move	a2, a0
	move	a0, a1
	addu	a1, rGLUE, offGlue_retval 	

#ifdef ASSIST_DEBUGGER
	/* insert fake function header to help gdb find the stack frame */
	b       .Lskip
	.ent	dalvik_mterp
dalvik_mterp:
	STACK_STORE_FULL()
.Lskip:
#endif
	lw	t9, offMethod_nativeFunc(a2)
	jalr	t9
	lw      gp, STACK_OFFSET_GP(sp)

#if defined(WITH_JIT)
	lw      a3, offGlue_ppJitProfTable(rGLUE) # Refresh Jit's on/off status
#endif

        # native return; rBIX=self, rOBJ=newSaveArea
        # equivalent to dvmPopJniLocals
	lw	a0, offStackSaveArea_localRefCookie(rOBJ)
	lw	a1, offThread_exception(rBIX)
#if defined(WITH_JIT)
	lw      a3, (a3)   # a3 <- gDvmJit.pProfTable
#endif
	sw	rFP, offThread_curFrame	(rBIX)
	sw	a0, offThread_jniLocal_topCookie(rBIX)
#if defined(WITH_JIT)
	sw      a3, offGlue_pJitProfTable(rGLUE) # refresh cached on/off switch
#endif
	bnez	a1, common_exceptionThrown
	
	FETCH_ADVANCE_INST(3)
	GET_INST_OPCODE(t0)
	GOTO_OPCODE(t0)

.LstackOverflow: # a0=methodToCall
	move    a1, a0     # a1 <- methodToCall
	lw	a0, offGlue_self(rGLUE)
	JAL(dvmHandleStackOverflow)
	b	common_exceptionThrown
#ifdef ASSIST_DEBUGGER
	.end	dalvik_mterp
#endif


/*
 * Common code for handling a return instruction.
 *
 * This does not return.
 */
common_returnFromMethod:
.LreturnNew:
	li	a0, kInterpEntryReturn
	li	rBIX, 0
	JAL(common_periodicChecks)

	SAVEAREA_FROM_FP(a0, rFP)
	lw	rFP, offStackSaveArea_prevFrame(a0)
	lw	a2, (offStackSaveArea_method - sizeofStackSaveArea)(rFP)
	li	a1, 0
	beqz	a2, common_gotoBail
	
	lw	rPC, offStackSaveArea_savedPc(a0)
	lw	a3, offGlue_self(rGLUE)
	sw	a2, offGlue_method(rGLUE)
	sw	rFP, offThread_curFrame(a3)
	lw	a1, offMethod_clazz(a2) 
	FETCH_ADVANCE_INST(3)
	lw	a1, offClassObject_pDvmDex(a1)
#if defined(WITH_JIT)
	lw	t0, offStackSaveArea_returnAddr(a0) # t0 = saveArea->returnAddr
	GET_JIT_PROF_TABLE(a0)
	sw	a1, offGlue_methodClassDex(rGLUE)
	sw      t0, offThread_inJitCodeCache(a3) # may return to JIT'ed land
	beqz	t0, no_compiled_code_caller
	jalr	t0  # caller is compiled code
	move	a0, v0
no_compiled_code_caller:
	GET_INST_OPCODE(t0)
	bnez	a0, common_updateProfile
	GOTO_OPCODE(t0)
#else
	GET_INST_OPCODE(t0)
	sw	a1, offGlue_methodClassDex(rGLUE)
	GOTO_OPCODE(t0)
#endif

/*
 * Somebody has thrown an exception.  Handle it.
 *
 * If the exception processing code returns to us (instead of falling
 * out of the interpreter), continue with whatever the next instruction
 * now happens to be.
 *
 * This does not return.
 */
     .global dvmMterpCommonExceptionThrown
dvmMterpCommonExceptionThrown:
common_exceptionThrown:
.LexceptionNew:
	li	a0, kInterpEntryThrow
	li	rBIX, 0
	JAL(common_periodicChecks)
	
	lw	rSELF, offGlue_self(rGLUE)
	lw	rBIX, offThread_exception(rSELF)
	move	a1, rSELF
	move	a0, rBIX
	JAL(dvmAddTrackedAlloc)
	sw	zero, offThread_exception(rSELF)		

	/* set up args and a local for "&fp" */
	sw	rFP, 20(sp)          		   # store   rFP => tmp
	addu    t0, sp, 20                         # compute &tmp
	sw	t0, STACK_OFFSET_ARG04(sp)         # save it in arg4 as per ABI

	li	a3, 0
	lw	a1, offGlue_method(rGLUE)
	move	a0, rSELF
	lw	a1, offMethod_insns(a1)
	move	a2, rBIX
	subu	a1, rPC, a1
	sra	a1, a1, 1
        /*
         * call 
         *     int dvmFindCatchBlock(Thread* self, int relPc, Object* exception,
         *                           bool scanOnly, void** newFrame)
         */
	JAL(dvmFindCatchBlock)
	lw	rFP, 20(sp)    		# retrieve the updated pc

	move    a0, v0
	bltz	v0, .LnotCaughtLocally

	/* fix stack overflow if necessary; must preserve a0 */
	lbu	a1, offThread_stackOverflowed(rSELF)
	beqz	a1, 1f
	move    a1, rBIX
	move 	rBIX, a0
	move	a0, rSELF
	JAL(dvmCleanupStackOverflow)
	move	a0, rBIX
	lw	rBIX, offThread_exception(rSELF)	

1:
	/* adjust locals to match self->curFrame and updated PC */
	SAVEAREA_FROM_FP(a1, rFP)
	lw	a1, offStackSaveArea_method(a1)
	sw	a1, offGlue_method(rGLUE)
	lw	a2, offMethod_clazz(a1)
	lw	a3, offMethod_insns(a1)
	lw	a2, offClassObject_pDvmDex(a2)
	#sll	t2, a0,	1
	#addu	rPC, a3, t2
        EAS1(rPC, a3, a0)
	sw	a2, offGlue_methodClassDex(rGLUE)

	/* release the tracked alloc on the exception */	
	move	a0, rBIX
	move	a1, rSELF
	JAL(dvmReleaseTrackedAlloc)

	/* restore the exception if the handler wants it */
	FETCH_INST()
	GET_INST_OPCODE(t0)
	bne		t0, OP_MOVE_EXCEPTION, 2f
 	sw		rBIX, offThread_exception(rSELF)
2:
	GOTO_OPCODE(t0)

.LnotCaughtLocally:	 # rBIX = exception, rSELF = self
	/* fix stack overflow if necessary */
	lbu     a1, offThread_stackOverflowed(rSELF)
	beqz	a1, 3f
	move	a0, rSELF
	move	a1, rBIX
	JAL(dvmCleanupStackOverflow)

3:	
	# may want to show "not caught locally" debug messages here
#if DVM_SHOW_EXCEPTION >= 2
	/* call __android_log_print(prio, tag, format, ...) */
	/* "Exception %s from %s:%d not caught locally" */
	lw	a0, offGlue_method(rGLUE)
	lw	a1, offMethod_insns(a0)
	subu	a1, rPC, a1
	sra	a1, a1, 1
	JAL(dvmLineNumFromPC)
	sw	v0, 20(sp)
	# dvmGetMethodSourceFile(method)
	lw	a0, offGlue_method(rGLUE)
	JAL(dvmGetMethodSourceFile)
	sw	v0, 16(sp)
	# exception->clazz->descriptor
	lw	a3, offObject_clazz(rBIX)
	lw	a3, offClassObject_descriptor(a3)
	la	a2, .LstrExceptionNotCaughtLocally
	la	a1, .LstrLogTag  
	li	a0, 3
	JAL(__android_log_print)
#endif
	sw	rBIX, offThread_exception(rSELF)
	move	a0, rBIX
	move	a1, rSELF
	JAL(dvmReleaseTrackedAlloc)
	li	a1, 0
        STACK_LOAD_RA()
	b	common_gotoBail	
/*
 * After returning from a "glued" function, pull out the updated
 * values and start executing at the next instruction.
 */
common_resumeAfterGlueCall:
	LOAD_PC_FROM_GLUE()		# pull rPC and rFP out of glue
	LOAD_FP_FROM_GLUE()
	FETCH_INST()			# load rINST from rPC
	GET_INST_OPCODE(t0)		# extract opcode from rINST
	GOTO_OPCODE(t0)			# jump to next instruction

/*
 * Invalid array index.
 */
common_errArrayIndex:
    EXPORT_PC()
    la      a0, .LstrArrayIndexException
    li	    a1, 0
    JAL(dvmThrowException)
    b       common_exceptionThrown

/*
 * Invalid array value.
 */
common_errArrayStore:
    EXPORT_PC()
    la      a0, .LstrArrayStoreException
    li	    a1, 0
    JAL(dvmThrowException)
    b       common_exceptionThrown

/*
 * Integer divide or mod by zero.
 */
common_errDivideByZero:
    EXPORT_PC()
    la      a0, .LstrArithmeticException
    la      a1, .LstrDivideByZero
    JAL(dvmThrowException)
    b       common_exceptionThrown

/*
 * Attempt to allocate an array with a negative size.
 */
common_errNegativeArraySize:
    EXPORT_PC()
    la      a0, .LstrNegativeArraySizeException
    li		a1, 0
    JAL(dvmThrowException)
    b       common_exceptionThrown

/*
 * Invocation of a non-existent method.
 */
common_errNoSuchMethod:
    EXPORT_PC()
    la		a0, .LstrNoSuchMethodError
    li		a1, 0
    JAL(dvmThrowException)
    b		common_exceptionThrown

/*
 * We encountered a null object when we weren't expecting one.  We
 * export the PC, throw a NullPointerException, and goto the exception
 * processing code.
 */
common_errNullObject:
    EXPORT_PC()
    la      a0, .LstrNullPointerException
    li	    a1, 0
    JAL(dvmThrowException)
    b       common_exceptionThrown

/*
 * For debugging, cause an immediate fault. 
 */
common_abort:
	b		.LdeadFood
.LdeadFood:
    .word   0xffffffff

/*
 * Spit out a "we were here", preserving all registers.  
 */
    .macro  SQUEAK num
common_squeak\num:
        STACK_STORE_RA();
	la	a0, .LstrSqueak
	LOAD_IMM(a1, \num);
	JAL(printf);
        STACK_LOAD_RA();
	RETURN;
    .endm

    SQUEAK  0
    SQUEAK  1
    SQUEAK  2
    SQUEAK  3
    SQUEAK  4
    SQUEAK  5

/*
 * Spit out the number in a0, preserving registers.
 */
common_printNum:
    STACK_STORE_RA()
    MOVE_REG(a1, a0)
    la  a0, .LstrSqueak
    JAL(printf)
    STACK_LOAD_RA()
    RETURN

/*
 * Print a newline, preserving registers.
 */
common_printNewline:
    STACK_STORE_RA()
    la a0, .LstrNewline
    JAL(printf)
    STACK_LOAD_RA()
    RETURN

    /*
     * Print the 32-bit quantity in a0 as a hex value, preserving registers.
     */
common_printHex:
    STACK_STORE_RA()
    MOVE_REG(a1,a0)
    la a0, .LstrPrintHex
    JAL(printf)
    STACK_LOAD_RA()
RETURN;

/*
 * Print the 64-bit quantity in a0-a1, preserving registers.
 */
common_printLong:
    STACK_STORE_RA()
    MOVE_REG(a3,a1)
    MOVE_REG(a2,a0)
    la a0, .LstrPrintLong
    JAL(printf)
    STACK_LOAD_RA()
    RETURN;

/*
 * Print full method info.  Pass the Method* in a0.  Preserves regs.
 */
common_printMethod:
    STACK_STORE_RA()
    JAL(dvmMterpPrintMethod)
    STACK_LOAD_RA()
    RETURN

/*
 * Call a C helper function that dumps regs and possibly some
 * additional info.  Requires the C function to be compiled in.
 */
    .if     0
common_dumpRegs:
    STACK_STORE_RA()
    JAL(dvmMterpDumpMipsRegs)
    STACK_LOAD_RA()
    RETURN
    .endif

/*
 * Zero-terminated ASCII string data.
 */
    .data

.LstrBadEntryPoint:
    .asciiz  "Bad entry point %d\n"
.LstrArithmeticException:
    .asciiz  "Ljava/lang/ArithmeticException;"
.LstrArrayIndexException:
    .asciiz  "Ljava/lang/ArrayIndexOutOfBoundsException;"
.LstrArrayStoreException:
    .asciiz  "Ljava/lang/ArrayStoreException;"
.LstrClassCastException:
    .asciiz  "Ljava/lang/ClassCastException;"
.LstrDivideByZero:
    .asciiz  "divide by zero"
.LstrFilledNewArrayNotImpl:
    .asciiz  "filled-new-array only implemented for 'int'"
.LstrInternalError:
    .asciiz  "Ljava/lang/InternalError;"
.LstrInstantiationError:
    .asciiz  "Ljava/lang/InstantiationError;"
.LstrNegativeArraySizeException:
    .asciiz  "Ljava/lang/NegativeArraySizeException;"
.LstrNoSuchMethodError:
    .asciiz  "Ljava/lang/NoSuchMethodError;"
.LstrNullPointerException:
    .asciiz  "Ljava/lang/NullPointerException;"

.LstrLogTag:
    .asciiz  "mterp"
.LstrExceptionNotCaughtLocally:
    .asciiz  "Exception %s from %s:%d not caught locally\n"

.LstrNewline:
    .asciiz  "\n"
.LstrSqueak:
    .asciiz  "<%d>"
.LstrPrintHex:
    .asciiz  "<0x%x>"
.LstrPrintLong:
    .asciiz  "<%lld>"
