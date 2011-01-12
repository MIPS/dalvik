%default { "barrier":"# no-op " }
%verify "executed"
%verify "field already resolved"
%verify "field not yet resolved"
%verify "field cannot be resolved"
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-object, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field #BBBB */
    LOAD_rGLUE_methodClassDex(a2)		# a2<- DvmDex
    FETCH(a1, 1)				# a1<- field ref BBBB
    LOAD_base_offDvmDex_pResFields(a2,a2)	# a2<- dvmDex->pResFields
    LOAD_eas2(a0, a2, a1)			# a0<- resolved StaticField ptr
    bnez   a0, 1f				# is resolved entry null?
    /*
     *  a1: BBBB field ref
     */
    LOAD_rGLUE_method(a2)			# a2<- current method
    EXPORT_PC()					# resolve() may throw, so export now
    LOAD_base_offMethod_clazz(a0,a2)		# a0<- method->clazz
    JAL(dvmResolveStaticField)			# v0<- resolved StaticField ptr
    move    a0, v0
    beqz    v0, common_exceptionThrown		# success? no, handle exception
1:						# field ptr in a0
    GET_OPA(a2)					# a2<- AA
    FETCH_ADVANCE_INST(2)			# advance rPC, load rINST
    GET_VREG(a1, a2)				# a1<- fp[AA]
    GET_INST_OPCODE(t0)				# extract opcode from rINST
    $barrier                                    # releasing store
    sw    a1, offStaticField_value(a0)		# field<- vAA
    GOTO_OPCODE(t0)				# jump to next instruction
