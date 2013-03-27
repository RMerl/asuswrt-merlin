.include "t-macros.i"

	start

	PSW_BITS = 0
	point_dmap_at_imem
	check_interrupt (VEC_AE&DMAP_MASK)+DMAP_BASE PSW_BITS test_ld
	
	ldi r10, #0x4001
	ld r8, @(1,r10)

test_ld:
	ld r8,@(2,r10)
	nop
	exit47
