%verify "executed"
    /* move-result-wide vAA */
    GET_OPA(a2)			        # a2<- AA
    addu    a3, rGLUE, offGlue_retval   # a3<- &glue->retval
    EAS2(a2, rFP, a2)    	        # a2<- &fp[AA]
    LOAD64(a0, a1, a3)		# a0/a1<- retval.j
    FETCH_ADVANCE_INST(1)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    STORE64(a0, a1, a2)		# fp[AA]<- a0/a1
    GOTO_OPCODE(t0)                     # jump to next instruction

