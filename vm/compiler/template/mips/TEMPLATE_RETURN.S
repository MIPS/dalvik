    /*
     * Unwind a frame from the Dalvik stack for compiled OP_RETURN_XXX.
     * If the stored value in returnAddr
     * is non-zero, the caller is compiled by the JIT thus return to the
     * address in the code cache following the invoke instruction. Otherwise
     * return to the special dvmJitToInterpNoChain entry point.
     */
    SAVEAREA_FROM_FP(a0, rFP)           # a0<- saveArea (old)
    lw      t0, offStackSaveArea_prevFrame(a0)     # t0<- saveArea->prevFrame
    lw      t1, offGlue_pSelfSuspendCount(rGLUE)   # t1<- &suspendCount
    lw      rPC, offStackSaveArea_savedPc(a0)      # rPC<- saveArea->savedPc
#if !defined(WITH_SELF_VERIFICATION)
    lw      t2,  offStackSaveArea_returnAddr(a0)   # t2<- chaining cell ret
#else
    move    t2, zero                               # disable chaining
#endif
    lw      a2, offStackSaveArea_method - sizeofStackSaveArea(t0)
                                                   # a2<- method we're returning to
    lw      a3, offGlue_self(rGLUE)                # a3<- glue->self
#if !defined(WITH_SELF_VERIFICATION)
    beq     a2, zero, 1f                           # bail to interpreter
#else
    bne     a2, zero, 2f
    JALR(ra)                                       # punt to interpreter and compare state
    # DOUG: assume this does not return ???
2:
#endif
    la      t4, .LdvmJitToInterpNoChain            # defined in footer.S
    lw      a1, (t4)
    move    rFP, t0                                # publish new FP
    beq     a2, zero, 4f
    lw      t0, offMethod_clazz(a2)                # t0<- method->clazz
4:
    lw      t1, (t1)                               # t1<- suspendCount

    sw      a2, offGlue_method(rGLUE)              # glue->method = newSave->method
    lw      a0, offClassObject_pDvmDex(t0)         # a0<- method->clazz->pDvmDex
    sw      rFP, offThread_curFrame(a3)            # self->curFrame = fp
    add     rPC, rPC, 3*2                          # publish new rPC
    sw      a0, offGlue_methodClassDex(rGLUE)
    movn    t2, zero, t1                           # check the suspendCount and
                                                   # clear the chaining cell address
    sw      t2, offThread_inJitCodeCache(a3)       # in code cache or not
    beq     t2, zero, 3f                           # chaining cell exists?
    JALR(t2)                                       # jump to the chaining cell
    # DOUG: assume this does not return ???
3:
#if defined(JIT_STATS)
    li      a0, kCallsiteInterpreted
#endif
    j       a1                                     # callsite is interpreted
1: 
    SAVE_PC_TO_GLUE()                              # SAVE_PC_FP_TO_GLUE()
    SAVE_FP_TO_GLUE()
    la      t4, .LdvmMterpStdBail                  # defined in footer.S
    lw      a2, (t4)
    move    a1, zero                               # changeInterp = false
    move    a0, rGLUE                              # Expecting rGLUE in a0
    JALR(a2)                                       # exit the interpreter
    # DOUG: assume this does not return ???
