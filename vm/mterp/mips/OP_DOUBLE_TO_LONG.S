%verify "executed"
%include "mips/unflopWide.S" {"instr":"b    d2l_doconv","st_result":"STORE64(v0, v1, rOBJ)"}
%break

d2l_doconv:
#ifdef	SOFT_FLOAT
    ld		a2, .LDOUBLE_TO_LONG_max        # maxlong, as a double (high word)
    move	rBIX, a0			# save a0	
    move	rSELF, a1			#  and a1
    JAL(__gedf2)

    move	t0, v0
    ld		v0, .LDOUBLE_TO_LONG_ret_max
    bgez	t0, .L${opcode}_set_vreg

    move	a0, rBIX
    move	a1, rSELF
    ld		a2, .LDOUBLE_TO_LONG_min        # minlong, as a double (high word)
    JAL(__ledf2)

    move	t0, v0
    ld		v0, .LDOUBLE_TO_LONG_ret_min
    blez	t0, .L${opcode}_set_vreg
    
    move	a0, rBIX
    move	a1, rSELF
    move	a2, rBIX
    move	a3, rSELF
    JAL(__nedf2)

   move		t0, v0
   li		v0, 0
   li		v1, 0
   bnez		v0, .L${opcode}_set_vreg

   move		a0, rBIX
   move		a1, rSELF
   JAL(__fixdfdi)

#else
    l.d		fa1, .LDOUBLE_TO_LONG_max       # maxlong, as a double (high word)
    c.ole.d	fcc0, fa1, fa0
    l.d		fv0, .LDOUBLE_TO_LONG_ret_max
    bc1t	.L${opcode}_set_vreg
	
    l.d		fa1, .LDOUBLE_TO_LONG_min       # minlong, as a double (high word)
    c.ole.d	fcc0, fa0, fa1
    l.d		fv0, .LDOUBLE_TO_LONG_ret_min
    bc1t	.L${opcode}_set_vreg
    
    mov.d	fa1, fa0
    c.un.d	fcc0, fa0, fa1
    li.d	fv0, 0
    bc1t	.L${opcode}_set_vreg
    JAL(__fixdfdi)
#endif
    b		.L${opcode}_set_vreg


.LDOUBLE_TO_LONG_max:
    .dword   0x43e0000000000000                  # maxlong, as a double (high word)
.LDOUBLE_TO_LONG_min:
    .dword   0xc3e0000000000000                  # minlong, as a double (high word)
.LDOUBLE_TO_LONG_ret_max:
    .dword   0x7fffffffffffffff
.LDOUBLE_TO_LONG_ret_min:
    .dword   0x8000000000000000
