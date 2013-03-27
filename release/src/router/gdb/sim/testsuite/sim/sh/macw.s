# sh testcase for mac.w 
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start
	set_grs_a5a5

	# Prime {MACL, MACH} to #1.
	mov	#1, r0
	dmulu.l	r0, r0

	# Set up addresses.
	mov.l	pfour00, r0	! 85
	mov.l	pfour12, r1	! 17

test:
	mac.w	@r0+, @r1+	! MAC = 85 * 17 + 1

check:
	# Check result.
	assert_sreg	0, mach
	assert_sreg	85*17+1, macl

	# Ensure post-increment occurred.
	assertreg0	four00+2
	assertreg	four12+2, r1

doubleinc:
	mov.l	pfour00, r0
	mac.w	@r0+, @r0+
	assertreg0 four00+4

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1

	test_grs_a5a5

	pass
	exit 0

	.align 2
four00:
	.word	85
	.word	2
four12:
	.word	17
	.word	3


pfour00:
	.long four00
pfour12:
	.long four12
