	comment "subroutine prologue"
	.macro gdbasm_enter
	save	%sp, -192, %sp
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	return %i7 + 8
	nop
	.endm

	.macro gdbasm_call subr
	call	\subr
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
	ldx	[%g0], %i0
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	clr	%fp
	.endm

