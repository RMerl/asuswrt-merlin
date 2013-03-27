	comment "subroutine prologue"
	.macro gdbasm_enter
	mov ip, sp
	stmdb sp!, {fp, ip, lr, pc}
	sub fp, ip, #4
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	ldmea fp, {fp, sp, pc}
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
	mov r0, #0
	swi 0x00123456
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	mov fp, #0
	mov r7, #0
	.endm

