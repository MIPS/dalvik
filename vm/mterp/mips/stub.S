    /* (stub) */
	SAVE_PC_TO_GLUE()
	SAVE_FP_TO_GLUE()
	move  a0, rGLUE
	JAL(dvmMterp_${opcode})
	LOAD_PC_FROM_GLUE()
	LOAD_FP_FROM_GLUE()
	FETCH_INST()
	GET_INST_OPCODE(t0)
	GOTO_OPCODE(t0)
