%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle a direct method call.
     *
     * (We could defer the "is 'this' pointer null" test to the common
     * method invocation code, and use a flag to indicate that static
     * calls don't count.  If we do this as part of copying the arguments
     * out we could avoiding loading the first arg twice.)
     *
     * for: invoke-direct, invoke-direct/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class#CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth#BBBB */
    LOAD_rGLUE_methodClassDex(a3)               # a3<- pDvmDex
    FETCH(a1, 1)                                # a1<- BBBB
    LOAD_base_offDvmDex_pResMethods(a3,a3)      # a3<- pDvmDex->pResMethods
    FETCH(rOBJ, 2)                              # rOBJ<- GFED or CCCC
    LOAD_eas2(a0, a3, a1)		        # a0<- resolved methodToCall
    .if     (!$isrange)
    and     rOBJ, rOBJ, 15                      # rOBJ<- D (or stays CCCC)
    .endif
    EXPORT_PC()                                 # must export for invoke
    GET_VREG(a2, rOBJ)                          # a2<- "this" ptr
				                # already resolved?
    bnez    a0, 1f				# resolved, call the function

    LOAD_rGLUE_method(a3)		        # a3<- glue->method
    LOAD_base_offMethod_clazz(a0, a3)	        # a0<- method->clazz
    li      a2, METHOD_DIRECT                   # resolver method type
    JAL(dvmResolveMethod)			# v0<- call(clazz, ref, flags)
    move    a0, v0
                                                # got null?
    GET_VREG(a2, rOBJ)                          # a2<- "this" ptr (reload)
    beqz    v0, common_exceptionThrown          # yes, handle exception

1:
    bnez    a2, common_invokeMethod${routine}   # no, continue on
    b       common_errNullObject                # yes, throw exception



