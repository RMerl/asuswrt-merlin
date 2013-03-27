# sh testcase for sett, clrt, movt
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start
sett_1:	set_grs_a5a5
	sett
	bt .Lsett
	nop
	fail
.Lsett:
	test_grs_a5a5

clrt_1:	set_grs_a5a5
	clrt
	bf .Lclrt
	nop
	fail
.Lclrt:
	test_grs_a5a5

movt_1:	set_grs_a5a5
	sett
	movt	r1
	test_gr_a5a5 r0
	assertreg 1, r1
	test_gr_a5a5 r2
	test_gr_a5a5 r3
	test_gr_a5a5 r4
	test_gr_a5a5 r5
	test_gr_a5a5 r6
	test_gr_a5a5 r7
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

movt_2:	set_grs_a5a5
	clrt
	movt	r1
	test_gr_a5a5 r0
	assertreg 0, r1
	test_gr_a5a5 r2
	test_gr_a5a5 r3
	test_gr_a5a5 r4
	test_gr_a5a5 r5
	test_gr_a5a5 r6
	test_gr_a5a5 r7
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

	pass

	exit 0
