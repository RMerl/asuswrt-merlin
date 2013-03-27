.include "t-macros.i"

	start

	PSW_BITS = 0
	point_dmap_at_imem
	check_interrupt (VEC_AE&DMAP_MASK)+DMAP_BASE PSW_BITS test_st2w
	
	ldi sp, #0x4004
	st2w r8, @-SP

	ldi sp, #0x4005
test_st2w:
	st2w r8,@-SP
	nop
	exit47
