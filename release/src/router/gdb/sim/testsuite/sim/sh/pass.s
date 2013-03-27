# sh testcase, pass
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start
	set_grs_a5a5
	test_grs_a5a5
	pass

	exit 0

