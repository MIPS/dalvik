%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
%verify "null object"
    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class#CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth#BBBB */
    LOAD_rGLUE_methodClassDex(a3)		# a3<- pDvmDex
    FETCH(a1, 1)				# a1<- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3)	# a3<- pDvmDex->pResMethods
    FETCH(rOBJ, 2)				# rOBJ<- GFED or CCCC
    LOAD_eas2(a0, a3, a1)			# a0<- resolved baseMethod
    .if     (!$isrange)
    and     rOBJ, rOBJ, 15			# rOBJ<- D (or stays CCCC)
    .endif
    EXPORT_PC()					# must export for invoke
						# already resolved?
    bnez     a0, .L${opcode}_continue		# yes, continue on

    LOAD_rGLUE_method(a3)			# a3<- glue->method
    LOAD_base_offMethod_clazz(a0,a3)		# a0<- method->clazz
    li      a2, METHOD_VIRTUAL			# resolver method type
    JAL(dvmResolveMethod)			# v0<- call(clazz, ref, flags)
    move    a0, v0
						# got null?
    bnez    v0, .L${opcode}_continue		# no, continue
    b       common_exceptionThrown		# yes, handle exception
%break

    /*
     * At this point:
     *  a0 = resolved base method
     *  rOBJ= C or CCCC (index of first arg, which is the "this" ptr)
     */
.L${opcode}_continue:
    GET_VREG(a1, rOBJ)		   		# a1<- "this" ptr
    LOADu2_offMethod_methodIndex(a2, a0)	# a2<- baseMethod->methodIndex
						# is "this" null?
    beqz    a1, common_errNullObject		# null "this", throw exception
    LOAD_base_offObject_clazz(a3, a1)		# a3<- thisPtr->clazz
    LOAD_base_offClassObject_vtable(a3, a3)	# a3<- thisPtr->clazz->vtable
    LOAD_eas2(a0, a3, a2)			# a0<- vtable[methodIndex]
    b	 common_invokeMethod${routine}		# continue on

