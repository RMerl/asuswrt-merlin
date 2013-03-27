	comment "subroutine prologue"
	.macro gdbasm_enter
	push	%rbp
	mov	%rsp,%rbp
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	pop	%rbp
	ret
	.endm

	.macro gdbasm_call subr
	call	\subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	hlt
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	xor	%rbp, %rbp
	.endm

	comment "Declare a data variable"
	.purgem gdbasm_datavar
	.macro gdbasm_datavar name value
	.data
\name:
	.long	\value
	.endm
