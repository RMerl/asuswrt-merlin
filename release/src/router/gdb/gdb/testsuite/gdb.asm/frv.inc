	comment "subroutine prologue"
	.macro gdbasm_enter
	addi sp,#-16,sp
	sti fp, @(sp,0)
	mov sp, fp
	movsg lr, gr5
	sti gr5, @(fp,8)
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	ldi @(fp,8), gr5
	ld @(fp,gr0), fp
	addi sp,#16,sp
	jmpl @(gr5,gr0)
	.endm

	.macro gdbasm_call subr
	call \subr
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
	ldi @(gr0,0), gr5
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	call	.Lcall
.Lcall:	movsg	lr, gr4
	sethi   #gprelhi(.Lcall), gr5
	setlo	#gprello(.Lcall), gr5
	sub	 gr4, gr5, gr16

	sethi	 #gprelhi(_stack), sp
	setlo    #gprello(_stack), sp
	setlos   #0, fp
	add	 sp, gr16, sp
	.endm

	comment "Declare a data variable"
	.purgem gdbasm_datavar
	.macro gdbasm_datavar name value
	.data
\name:
	.long	\value
	.endm
