%verify "executed"
    /*
     * invoke-direct-empty is a no-op in a "standard" interpreter.
     */
    FETCH_ADVANCE_INST(3)               # advance to next instr, load rINST
    GET_INST_OPCODE(t0)                 # t0<- opcode from rINST
    GOTO_OPCODE(t0)                     # execute it
