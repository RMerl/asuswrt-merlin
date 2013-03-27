	comment "subroutine prologue"
	.macro gdbasm_enter
	link	%a6,#0
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	unlk	%a6
	rts
	.endm

	.macro gdbasm_call subr
	jbsr	\subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	illegal
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	lea	0,%a6
	.endm

	comment "Declare a data variable"
	.purgem gdbasm_datavar
	.macro gdbasm_datavar name value
	.data
\name:
	.long	\value
	.endm
