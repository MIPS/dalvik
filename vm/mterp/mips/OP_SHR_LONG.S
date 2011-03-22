%verify "executed"
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shr-long vAA, vBB, vCC */
    FETCH(a0, 1)                        # a0<- CCBB
    GET_OPA(t3)			        # t3<- AA
    and     a3, a0, 255                 # a3<- BB
    srl	    a0, a0, 8		        # a0<- CC
    EAS2(a3, rFP, a3)    	        # a3<- &fp[BB]
    GET_VREG(a2, a0)                    # a2<- vCC
    LOAD64(a0, a1, a3)			# a0/a1<- vBB/vBB+1
    and     a2, a2, 63                  # a2<- a2 & 0x3f
    EAS2(t3, rFP, t3)                   # t3<- &fp[AA]
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST

    not     v0, a2
    sll     a3, a1, 1
    sll     a3, a3, v0
    srl     v0, a0, a2
    sra     v1, a1, a2
    or      v0, a3, v0
    andi    a2, a2, 0x20
    sra     a1, a1, 31
    movn    v0, v1, a2
    movn    v1, a1, a2

    STORE64(v0, v1, t3)			# vAA/VAA+1 <- v0/v0
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    b       .L${opcode}_finish
%break

.L${opcode}_finish:
    GOTO_OPCODE(t0)                     # jump to next instruction

