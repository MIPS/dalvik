    /*
     * This handler transfers control to the interpeter without performing
     * any lookups.  It may be called either as part of a normal chaining
     * operation, or from the transition code in header.S.  We distinquish
     * the two cases by looking at the link register.  If called from a
     * translation chain, it will point to the chaining Dalvik PC + 1.
     * On entry:
     *    ra - if NULL:
     *        a1 - the Dalvik PC to begin interpretation.
     *    else
     *        [ra] contains Dalvik PC to begin interpretation
     *    rGLUE - pointer to interpState
     *    rFP - Dalvik frame pointer
     */
    la      t0, dvmJitToInterpPunt
    lw      a0, 0(ra)
    seq     t1, ra, zero
    movn    a0, a1, t1                 # a0 <- a1 if ra == 0
    jr      t0
    # doesn't return
