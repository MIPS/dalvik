%verify "executed"
%verify "exception handled"
    /*
     * Execute a "native inline" instruction.
     *
     * We need to call:
     *  dvmPerformInlineOp4Std(arg0, arg1, arg2, arg3, &retval, ref)
     *
     * The first four args are in a0-a3, but the last two must be pushed
     * onto the stack.
     */
    /* [opt] execute-inline vAA, {vC, vD, vE, vF}, inline#BBBB */
    FETCH(rOBJ, 1)                       # rOBJ<- BBBB
    addu    a1, rGLUE, offGlue_retval   # a1<- &glue->retval
    EXPORT_PC()                         # can throw
    GET_OPB(a0)			        # a0<- B
				        # Stack should have 16/20 available
    sw      a1, 16(sp)                  # push &glue->retval
    BAL( .L${opcode}_continue )		# make call; will return after
    lw      gp, STACK_OFFSET_GP(sp)     # restore gp
                                        # test boolean result of inline
    beqz    v0, common_exceptionThrown  # returned false, handle exception
    FETCH_ADVANCE_INST(3)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction
%break

    /*
     * Extract args, call function.
     *  a0 = #of args (0-4)
     *  rOBJ = call index
     *
     * Other ideas:
     * - Use a jump table from the main piece to jump directly into the
     *   AND/LW pairs.  Costs a data load, saves a branch.
     * - Have five separate pieces that do the loading, so we can work the
     *   interleave a little better.  Increases code size.
     */
.L${opcode}_continue:
    FETCH(t2, 2)                        # t2<- FEDC
    beq      a0, 0, 0f
    beq      a0, 1, 1f
    beq      a0, 2, 2f
    beq      a0, 3, 3f
    beq      a0, 4, 4f
    BAL(common_abort)		# too many arguments

4:  and     t0, t2, 0xf000             # isolate F
    ESRN(t1, rFP, t0, 10)
    lw      a3, 0(t1)                  # a3<- vF (shift right 12, left 2)
3:  and     t0, t2, 0x0f00             # isolate E
    ESRN(t1, rFP, t0, 6)
    lw      a2, 0(t1)                  # a2<- vE
2:  and     t0, t2, 0x00f0             # isolate D
    ESRN(t1, rFP, t0, 2)
    lw      a1, 0(t1)                  # a1<- vD
1:  and     t0, t2, 0x000f             # isolate C
    EASN(t1, rFP, t0, 2)
    lw      a0, 0(t1)                  # a0<- vC
0:
    lw     t0, .L${opcode}_table       # table of InlineOperation
    EAS4(t1, t0, rOBJ)                 # t1 <- t0 + rOBJ<<4
    lw     t9, 0(t1)
    jr     t9                          # sizeof=16, "func" is first entry
    # (not reached)

.L${opcode}_table:
    .word   gDvmInlineOpsTable

