%verify "executed"
%verify "class not resolved"
%verify "class cannot be resolved"
%verify "class not initialized"
%verify "class fails to initialize"
%verify "class already resolved/initialized"
%verify "class is abstract or interface"
%verify "allocation fails"
    /*
     * Create a new instance of a class.
     */
    /* new-instance vAA, class #BBBB */
    LOAD_rGLUE_methodClassDex(a3)		        # a3<- pDvmDex
    FETCH(a1, 1)                                	# a1<- BBBB
    LOAD_base_offDvmDex_pResClasses(a3,a3)		# a3<- pDvmDex->pResClasses
    LOAD_eas2(a0, a3, a1)		                # a0<- resolved class
    EXPORT_PC()                                         # req'd for init, resolve, alloc
                                                        # already resolved?
    beqz    a0, .L${opcode}_resolve                     # no, resolve it now
.L${opcode}_resolved:                                   # a0=class
    lbu     a1, offClassObject_status(a0) 		# a1<- ClassStatus enum
                                                        # has class been initialized?
    li      t0, CLASS_INITIALIZED
    move    rOBJ, a0                                    # save a0
    bne     a1, t0, .L${opcode}_needinit		# no, init class now

.L${opcode}_initialized:				# a0=class
    LOAD_base_offClassObject_accessFlags(a3, a0)	# a3<- clazz->accessFlags
    and     t0, a3, (ACC_INTERFACE|ACC_ABSTRACT)        # abstract or interface?
    li      a1, ALLOC_DONT_TRACK                        # flags for alloc call
    bnez    t0,  .L${opcode}_abstract                   # fail
            						# concrete class, fall thru
		        				# a0=class
    JAL(dvmAllocObject)				# v0<- new object
    GET_OPA(a3)			                   	# a3<- AA
                                                        # failed?
    beqz    v0, common_exceptionThrown                  # yes, handle the exception
    FETCH_ADVANCE_INST(2)                       	# advance rPC, load rINST
    b      .L${opcode}_continue

%break

.L${opcode}_continue:
    GET_INST_OPCODE(t0)                         	# extract opcode from rINST
    SET_VREG(v0, a3)					# vAA<- v0
    GOTO_OPCODE(t0)                             	# jump to next instruction

    /*
     * Class initialization required.
     *
     *  a0 holds class object
     */
.L${opcode}_needinit:
    JAL(dvmInitClass)					# initialize class
    move    a0, rOBJ                              	# restore a0
                                                        # check boolean result
    bnez     v0, .L${opcode}_initialized                # success, continue
    b       common_exceptionThrown                      # failed, deal with init exception


    /*
     * Resolution required.  This is the least-likely path.
     *
     *  a1 holds BBBB
     */
.L${opcode}_resolve:
    LOAD_rGLUE_method(a3)		         	# a3<- glue->method
    li      a2, 0                               	# a2<- false
    LOAD_base_offMethod_clazz(a0,a3)		        # a0<- method->clazz
    JAL(dvmResolveClass)		# v0<- resolved ClassObject ptr
    move     a0, v0
                                                   	# got null?
    bnez     v0, .L${opcode}_resolved                   # no, continue
    b       common_exceptionThrown                      # yes, handle exception

    /*
     * We can't instantiate an abstract class or interface, so throw an
     * InstantiationError with the class descriptor as the message.
     *
     *  a0 holds class object
     */
.L${opcode}_abstract:
    LOAD_base_offClassObject_descriptor(a1, a0)
    lw      a0, .LstrInstantiationErrorPtr
    JAL(dvmThrowExceptionWithClassMessage)
    b       common_exceptionThrown

.LstrInstantiationErrorPtr:
    .word   .LstrInstantiationError

