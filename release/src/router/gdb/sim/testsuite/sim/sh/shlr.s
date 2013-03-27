# sh testcase for shlr 
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

shlr:
	set_grs_a5a5
	mov #0, r0
	or #192, r0
	shlr r0
	assertreg0 96
	shlr r0
	assertreg0 48
	shlr r0
	assertreg0 24
	shlr r0
	assertreg0 12
	shlr r0
	assertreg0 6
	shlr r0
	assertreg0 3

	# Make sure a bit is shifted into T.
	shlr r0
	bf wrong
	assertreg0 1
	# Ditto.
	shlr r0
	bf wrong
	assertreg0 0

	set_greg 0xa5a5a5a5, r0
	test_grs_a5a5
	pass
	exit 0

wrong:
	fail
