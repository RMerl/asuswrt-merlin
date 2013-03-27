	comment "subroutine prologue"
	.macro gdbasm_enter
	addiu %29, %29, 0xfffc
	sw %31,0(%29)
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	lw %31,0(%29)
	addiu %29, %29, 4
	jr %31
	nop
	.endm

	.macro gdbasm_call subr
	jal \subr
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
	ori  %4, %0, 1	# SYS_exit
	ori  %5, %0, 0	# normal exit code
	ori  %6, %0, 0
	ori  %7, %0, 0
	syscall
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	andi %0, %0, 0
	addiu %29, %0, 0xffc
	.endm

