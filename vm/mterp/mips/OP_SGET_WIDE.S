%verify "executed"
%verify "field already resolved"
%verify "field not yet resolved"
%verify "field cannot be resolved"
    /*
     * 64-bit SGET handler.
     */
    /* sget-wide vAA, field#BBBB */
    LOAD_rGLUE_methodClassDex(a2)		# a2<- DvmDex
    FETCH(a1, 1)				# a1<- field ref BBBB
    LOAD_base_offDvmDex_pResFields(a2,a2)	# a2<- dvmDex->pResFields
    LOAD_eas2(a0, a2, a1)			# a0<- resolved StaticField ptr
						# is resolved entry null?
    bnez    a0, 1f
    LOAD_rGLUE_method(a2)			# a2<- current method
    EXPORT_PC()				 	# resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0,a2)		# a0<- method->clazz
    JAL(dvmResolveStaticField)			# a0<- resolved StaticField ptr
    move    a0, v0
						# success?
    beqz    v0, common_exceptionThrown	      	# no, handle exception

1:
    GET_OPA(a1)					# a1<- AA
    LOAD64_off(a2, a3, a0, offStaticField_value) # a2/a3<- field value (aligned)
    FETCH_ADVANCE_INST(2)		 	# advance rPC, load rINST
    EAS2(a1, rFP, a1)			   	# a1<- &fp[AA]
    STORE64(a2, a3, a1)		                # vAA/vAA+1<- a2/a3
    GET_INST_OPCODE(t0)			 	# extract opcode from rINST
    GOTO_OPCODE(t0)				# jump to next instruction


