%verify "executed"
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* ushr-long/2addr vA, vB */
    GET_OPA4(t3)		        # t3<- A+
    GET_OPB(a3)			        # a3<- B
    GET_VREG(a2, a3)                    # a2<- vB
    EAS2(t3, rFP, t3)   	        # t3<- &fp[A]
    and     a2, 63                      # a2<- a2 & 0x3f
    LOAD64(a0, a1, t3)			# a0/a1<- vAA/vAA+1

    FETCH_ADVANCE_INST(1)               # advance rPC, load rINST
    not     v0, a2
    move    v1, a1
    sll     a1, a1, 1
    sll     a1, a1, v0
    srl     v0, a0, a2
    srl     v1, v1, a2
    or      v0, a1, v0
    andi    a2, a2, 0x20
    movn    v0, v1, a2
    movn    v1, zero, a2

    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    STORE64(v0, v1, t3)			# vAA/vAA+1<- a0/a1
    GOTO_OPCODE(t0)                     # jump to next instruction

