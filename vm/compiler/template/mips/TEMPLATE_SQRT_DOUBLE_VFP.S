%verify "executed"

    /*
     * 64-bit floating point vfp sqrt operation.
     * If the result is a NaN, bail out to library code to do
     * the right thing.
     *
     * Since MIPS don't have a 64-bit sqrt function, we bail
     * out to library code right away.
     *
     * On entry:
     *     a2 src addr of op1
     * On exit:
     *     v0,v1 = res
     */
    LOAD64(rARG0, rARG1, a2)        # a0/a1<- vBB/vBB+1
    JAL(sqrt)
    RETURN
