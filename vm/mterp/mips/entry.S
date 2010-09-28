#entry.S 

#define ASSIST_DEBUGGER 1

	.text
	.align  2
	.global	dvmMterpStdRun
	.ent	dvmMterpStdRun
dvmMterpStdRun:
	.set noreorder
	.cpload		t9
	.set reorder
/* Save to the stack. Frame size = STACK_SIZE */
	STACK_STORE_FULL()
/* This directive will make sure all subsequent jal restore gp at a known offset */
	.cprestore STACK_OFFSET_GP
	
	addu		fp, sp, STACK_SIZE	# Move Frame Pointer to the base of frame
	/* save stack pointer, add magic word for debuggerd */
	sw		sp, offGlue_bailPtr(a0)	#Save SP

	/* set up "named" registers, figure out entry point */
	move		rGLUE, a0		# set rGLUE
	lw 		a1, offGlue_entryPoint(a0)
	LOAD_PC_FROM_GLUE()
	LOAD_FP_FROM_GLUE()
	la		rIBASE, dvmAsmInstructionStart	#set rIBASE. GOT needed here.
	li		t0, kInterpEntryInstr
	bne		a1, t0, .Lnot_instr		# no, handle it
		
#if defined(WITH_JIT)
entryInstr:
	lw              t0, offGlue_self(rGLUE)
	/* Entry is always a possible trace start */
	GET_JIT_PROF_TABLE(a0)
	FETCH_INST()			# load rINST from rPC
	sw              zero, offThread_inJitCodeCache(t0)
	bnez		a0, common_updateProfile
	GET_INST_OPCODE(t0)		# extract opcode from rINST
	GOTO_OPCODE(t0)			# jump to next instruction
#else
	/* start executing the instruction at rPC */
	FETCH_INST()			# load rINST from rPC
	GET_INST_OPCODE(t0)		# extract opcode from rINST
	GOTO_OPCODE(t0)			# jump to next instruction
#endif

.Lnot_instr:		# were we returning from a method?
	beq		a1, kInterpEntryReturn, common_returnFromMethod
	
.Lnot_return:		# were we throwing an exception?
	beq             a1, kInterpEntryThrow, common_exceptionThrown

#if defined(WITH_JIT)
not_throw:
	lw		t0, offGlue_jitResumeNPC(rGLUE)
	lw		a2, offGlue_jitResumeDPC(rGLUE)
	bne		a1, kInterpEntryResume, .Lbad_arg # resuming after Jit single-step?
	bne		rPC, a2, entryInstr # must have branched, don't resume
#if defined(WITH_SELF_VERIFICATION)
        # glue->entryPoint will be set in dvmSelfVerificationSaveState
        b               jitSVShadowRunStart # reenter xlation after single-stepped insn
#endif
	li		a1, kInterpEntryInstr
	sw		a1, offGlue_entryPoint(rGLUE)
	jr		t0  # reenter the translation
.LdvmCompilerTemplate:
	.word		dvmCompilerTemplateStart
#endif

.Lbad_arg:
    	lw		a0, strBadEntryPoint
	#a1 holds value of entryPoint
	JAL(printf)
	JAL(dvmAbort)

	.end	dvmMterpStdRun	

	.global dvmMterpStdBail
	.ent	dvmMterpStdBail

/* Restore the stack pointer from rGLUE. Also restore all the registers stored at SP.
 * Return to whoever called dvmMterpStdRun.
 */
dvmMterpStdBail:
	lw		sp, offGlue_bailPtr(a0)		# Restore sp
	move		v0, a1				# return the changeInterp value
	STACK_LOAD_FULL()
	jr		ra
	
	.end dvmMterpStdBail

/*
 * String references.
 */
strBadEntryPoint:
    .word   .LstrBadEntryPoint

