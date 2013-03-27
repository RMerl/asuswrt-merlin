# Hitachi H8 testcase 'nop'
# mach(): all
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"

	start
	
nop:	set_grs_a5a5
	set_ccr_zero

	nop

	test_cc_clear
	test_grs_a5a5
	
	
	pass

	exit 0
