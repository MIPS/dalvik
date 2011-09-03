    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     */
    /* ushr-long vAA:vBB(rARG1:rARG0), vCC(a2) - result in (rRESULT1:rRESULT0) */
    andi    a2, 0x3f			#  shift<- shift & 0x3f
    srl     rRESULT1, rARG1, a2		#  rhi<- ahi >> shift
    srl     rRESULT0, rARG0, a2		#  rlo<- alo >> shift
    not     rARG0, a2			#  alo<- 31-n  (shift is 5b)
    sll     rARG1, 1
    sll     rARG1, rARG0		#  ahi<- ahi << (32-shift)
    or      rRESULT0, rARG1		#  rlo<- rlo | ahi
    andi    a2, 0x20			#  shift & 0x20
    movn    rRESULT0, rRESULT1, a2	#  rlo<- rhi (if n=32)
    movn    rRESULT1, zero, a2		#  rhi<- 0 (if n=32)
    RETURN
