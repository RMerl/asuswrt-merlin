	comment "subroutine declare"
	.purgem gdbasm_declare
	.macro gdbasm_declare name
	.align	2
	.ent	\name
	.type	\name,@function
\name:
	.endm

	comment "subroutine prologue"
	.macro gdbasm_enter
	.frame	$fp, 32, $31
	.mask	0xd0000000,-4
	.set	noreorder
	.cpload	$25
	.set	reorder
	subu	$sp, $sp, 32
	.cprestore 16
	sw	$31, 28($sp)
	sw	$fp, 24($sp)
	sw	$28, 20($sp)
	move	$fp, $sp
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	lw	$31, 28($sp)
	lw	$fp, 24($sp)
	.set	noreorder
	.set	nomacro
	j	$31
	addu	$sp, $sp, 32
	.set	macro
	.set	reorder
	.endm

	comment "subroutine end"
	.purgem gdbasm_end
	.macro gdbasm_end name
	.end	\name
	.endm

	.macro gdbasm_call subr
	la	$25, \subr
	jal	$31, $25
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
	lw 	$2, 0($0)
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	move	$fp, $sp
	.endm
