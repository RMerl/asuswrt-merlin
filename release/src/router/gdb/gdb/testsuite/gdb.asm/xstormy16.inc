	comment "subroutine prologue"
	.macro gdbasm_enter
	push r13
	mov r13,r15
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	pop r13
	ret
	.endm

	.macro gdbasm_call subr
	callf \subr
	.endm

	.macro gdbasm_several_nops
	add r0,#0
	add r0,#0
	add r0,#0
	add r0,#0
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	mov.w r2,#0
	halt
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	mov r15,#2
	.endm

