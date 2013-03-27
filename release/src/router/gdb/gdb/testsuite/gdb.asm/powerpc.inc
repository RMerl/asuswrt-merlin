	comment "subroutine prologue"
	.macro gdbasm_enter
	stwu	1, -16(1)
	stw	31, 8(1)
	mr	31, 1
	mflr	0
	stw	0, 20(1)
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	lwz	0, 20(1)
	mtlr	0
	lwz	31, 8(1)
	lwz	1, 0(1)
	blr
	.endm

	.macro gdbasm_call subr
	bl \subr
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
	li	0, 0
	lwz 	0, 0(0)
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	.p2align 2
	.endm

	comment "Declare a data variable"
	.purgem gdbasm_datavar
	.macro gdbasm_datavar name value
	.data
\name:
	.long   \value
	.endm
