%default { "barrier":"# no-op " }
%verify "executed"
%verify "field already resolved"
%verify "field not yet resolved"
%verify "field cannot be resolved"
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field  #BBBB */
    LOAD_rGLUE_methodClassDex(a2)		# a2<- DvmDex
    FETCH(a1, 1)                                # a1<- field ref BBBB
    LOAD_base_offDvmDex_pResFields(a2, a2)	# a2<- dvmDex->pResFields
    LOAD_eas2(a0, a2, a1)		        # a0<- resolved StaticField ptr
                                                # is resolved entry !null?
    bnez    a0, 1f

    LOAD_rGLUE_method(a2)		        # a2<- current method
    EXPORT_PC()                                 # resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)		# a0<- method->clazz
    JAL(dvmResolveStaticField)			# v0<- resolved StaticField ptr
    move    a0, v0
                                                # success?
    beqz    v0, common_exceptionThrown          # no, handle exception

1:
    LOAD_base_offStaticField_value(a1, a0)	# a1<- field value
    $barrier                                    # acquiring load
    GET_OPA(a2)			                # a2<- AA
    FETCH_ADVANCE_INST(2)                       # advance rPC, load rINST
    GET_INST_OPCODE(t0)                         # extract opcode from rINST
    SET_VREG_GOTO(a1, a2, t0)                   # fp[AA]<- a1



