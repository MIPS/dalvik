%verify "branch taken"
%verify "branch not taken"
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                         	# a0<- AA
    GET_VREG(a2, a0)                    	# a2<- vAA
    li      rBIX, 4               		# rBIX<- BYTE branch dist for not-taken
    b${revcmp}  a2, 0, 1f               	# branch to 1 if comparison failed
    FETCH_S(rBIX, 1)                      	# rBIX<- branch offset, in code units
    sll     rBIX, 1 				# convert to bytes, check sign
    bltz    rBIX, common_backwardBranch   	# backward branch, do periodic checks
1:
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE(a0)
    FETCH_ADVANCE_INST_RB(rBIX)           	# update rPC, load rINST
    bnez    a0, common_updateProfile # ??? DRP should target be common_testUpdateProfile?
    GET_INST_OPCODE(t0)                 	# extract opcode from rINST
    GOTO_OPCODE(t0)                     	# jump to next instruction
#else
    FETCH_ADVANCE_INST_RB(rBIX)           	# update rPC, load rINST
    GET_INST_OPCODE(t0)                 	# extract opcode from rINST
    GOTO_OPCODE(t0)                     	# jump to next instruction
#endif

