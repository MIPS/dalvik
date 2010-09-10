#if 0 /* DRP */
/* DOUG: This code is not needed for MIPS. */
/*
 * ===========================================================================
 *  CPU-version-specific defines and utility
 * ===========================================================================
 */

/*
 * Macro for "MOV LR,PC / LDR PC,xxx", which is not allowed pre-ARMv5.
 * Jump to subroutine.
 *
 * May modify IP and LR.
 */
.macro  LDR_PC_LR source
    mov     lr, pc
    ldr     pc, \source
.endm
#endif
