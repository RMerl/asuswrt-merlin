	comment "subroutine prologue"
	.macro gdbasm_enter
	enter #0
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	exitd
	.endm

	.macro gdbasm_call subr
	jsr.a \subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	mov.w #0, r1
       	mov.b #1, r0l
	ste.b r0l,0xe0000
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
     	ldc #0xfc00, sp
	.endm

