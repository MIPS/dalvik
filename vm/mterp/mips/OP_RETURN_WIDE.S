%verify "executed"
    /*
     * Return a 64-bit value.  Copies the return value into the "glue"
     * structure, then jumps to the return handler.
     */
    /* return-wide vAA */
    GET_OPA(a2)			        # a2<- AA
    EAS2(a2, rFP, a2)    	        # a2<- &fp[AA]
    addu    a3, rGLUE, offGlue_retval   # a3<- &glue->retval
    LOAD64(a0, a1, a2)			# a0/a1 <- vAA/vAA+1
    STORE64(a0, a1, a3)			# retval<- a0/a1
    b       common_returnFromMethod

