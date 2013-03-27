# sh testcase for shll16 
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

shll16:
	set_grs_a5a5
	mov #0x18, r1
	shll16 r1
	assertreg 0x180000, r1
	shll16 r1
	assertreg 0, r1

	# another:
	mov #1, r1
	shll16 r1
	mov #1, r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	shll r7
	cmp/eq r1, r7
	bt   okay
	fail
okay:
	set_greg 0xa5a5a5a5, r1
	set_greg 0xa5a5a5a5, r7
	test_grs_a5a5
	pass
	exit 0
