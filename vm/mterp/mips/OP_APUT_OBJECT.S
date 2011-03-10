%verify "executed"
    /*
     * Store an object into an array.  vBB[vCC] <- vAA.
     *
     */
    /* op vAA, vBB, vCC */
    FETCH(a0, 1)        # a0<- CCBB
    GET_OPA(t1)         # t1<- AA
    and     a2, a0, 255       # a2<- BB
    srl     a3, a0, 8       # a3<- CC
    GET_VREG(rINST, a2)       # rINST<- vBB (array object)
    GET_VREG(a0, a3)        # a0<- vCC (requested index)
    GET_VREG(rBIX, t1)        # rBIX<- vAA
            # null array object?
    beqz    rINST, common_errNullObject   # yes, bail

    LOAD_base_offArrayObject_length(a3,rINST) # a3<- arrayObj->length
    EAS2(rOBJ, rINST, a0)         # rOBJ<- arrayObj + index*width
            # compare unsigned index, length
    bgeu    a0, a3, common_errArrayIndex  # index >= length, bail
    /*
     * On entry:
     *  rINST = vBB (arrayObj)
     *  rBIX = vAA (obj)
     *  rOBJ = offset into array (vBB + vCC * width)
     */
    bnez     rBIX, .L${opcode}_checks   # yes, skip type checks
.L${opcode}_finish:
    FETCH_ADVANCE_INST(2)     # advance rPC, load rINST
    GET_INST_OPCODE(t0)       # extract opcode from rINST
    sw     rBIX, offArrayObject_contents(rOBJ)  # vBB[vCC]<- vAA
    GOTO_OPCODE(t0)       # jump to next instruction

%break
.L${opcode}_checks:
    LOAD_base_offObject_clazz(a0, rBIX)   # a0<- obj->clazz
    LOAD_base_offObject_clazz(a1,rINST)   # a1<- arrayObj->clazz
    JAL(dvmCanPutArrayElement)      # test object type vs. array type
    beqz     v0, common_errArrayStore   # okay ?
    lw      a2, offGlue_cardTable(rGLUE)
    srl     t1, rINST, GC_CARD_SHIFT
    addu    t2, a2, t1
    sb      a2, (t2)
    b        .L${opcode}_finish     # yes, skip type checks

