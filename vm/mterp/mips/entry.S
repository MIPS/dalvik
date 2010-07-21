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
	lbu		a1, offGlue_entryPoint(a0)	#InterpEntry enum is char
	LOAD_PC_FROM_GLUE()
	LOAD_FP_FROM_GLUE()
	la		rIBASE, dvmAsmInstructionStart	#set rIBASE. GOT needed here.
	li		t0, kInterpEntryInstr
	bne		a1, t0, .Lnot_instr		# no, handle it
		
	/* start executing the instruction at rPC */
	FETCH_INST()			# load rINST from rPC
	GET_INST_OPCODE(t0)		# extract opcode from rINST
	GOTO_OPCODE(t0)			# jump to next instruction
.Lnot_instr:
	beq		a1, kInterpEntryReturn, common_returnFromMethod
	
.Lnot_return:
	beq             a1, kInterpEntryThrow, common_exceptionThrown
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

