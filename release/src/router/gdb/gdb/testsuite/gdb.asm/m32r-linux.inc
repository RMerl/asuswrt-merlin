	comment "subroutine prologue"
	.macro gdbasm_enter
	push fp -> push lr
	addi sp,#-4 -> mv fp,sp
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	addi sp,#4 -> pop lr
	pop fp -> jmp lr
	.endm

	.macro gdbasm_call subr
	bl	\subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	ldi r0,#1 -> ldi r1,#0
	ldi r2,#0 -> ldi r3,#0
	trap #0 -> nop
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	ldi fp,#0
	.endm
