%verify "executed"
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    GET_OPA4(t2)			# t2<- A+
    GET_OPB(a3)			        # a3<- B
    GET_VREG(a2, a3)                    # a2<- vB
    EAS2(rOBJ, rFP, t2)   	        # rOBJ<- &fp[A]
    and     a2, a2, 63                  # a2<- a2 & 0x3f
    LOAD64(a0, a1, rOBJ)		# a0/a1<- vAA/vAA+1

    FETCH_ADVANCE_INST(1)               # advance rPC, load rINST

    not     v1, a2			# v1= 31-n  (shift is 5b)
    move    v0, a0
    srl     a0, a0, 1			# a0<- a0 >> 1
    srl     a0, a0, v1			# a0<- a0 >> v1
    sll     v1, a1, a2			# v1<- a1 << n
    sll     v0, v0, a2			# v0<- a0 << n
    or      v1, v1, a0			# v1<- v1 | a0
    andi    a2, a2, 0x20		# n & 0x20
    movn    v1, v0, a2			# v1<- v0 (if n=32) 
    movn    v0, zero, a2		# v0<- 0  (if n=32)

    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    STORE64(v0, v1, rOBJ)		# vAA/vAA+1<- a0/a1
    GOTO_OPCODE(t0)                     # jump to next instruction

