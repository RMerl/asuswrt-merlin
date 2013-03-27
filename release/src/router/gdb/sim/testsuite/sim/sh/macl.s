# sh testcase for mac.l 
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start
	# force S-bit clear
	clrs

init:
	# Prime {MACL, MACH} to #1.
	mov #1, r0
	dmulu.l r0, r0

	# Set up addresses.
	mov.l	pfour00, r0	! 85
	mov.l	pfour12, r1	! 17

test:
	mac.l @r0+, @r1+

check:
	# Check result.
	assert_sreg	0, mach
	assert_sreg	85*17+1, macl

	# Ensure post-increment occurred.
	assertreg0	four00+4
	assertreg	four12+4, r1

doubleinc:
	mov.l	pfour00, r0
	mac.l	@r0+, @r0+
	assertreg0 four00+8


	pass
	exit 0

	.align 1
four00:
	.long	85
	.long	2
four12:
	.long	17
	.long	3

	.align 2
pfour00:
	.long four00
pfour12:
	.long four12
