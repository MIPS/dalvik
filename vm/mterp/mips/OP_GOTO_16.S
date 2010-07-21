%verify "executed"
%verify "forward and backward"
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    FETCH_S(a0, 1)                      # a0<- ssssAAAA (sign-extended)
    sll    rBIX, a0, 1                  # rBIX<- byte offset, check sign
    bltz   rBIX, common_backwardBranch  # backward branch, do periodic checks
    FETCH_ADVANCE_INST_RB(rBIX)         # update rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

