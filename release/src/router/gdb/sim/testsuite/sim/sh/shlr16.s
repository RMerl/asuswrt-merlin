# sh testcase for shlr16 
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

shrl16:
	set_grs_a5a5
	shlr16 r0
	assertreg0 0xa5a5
	shlr16 r0
	assertreg0 0

	set_greg 0xa5a5a5a5, r0
	test_grs_a5a5
	pass
	exit 0
