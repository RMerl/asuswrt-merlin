	comment "subroutine prologue"
	.macro gdbasm_enter
	stqd    $lr, 16($sp)
	stqd    $sp, -32($sp)
	ai      $sp, $sp, -32
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	ai	$sp, $sp, 32
	lqd     $lr, 16($sp)
	bi	$lr	
	.endm

	.macro gdbasm_call subr
	brsl	$lr, \subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	stop	0x2000
	stop 
	stop 
	stop 
	.endm

	comment "startup"
	.macro gdbasm_startup
	il	$0, 0
	ila	$sp, 16368
	stqd	$0, 0($sp)
	stqd	$sp, -32($sp)
	ai	$sp, $sp, -32
	stqd	$0, 16($sp)
	.endm
