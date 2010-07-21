%verify "branch taken"
%verify "branch not taken"
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)		            # a0<- A+
    GET_OPB(a1)			            # a1<- B
    GET_VREG(a3, a1)                        # a3<- vB
    GET_VREG(a2, a0)                        # a2<- vA
    li      rBIX, 4                         # rBIX- BYTE branch dist for not-taken
    b${revcmp} a2, a3, 1f                   # branch to 1 if comparison failed
    FETCH_S(rBIX, 1)                        # rBIX<- branch offset, in code units
    sll     rBIX, 1                         # convert to bytes, check sign
    bltz    rBIX,  common_backwardBranch    # yes, do periodic checks
1:  FETCH_ADVANCE_INST_RB(rBIX)             # update rPC, load rINST
    GET_INST_OPCODE(t0)                     # extract opcode from rINST
    GOTO_OPCODE(t0)                         # jump to next instruction

