	comment "subroutine prologue"
	.macro gdbasm_enter
	std %rp, -16(%sp)
	copy %r3, %r1
	copy %sp, %r3
	std,ma %r1, 128(%sp)
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	ldd -16(%r3), %rp
	bve (%rp)
	ldd,mb -128(%sp), %r3
	.endm

	.macro gdbasm_call subr
	b,l	\subr , %rp
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
	.align 8
	.endm

	comment "Declare a data variable"
	.purgem gdbasm_datavar
	.macro gdbasm_datavar name value
	.data
\name:
	.long	\value
	.endm

