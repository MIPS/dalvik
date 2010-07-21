%verify "executed"
%verify "exception for null object"
    /*
     * Synchronize on an object.
     */
    /* monitor-enter vAA */
    GET_OPA(a2)			            # a2<- AA
    GET_VREG(a1, a2)                        # a1<- vAA (object)
    LOAD_rGLUE_self(a0)                     # a0<- glue->self
#ifdef WITH_MONITOR_TRACKING
    EXPORT_PC()                             # export PC so we can grab stack trace
#endif
                                            # null object?
    beqz  a1, common_errNullObject          # null object, throw an exception
    FETCH_ADVANCE_INST(1)                   # advance rPC, load rINST
    JAL(dvmLockObject)			    # v0= call(self, obj)

#ifdef WITH_DEADLOCK_PREDICTION /* implies WITH_MONITOR_TRACKING */
    LOAD_rGLUE_self(v0)                     # v0<- glue->self
    LOAD_base_offThread_exception(a1,v0)    # check for exception
    bnez    a1, common_exceptionThrown      # exception raised, bail out
#endif
    GET_INST_OPCODE(t0)                     # extract opcode from rINST
    GOTO_OPCODE(t0)                         # jump to next instruction

