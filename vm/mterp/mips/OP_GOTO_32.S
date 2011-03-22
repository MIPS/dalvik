%verify "executed"
%verify "forward, backward, self"
    /*
     * Unconditional branch, 32-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     *
     * Unlike most opcodes, this one is allowed to branch to itself, so
     * our "backward branch" test must be "<=0" instead of "<0".  The ORRS
     * instruction doesn't affect the V flag, so we need to clear it
     * explicitly.
     */
    /* goto/32 +AAAAAAAA */
    FETCH(a0, 1)                        # a0<- aaaa (lo)
    FETCH(a1, 2)                        # a1<- AAAA (hi)
    sll    a1, a1, 16
    or     a0, a0, a1                   # a0<- AAAAaaaa, check sign
    sll	   rBIX, a0, 1                  # rBIX<- byte offset
    blez   a0, common_backwardBranch    # backward branch, do periodic checks
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

