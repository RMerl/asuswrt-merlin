	comment "subroutine prologue"
	.macro gdbasm_enter
	add -8,sp
	st.w r31,4[sp]
	st.w r29,0[sp]
	mov sp,r29
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	mov r29,sp
	ld.w 0[sp],r29
	ld.w 4[sp],r31
	add 8,sp
	jmp [r31]
	.endm

	.macro gdbasm_call subr
	jarl \subr,r31
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	halt
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	movea 255,r0,r20
	mov r0, r21
	ori 65535, r0, r21
	movhi 32, r0, sp
	.endm

