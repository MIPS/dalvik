    # a0 = methodToCall, a1 = returnCell, rPC = dalvikCallsite
    lh     t7, offMethod_registersSize(a0)        # t7<- methodToCall->regsSize
    lw     t9, offGlue_interpStackEnd(rGLUE)      # t9<- interpStackEnd
    lw     t8, offGlue_pSelfSuspendCount(rGLUE)   # t8<- &suspendCount
    move   a3, a1                                 # a3<- returnCell
    SAVEAREA_FROM_FP(a1, rFP)                     # a1<- stack save area
    sll    t6, t7, 2                              # multiply regsSize by 4 (4 bytes per reg)
    sub    a1, a1, t6                             # a1<- newFp(old savearea-regsSize)
    SAVEAREA_FROM_FP(t0, a1)                      # t0<- stack save area
    lw     t8, (t8)                               # t8<- suspendCount(int)
    bge    t0, t9, 1f                             # bottom < interpStackEnd?
    RETURN                                        # return to raise stack overflow excep.

1:
    # a1 = newFP, a0 = methodToCall, a3 = returnCell, rPC = dalvikCallsite
    sw     rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)
    sw     rPC, (offStackSaveArea_savedPc - sizeofStackSaveArea)(a1)
    lw     rPC, offMethod_insns(a0)               # rPC<- methodToCall->insns

    # set up newSaveArea
    sw     rFP, (offStackSaveArea_prevFrame - sizeofStackSaveArea)(a1)
    sw     a3, (offStackSaveArea_returnAddr - sizeofStackSaveArea)(a1)
    lw     a3, offGlue_self(rGLUE)                # a3<- glue->self
    sw     a0, (offStackSaveArea_method - sizeofStackSaveArea)(a1)
    lw     t9, offMethod_nativeFunc(a0)           # t9<- method->nativeFunc
#if !defined(WITH_SELF_VERIFICATION)
    beqz   t8, 2f                                 # suspendCount != 0
    RETURN                                        # bail to the interpreter
2:
#else
    RETURN                                        # bail to the interpreter unconditionally
#endif

    # go ahead and transfer control to the native code
    lw     t6, offThread_jniLocal_topCookie(a3)   # t6<- thread->localRef->...
    sw     a1, offThread_curFrame(a3)             # self->curFrame = newFp
    sw     zero, offThread_inJitCodeCache(a3)     # not in the jit code cache
    sw     t6, (offStackSaveArea_localRefCookie - sizeofStackSaveArea)(a1)
                                                  # newFp->localRefCookie=top
    move   rOBJ, a3                               # rOBJ<- glue->self (preserve)
    SAVEAREA_FROM_FP(rBIX, a1)                    # rBIX<- new stack save area
    move   a2, a0                                 # a2<- methodToCall
    move   a0, a1                                 # a0<- newFp
    add    a1, rGLUE, offGlue_retval              # a1<- &retval
    JALR(t9)                                      # off to the native code
    lw     gp, STACK_OFFSET_GP(sp)

    # native return; rOBJ=self, rBIX=newSaveArea
    # equivalent to dvmPopJniLocals
    lw     a2, offStackSaveArea_returnAddr(rBIX)     # a2 = chaining cell ret addr
    lw     a0, offStackSaveArea_localRefCookie(rBIX) # a0<- saved->top
    lw     a1, offThread_exception(rOBJ)             # check for exception
    sw     rFP, offThread_curFrame(rOBJ)             # self->curFrame = fp
    sw     a0, offThread_jniLocal_topCookie(rOBJ)    # new top <- old top
    lw     a0, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)

    # a0 = dalvikCallsitePC
    bnez   a1, .LhandleException                     # handle exception if any

    sw     a2, offThread_inJitCodeCache(rOBJ)        # set the mode properly
    beqz   a2, 3f
    jr     a2                                        # go if return chaining cell still exist

3:
    # continue executing the next instruction through the interpreter
    la     a1, .LdvmJitToInterpTraceSelectNoChain    # defined in footer.S
    lw     a1, (a1)
    add    rPC, a0, 3*2                              # reconstruct new rPC (advance 3 instr)

#if defined(JIT_STATS)
    li     a0, kCallsiteInterpreted
#endif
    jr     a1
