	comment "subroutine prologue"
	.macro gdbasm_enter
	mov.w	r6,@-r7
	mov.w	r7,r6
	subs	#2,r7
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	adds	#2,r7
	mov.w	@r7+,r6
	rts
	.endm

	.macro gdbasm_call subr
	jsr @\subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	mov.w #0xdead,r1
	mov.w #0xbeef,r2
	mov.w #0,r0
	sleep
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	.section .stack
_stack:	.long 1
	.section .text
	.global _start
_start:
	mov.w	#0,r6
	mov.w	#_stack,sp	; or mov.l #_stack,sp
	mov.w	#_edata,r0
	mov.w	#_end,r1
	sub.w   r2,r2
.loop:	mov.w	r2,@r0
	adds	#2,r0
	cmp	r1,r0
	blo	.loop
	.endm
