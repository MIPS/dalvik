/*
 * JNI method invocation.  This is used to call a C/C++ JNI method.  The
 * argument list has to be pushed onto the native stack according to
 * local calling conventions.
 *
 * This version supports the MIPS O32 ABI.
 */




/*
Function prototype:

void dvmPlatformInvoke(void* pEnv, ClassObject* clazz, int argInfo, int argc,
    const u4* argv, const char* signature, void* func, JValue* pReturn)

The method we are calling has the form:

  return_type func(JNIEnv* pEnv, ClassObject* clazz, ...)
    -or-
  return_type func(JNIEnv* pEnv, Object* this, ...)

We receive a collection of 32-bit values which correspond to arguments from
the interpreter (e.g. float occupies one, double occupies two).  It's up to
us to convert these into local calling conventions.

Please notice that argc in dvmPlatformInvoke does NOT include pEnv and clazz/this.
*/

    .text
    .align  2
    .globl dvmPlatformInvoke
    .ent dvmPlatformInvoke
/*
 * On entry:
 *   a0  JNIEnv (can be left alone)
 *   a1  clazz (NULL for virtual method calls, non-NULL for static)
 *   a2  arg info
 *   a3  argc (number of 32-bit values in argv)
 *   MIPS reservers 16 bytes on stack even if the first 4 args are passed by
 *   reg a0-a3. That's different from ARM.
 *   [sp + 16]  argv
 *   [sp + 20]  short signature
 *   [sp + 24]  func
 *   [sp + 28]  pReturn
 *
 * For a virtual method call, the "this" reference is in argv[0].
 *
 * argInfo (32-bit int) layout:
 *   SRRRLLLL FFFFFFFF FFFFFFFF FFFFFFFF
 *
 *   S - if set, do things the hard way (scan the signature)
 *   R - return type enumeration, really only important for hardware FP
 *   L - number of double-words of storage required on stack (0-30 words)
 *   F - pad flag -- if set, write a pad word to the stack
 *
 * With this arrangement we can efficiently push up to 24 words of arguments
 * onto the stack.  Anything requiring more than that -- which should happen
 * rarely to never -- can do the slow signature scan.
 *
 * (We could pack the Fs more efficiently -- we know we never push two pads
 * in a row, and the first word can never be a pad -- but there's really
 * no need for it.)
 *
 * NOTE: if the called function has more than 4 words of arguments, gdb
 * will not be able to unwind the stack past this method.  The only way
 * around this is to convince gdb to respect an explicit frame pointer.
 */

 /* Stack:
  *                     High
  *                 ____________
  *                 |__28______| pReturn
  *                 |__24______| func
  *                 |__20______| short signature
  *                 |__16______| argv
  *                 |__12______| reserved
  *                 |__8_______| reserved
  *                 |__4_______| reserved
  *__sp on entry_->_|__0_______|_reserved_____________
  *                 |__________| saved ra
  *                 |__________| saved s8
  *                 |__________| saved s0
  *                 |__________| saved s1
  *                 |__________| saved s2
  *                 |__________| pad for 8 bytes aligned
  *                 |__________| other argv or pad
  *                 |__________| other argv or pad
  *                 |__________| other argv or pad
  *                 |__________| other argv or pad
  *                 |__________| other argv or pad
  *                 |__________| other argv or pad
  *                 |__________| reserved for a3
  *                 |__________| reserved for a2
  *                 |__________| reserved for a1
  *_____new sp___-> |__________| reserved for a0
  * (new sp: sp when call native method)
  */

 /* Register usage:
  *
  *  s0: pReturn
  *  s1: stack frame size
  *  s2: Return type
  * These registers should be saved to and restrored from stack.
  *
  *  t0: argv
  *  t9: func
  * These registers do not need to be saved.
  *
  * We put the stack size into register s1 because we can not know the size of stack
  * at the beginning. This size can be calculated with the help of hints in jniarginfo.
  *
  */

dvmPlatformInvoke:

	/*  Do we have arg padding flags in "argInfo"? Check bit 31 */
	srl $t1,$a2,31
	bne $t1,$zero,.Lno_arginfo

	/*fast path. We have hints. */
	/* save s8 and ra to stack */
	sw $ra,-4($sp)
	sw $s8,-8($sp)
	sw $s0,-12($sp)
	sw $s1,-16($sp)
	sw $s2,-20($sp)

	/* t0 <- argv */
	lw $t0,16($sp)
	/* t9 <- func */
	lw $t9,24($sp)
	/* s0<- pReturn */
	lw $s0,28($sp)


	/* Is the method static? */
	bne $a1,$zero,1f
	/* Not static: set a1 to *argv++ ("this"), and set argc--*/
	lw $a1,($t0)
	addiu $t0,$t0,4
	addiu $a3,$a3,-1
1:
.Lhave_arginfo:
	/* expand the stack for args */
	lui $t1,0x0f00     //t1 = 0x0f00 0000
	srl $s2,$a2,28     //s2 = return type
	and $t2,$a2,$t1
	srl $s1,$t2,21     //s1 = (t2>>24)<<3. bytes required on stack
	addiu $s1,$s1,40   /*ship saved ra/s8/s0/s1/s2/pad/a0/a1/a2/a3 */
	addiu $t5,$sp,-24
	subu $sp,$sp,$s1
	addiu $t1,$sp,8

	 /*
	 * t0 :argv
	 * t1 :sp+8(first arg position in stack except pEnv and clazz/this)
	 * a2 :argInfo
	 * a3 :argc
	 * sp :new stack bottom
	 */

	/* first two args or one args and pad */
	blez $a3,.Largs_done
	lw $t2,($t0)
	srl $a2,$a2,1
	sw $t2,($t1)
	addiu $t0,$t0,4
	addiu $t1,$t1,4
	addiu $a3,$a3,-1
	blez $a3,.Largs_done
	andi $t3,$a2,0x1
	/* the second position is a pad? */
	bne $t3,$zero,.Lpad0
	lw $t2,($t0)
	sw $t2,($t1)
	addiu $t0,$t0,4
	addiu $a3,$a3,-1
.Lpad0:
	addiu $t1,$t1,4
	srl $a2,$a2,1
	blez $a3,.Largs_done

.Lloop1:
	/* copy other args
	 * t5: sp top for args
	 * t1: sp for next arg
	 */
	beq $t1,$t5,.Largs_done
	andi $t3,$a2,0x1
	srl $a2,$a2,1
	bne $t3,$zero,.Lpad
	lw $t2,($t0)
	sw $t2,($t1)
	addiu $t0,$t0,4
.Lpad:
	addiu $t1,$t1,4
	b .Lloop1

.Largs_done:

	/*
	 * We have copied args into stacks. Then copy argv[0]/argv[1] into
	 * reg a2/a3. You may find that if argv[0] is 32 bits and argv[1]
	 * is 64 bits, then we do not need to set reg a3 since it is a pad.
	 * However, copy a3 from argv is harmless. We do not need to set
	 * a0(pEnv)/a1(clazz/this) since they are already there.
	 */

	/*
	 * sp: new stack
	 * s0: pReturn
	 * s1: stack frame size
	 * s2: Return type
	 *
	 */
	lw $a2,8($sp)
	lw $a3,12($sp)

	/* Linux/PIC needs $t9 points to function address.
	 * call the function
	 */
	jalr $t9

	/* function call return */
	/* 1. check the return type
	 * 2. if the return type is not DALVIK_JNI_RETURN_VOID then copy v0/v1
	 *    to pReturn
	 */
	beq $s2,$zero,.Lend
	sw $v0,($s0)
	sw $v1,4($s0)
	
.Lend:
	/* restore saved register */
	addu $sp,$sp,$s1
	lw $ra,-4($sp)
	lw $s8,-8($sp)
	lw $s0,-12($sp)
	lw $s1,-16($sp)
	lw $s2,-20($sp)
	jr $ra

/* slow path. We call the generic routine here */
.Lno_arginfo:

.set noreorder
.cpload $t9
.set reorder
	/* save ra and t9 */
	sw $ra,-4($sp)
	sw $t9,-8($sp)

	/* prepare args */
	lw $t0,16($sp)
	lw $t1,20($sp)
	lw $t2,24($sp)
	lw $t3,28($sp)

	sw $t0,-24($sp)
	sw $t1,-20($sp)
	sw $t2,-16($sp)
	sw $t3,-12($sp)

	addiu $sp,$sp,-40
	la $t9,dvmPlatformInvokeMIPS
	jalr $t9

	/* function call return */
	lw $t9,32($sp)
	lw $ra,36($sp)
	addiu $sp,$sp,40
	jr $ra

.end dvmPlatformInvoke

