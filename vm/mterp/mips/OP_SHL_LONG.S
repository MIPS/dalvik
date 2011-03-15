%verify "executed"
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shl-long vAA, vBB, vCC */
    FETCH(a0, 1)                        # a0<- CCBB
    GET_OPA(t2)		                # t2<- AA
    and     a3, a0, 255                 # a3<- BB
    srl     a0, a0, 8	                # a0<- CC
    EAS2(a3, rFP, a3)                   # a3<- &fp[BB]
    GET_VREG(a2, a0)                    # a2<- vCC
    LOAD64(a0, a1, a3)			# a0/a1<- vBB/vBB+1
    and     a2, a2, 63                  # a2<- a2 & 0x3f

    EAS2(t2, rFP, t2)                   # t2<- &fp[AA]
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST

    /*
     * Same code as from SHL_LONG_2ADDR
     */
    not     v1, a2                      # v1= 31-n  (shift is 5b)
    move    v0, a0
    srl     a0, a0, 1                   # a0<- a0 >> 1
    srl     a0, a0, v1                  # a0<- a0 >> v1
    sll     v1, a1, a2                  # v1<- a1 << n
    sll     v0, v0, a2                  # v0<- a0 << n
    or      v1, v1, a0                  # v1<- v1 | a0
    andi    a2, a2, 0x20                # n & 0x20
    movn    v1, v0, a2                  # v1<- v0 (if n=32)
    movn    v0, zero, a2                # v0<- 0  (if n=32)

    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    STORE64(v0, v1, t2)			# vAA/vAA+1<- a0/a1
    b       .L${opcode}_finish
%break

.L${opcode}_finish:
    GOTO_OPCODE(t0)                     # jump to next instruction

