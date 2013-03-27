# sh testcase for setdmx, setdmy, clrdmxy
# mach: shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start
	set_grs_a5a5
	setdmx
	test_sr_bit_set   0x400
	test_sr_bit_clear 0x800
	setdmy
	test_sr_bit_clear 0x400
	test_sr_bit_set   0x800
	clrdmxy
	test_sr_bit_clear 0x400
	test_sr_bit_clear 0x800

	test_grs_a5a5
	pass
	exit 0
