	comment "subroutine prologue"
	.macro gdbasm_enter
	alloc r33=ar.pfs,0,2,0,0
	mov r32=b0
	nop.i 0
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	nop.m 0
	mov ar.pfs=r33
	mov b0=r32
	nop.m 0
	nop.f 0
	br.ret.sptk.many b0
	.endm

	.macro gdbasm_call subr
	nop.m 0
	nop.f 0
	br.call.sptk.many b0=\subr
	.endm

	.macro gdbasm_several_nops
	nop.m 0
	nop.i 0
	nop.i 0
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	break.m 0x0
	nop.m 0
	nop.i 0
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	gdbasm_enter
	mov r32=r0
	nop.i 0
	nop.i 0
	.endm

	comment "Declare a data variable"
	.purgem gdbasm_datavar
	.macro gdbasm_datavar name value
	.data
\name:
	.long	\value
	.endm
