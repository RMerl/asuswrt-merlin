.include "t-macros.i"

	start

	PSW_BITS = 0
	point_dmap_at_imem
	check_interrupt (VEC_RIE&DMAP_MASK)+DMAP_BASE PSW_BITS test_rie_xx
	
test_rie_xx:
        .short 0xe120, 0x0000  ;; Example of RIE code
	nop
	exit47
