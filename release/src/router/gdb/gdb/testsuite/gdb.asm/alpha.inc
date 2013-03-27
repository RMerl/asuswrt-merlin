	comment "subroutine declare"
	.purgem gdbasm_declare
	.macro gdbasm_declare name
	.ent	\name
\name:
	.endm

	comment "subroutine prologue"
	.macro gdbasm_enter
	.frame	$30, 16, $26, 0
	.mask	0x04000000, -16
	ldgp	$gp, 0($27)
	subq	$sp, 16, $sp
	stq	$26, 0($sp)
	.prologue 1
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	ldq	$26, 0($sp)
	addq	$sp, 16, $sp
	ret
	.endm

	comment "subroutine end"
	.purgem gdbasm_end
	.macro gdbasm_end name
	.end	\name
	.endm

	comment "subroutine call"
	/* Can't use ldgp here because the finish-frame test expects the
	   pc to wind up on the next line.  That's ok, we're all local.  */
	.macro gdbasm_call subr
	jsr	$26, \subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	lda	$16, 0($31)
	lda	$0, 1($31)
	callsys
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	.frame	$31, 0, $31, 0
	.prologue
	ldgp	$gp, 0($27)
	.endm

	comment "Declare a data variable"
	.purgem gdbasm_datavar
	.macro gdbasm_datavar name value
	.data
\name:
	.long	\value
	.endm
