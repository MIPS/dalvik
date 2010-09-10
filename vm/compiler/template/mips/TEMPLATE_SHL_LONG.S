    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     */
    /* shl-long vAA(a1), vBB(a0), vCC(a2) - result in v1:v0 */
    andi    t0, a2, 0x20                #  t0<- a2 & 0x20
    movn    a1, a0, t0                  #  a1<- a0 (if (a2 & 0x20) != 0)
    movn    a0, zero, t0                #  a0<- 0 (if (a2 & 0x20) != 0)
    andi    a2, a2, 0x1f                #  a2<- a2 & 0x1f
    sll     rRESULT0, a0, a2            #  v0<- a0 << a2
    sll     rRESULT1, a1, a2            #  v1<- a1 << a2
    add     t0, zero, 32                #  t0<- 32
    sub     t0, t0, a2                  #  t0<- 32 - a2
    srl     t0, a0, t0                  #  t0<- a0 >> (32-a2)
    or      rRESULT1, rRESULT1, t0      #  v1<- v1 | (a0 >> (32-a2))
    RETURN
