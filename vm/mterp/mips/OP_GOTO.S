%verify "executed"
%verify "forward and backward"
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    sll	   a0, rINST, 16                # a0<- AAxx0000
    sra    rBIX, a0, 24                 # rBIX<- ssssssAA (sign-extended)
    sll	   rBIX, rBIX, 1                # rBIX<- byte offset
    bltz   rBIX, common_backwardBranch	# backward branch, do periodic checks
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE(a0)
    FETCH_ADVANCE_INST_RB(rBIX)         # update rPC, load rINST
    bnez   a0, common_updateProfile
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction
#else
    FETCH_ADVANCE_INST_RB(rBIX)         # update rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction
#endif
