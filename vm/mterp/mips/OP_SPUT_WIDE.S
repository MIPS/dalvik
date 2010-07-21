%verify "executed"
%verify "field already resolved"
%verify "field not yet resolved"
%verify "field cannot be resolved"
    /*
     * 64-bit SPUT handler.
     */
    /* sput-wide vAA, field#BBBB */
    LOAD_rGLUE_methodClassDex(a2)		# a2<- DvmDex
    FETCH(a1, 1)				# a1<- field ref BBBB
    LOAD_base_offDvmDex_pResFields(a2, a2)	# a2<- dvmDex->pResFields
    GET_OPA(t0)					# t0<- AA
    LOAD_eas2(a0, a2, a1)			# a0<- resolved StaticField ptr
    EAS2(rOBJ, rFP, t0)				# rOBJ- &fp[AA]
						# is resolved entry null?
    bnez    a0, 1f				# no, do the move

    /*
     *  a1: BBBB field ref
     *  rOBJ &fp[AA]
     */
    LOAD_rGLUE_method(a2)			# a2<- current method
    EXPORT_PC()					# resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)		# a0<- method->clazz
    JAL(dvmResolveStaticField)			# v0<- resolved StaticField ptr
    move    a0, v0
    beqz    v0, common_exceptionThrown		# success? no, handle exception

1:						# field ptr in a0, AA in rOBJ
    FETCH_ADVANCE_INST(2)			# advance rPC, load rINST
    LOAD64(a2, a3, rOBJ)			# a2/a3<- vAA/vAA+1
    GET_INST_OPCODE(t0)				# extract opcode from rINST
    STORE64_off(a2, a3, a0, offStaticField_value) # field<- vAA/vAA+1
    GOTO_OPCODE(t0)				# jump to next instruction
