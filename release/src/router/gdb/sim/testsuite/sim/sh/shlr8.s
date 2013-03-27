# sh testcase for shlr8 
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

shrl8:
	set_grs_a5a5
	shlr8 r0
	assertreg0 0xa5a5a5
	shlr8 r0
	assertreg0 0xa5a5
	shlr8 r0
	assertreg0 0xa5
	shlr8 r0
	assertreg0 0x0

	set_greg 0xa5a5a5a5, r0
	test_grs_a5a5
	pass
	exit 0
