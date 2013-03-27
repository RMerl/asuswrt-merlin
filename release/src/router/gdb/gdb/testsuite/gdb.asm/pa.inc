	comment "subroutine prologue"
	.macro gdbasm_enter
	stw %rp, -20(%sp)
	copy %r3, %r1
	copy %sp, %r3
	stwm %r1, 64(%sp)
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	ldw -20(%r3), %rp
	bv %r0(%rp)
	ldwm -64(%sp), %r3
	.endm

	.macro gdbasm_call subr
	bl	\subr , %rp
	nop
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	comment "Don't know how to exit, but this will certainly halt..."
	ldw	0(%r0), %r1
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	.align 4
	.endm

	comment "Declare a data variable"
	.purgem gdbasm_datavar
	.macro gdbasm_datavar name value
	.data
\name:
	.long	\value
	.endm
