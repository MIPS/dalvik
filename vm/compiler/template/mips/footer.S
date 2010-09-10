/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .text
    .align  4
.LinvokeNative:
    # Prep for the native call
    # a1 = newFP, a0 = methodToCall
    lw     a3, offGlue_self(rGLUE)               # a3<- glue->self
    lw     t9, offThread_jniLocal_topCookie(a3)  # t9<- thread->localRef->...
    sw     zero, offThread_inJitCodeCache(a3)    # not in jit code cache
    sw     a1, offThread_curFrame(a3)            # self->curFrame = newFp
    sw     t9, (offStackSaveArea_localRefCookie - sizeofStackSaveArea)(a1)
                                                 # newFp->localRefCookie=top
    move   rOBJ, a3                              # rOBJ<- glue->self (preserve)
    SAVEAREA_FROM_FP(rBIX, a1)                   # rBIX<- new stack save area

    move    a2, a0                               # a2<- methodToCall
    move    a0, a1                               # a0<- newFp
    add     a1, rGLUE, offGlue_retval            # a1<- &retval

    lw      t9, offMethod_nativeFunc(a2)
    JALR(t9)                                     # call methodToCall->nativeFunc
    lw      gp, STACK_OFFSET_GP(sp)

    # Refresh Jit's on/off status
    lw     a3, offGlue_ppJitProfTable(rGLUE)

    # native return; rOBJ=self, rBIX=newSaveArea
    # equivalent to dvmPopJniLocals
    lw     a2, offStackSaveArea_returnAddr(rBIX)     # a2 = chaining cell ret addr
    lw     a0, offStackSaveArea_localRefCookie(rBIX) # a0<- saved->top
    lw     a1, offThread_exception(rOBJ)             # check for exception
    lw     a3, (a3)                                  # a3 <- pointer to Jit profile table
    sw     rFP, offThread_curFrame(rOBJ)             # self->curFrame = fp
    sw     a0, offThread_jniLocal_topCookie(rOBJ)    # new top <- old top
    lw     a0, offStackSaveArea_savedPc(rBIX)        # reload rPC
    sw     a3, offGlue_pJitProfTable(rGLUE)          # cache current JitProfTable

    # a0 = dalvikCallsitePC
    bnez   a1, .LhandleException                     # handle exception if any

    sw     a2, offThread_inJitCodeCache(rOBJ)        # set the mode properly
    beqz   a2, 3f
    jr     a2                                        # go if return chaining cell still exist

3:
    # continue executing the next instruction through the interpreter
    la     a1, .LdvmJitToInterpTraceSelectNoChain    # defined in footer.S
    lw     a1, (a1)
    add    rPC, a0, 3*2                              # reconstruct new rPC

#if defined(JIT_STATS)
    li     a0, kCallsiteInterpreted
#endif
    jr     a1


/*
 * On entry:
 * a0  Faulting Dalvik PC
 */
.LhandleException:
#if defined(WITH_SELF_VERIFICATION)
    la     t0, .LdeadFood
    lw     t0, (t0)                  # should not see this under self-verification mode
    jr     t0
.LdeadFood:
    .word   0xdeadf00d
#endif
    lw     a3, offGlue_self(rGLUE)              # a3<- glue->self
    sw     zero, offThread_inJitCodeCache(a3)   # in interpreter land
    la     a1, .LdvmMterpCommonExceptionThrown  # PIC way of getting &func
    lw     a1, (a1)
    la     rIBASE, .LdvmAsmInstructionStart     # PIC way of getting &func
    lw     rIBASE, (rIBASE)
    move   rPC, a0                              # reload the faulting Dalvid address
    jr     a1                                   # branch to dvmMterpCommonExeceptionThrown

    .align  4
.LdvmAsmInstructionStart:
    .word   dvmAsmInstructionStart
.LdvmJitToInterpTraceSelectNoChain:
    .word   dvmJitToInterpTraceSelectNoChain
.LdvmJitToInterpNoChain:
    .word   dvmJitToInterpNoChain
.LdvmMterpStdBail:
    .word   dvmMterpStdBail
.LdvmMterpCommonExceptionThrown:
    .word   dvmMterpCommonExceptionThrown
.LdvmLockObject:
    .word   dvmLockObject
#if defined(WITH_SELF_VERIFICATION)
.LdvmSelfVerificationMemOpDecode:
    .word   dvmSelfVerificationMemOpDecode
#endif
#if 0 /* !!! DRP handle __aeabi_cXcmple */
.L__aeabi_cdcmple:
    .word   __aeabi_cdcmple
.L__aeabi_cfcmple:
    .word   __aeabi_cfcmple
#endif

    .global dmvCompilerTemplateEnd
dmvCompilerTemplateEnd:

#endif /* WITH_JIT */
