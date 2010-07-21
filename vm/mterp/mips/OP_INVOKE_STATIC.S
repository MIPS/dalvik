%default { "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle a static method call.
     *
     * for: invoke-static, invoke-static/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class#CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth#BBBB */
    LOAD_rGLUE_methodClassDex(a3)		# a3<- pDvmDex
    FETCH(a1, 1)                        	# a1<- BBBB
    LOAD_base_offDvmDex_pResMethods(a3,a3)	# a3<- pDvmDex->pResMethods
    LOAD_eas2(a0, a3, a1)                       # a0<- resolved methodToCall
    EXPORT_PC()                         	# must export for invoke
						# already resolved?
    bnez    a0, common_invokeMethod${routine}   # yes, continue on
    LOAD_rGLUE_method(a3)		 	# a3<- glue->method
    LOAD_base_offMethod_clazz(a0,a3)		# a0<- method->clazz
    li      a2, METHOD_STATIC			# resolver method type
    JAL(dvmResolveMethod)			# v0<- call(clazz, ref, flags)
    move    a0, v0
    						# got null?
    bnez    v0, common_invokeMethod${routine}   # no, continue
    b       common_exceptionThrown      	# yes, handle exception

