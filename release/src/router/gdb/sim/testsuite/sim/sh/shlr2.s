# sh testcase for shlr2 
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

shrl2:
	set_grs_a5a5
	shlr2 r0
	assertreg0 0x29696969
	shlr2 r0
	assertreg0 0x0a5a5a5a
	shlr2 r0
	assertreg0 0x02969696
	shlr2 r0
	assertreg0 0x00a5a5a5
	shlr2 r0
	assertreg0 0x00296969
	shlr2 r0
	assertreg0 0x000a5a5a
	shlr2 r0
	assertreg0 0x00029696
	shlr2 r0
	assertreg0 0x0000a5a5
	shlr2 r0
	assertreg0 0x00002969
	shlr2 r0
	assertreg0 0x00000a5a
	shlr2 r0
	assertreg0 0x00000296
	shlr2 r0
	assertreg0 0x000000a5
	shlr2 r0
	assertreg0 0x00000029
	shlr2 r0
	assertreg0 0x0000000a
	shlr2 r0
	assertreg0 0x00000002
	shlr2 r0
	assertreg0 0

	set_greg 0xa5a5a5a5 r0
	test_grs_a5a5
	pass
	exit 0
