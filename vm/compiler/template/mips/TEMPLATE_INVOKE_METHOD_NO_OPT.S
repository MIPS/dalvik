    /*
     * For polymorphic callsites - setup the Dalvik frame and load Dalvik PC
     * into rPC then jump to dvmJitToInterpNoChain to dispatch the
     * runtime-resolved callee.
     */
    # a0 = methodToCall, a1 = returnCell, rPC = dalvikCallsite
    lh     t7, offMethod_registersSize(a0)        # t7<- methodToCall->regsSize
    lh     a2, offMethod_outsSize(a0)             # a2<- methodToCall->outsSize
    lw     t9, offGlue_interpStackEnd(rGLUE)      # t9<- interpStackEnd
    lw     t8, offGlue_pSelfSuspendCount(rGLUE)   # t8<- &suspendCount
    move   a3, a1                                 # a3<- returnCell
    SAVEAREA_FROM_FP(a1, rFP)                     # a1<- stack save area
    sll    t6, t7, 2                              # multiply regsSize by 4 (4 bytes per reg)
    sub    a1, a1, t6                             # a1<- newFp(old savearea-regsSize)
    SAVEAREA_FROM_FP(t0, a1)                      # t0<- stack save area
    sll    t6, a2, 2                              # multiply outsSize by 4 (4 bytes per reg)
    sub    t0, t0, t6                             # t0<- bottom (newsave-outsSize)
    lw     t8, (t8)                               # t8<- suspendCount(int)
    bge    t0, t9, 1f                             # bottom < interpStackEnd?
    RETURN                                        # return to raise stack overflow excep.

1:
    # a1 = newFP, a0 = methodToCall, a3 = returnCell, rPC = dalvikCallsite
    lw     t9, offMethod_clazz(a0)                # t9<- methodToCall->clazz
    lw     t0, offMethod_accessFlags(a0)          # t0<- methodToCall->accessFlags
    sw     rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)
    sw     rPC, (offStackSaveArea_savedPc - sizeofStackSaveArea)(a1)
    lw     rPC, offMethod_insns(a0)               # rPC<- methodToCall->insns

    # set up newSaveArea
    sw     rFP, (offStackSaveArea_prevFrame - sizeofStackSaveArea)(a1)
    sw     a3, (offStackSaveArea_returnAddr - sizeofStackSaveArea)(a1)
    sw     a0, (offStackSaveArea_method - sizeofStackSaveArea)(a1)
    beqz   t8, 2f                                 # suspendCount != 0
    RETURN                                        # bail to the interpreter

2:
    and    t6, t0, ACC_NATIVE
    beqz   t6, 3f
#if !defined(WITH_SELF_VERIFICATION)
    j      .LinvokeNative
#else
    RETURN                                        # bail to the interpreter
#endif

3:
    # continue executing the next instruction through the interpreter
    la     t0, .LdvmJitToInterpTraceSelectNoChain # defined in footer.S
    lw     t0, (t0)
    lw     a3, offClassObject_pDvmDex(t9)         # a3<- method->clazz->pDvmDex
    lw     a2, offGlue_self(rGLUE)                # a2<- glue->self

    # Update "glue" values for the new method
    sw     a0, offGlue_method(rGLUE)              # glue->method = methodToCall
    sw     a3, offGlue_methodClassDex(rGLUE)      # glue->methodClassDex = ...
    move   rFP, a1                                # fp = newFp
    sw     rFP, offThread_curFrame(a2)            # self->curFrame = newFp


    # Start executing the callee
#if defined(WITH_JIT_TUNING)
    li     a0, kInlineCacheMiss
#endif
    jr     t0                                     # dvmJitToInterpTraceSelectNoChain
