# sh testcase for bandnot, bornot
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	.align 2
_x:	.long	0xa5a5a5a5

	start

bandor_b_imm_disp12_reg:
	set_grs_a5a5
	# Make sure T is true to start.
	sett

	mov.l	x, r1

	bandnot.b	#0, @(3, r1)
	bt8k	mfail
	bornot.b	#1, @(3, r1)
	bf8k	mfail
	bandnot.b	#2, @(3, r1)
	bt8k	mfail
	bornot.b	#3, @(3, r1)
	bf8k	mfail

	bornot.b	#4, @(3, r1)
	bf8k	mfail
	bandnot.b	#5, @(3, r1)
	bt8k	mfail
	bornot.b	#6, @(3, r1)
	bf8k	mfail
	bandnot.b	#7, @(3, r1)
	bt8k	mfail

	bandnot.b	#0, @(2, r1)
	bt8k	mfail
	bornot.b	#1, @(2, r1)
	bf8k	mfail
	bandnot.b	#2, @(2, r1)
	bt8k	mfail
	bornot.b	#3, @(2, r1)
	bf8k	mfail

	bra	.L2
	nop

	.align 2
x:	.long	_x

.L2:
	bornot.b	#4, @(2, r1)
	bf8k	mfail
	bandnot.b	#5, @(2, r1)
	bt8k	mfail
	bornot.b	#6, @(2, r1)
	bf8k	mfail
	bandnot.b	#7, @(2, r1)
	bt8k	mfail

	bandnot.b	#0, @(1, r1)
	bt8k	mfail
	bornot.b	#1, @(1, r1)
	bf8k	mfail
	bandnot.b	#2, @(1, r1)
	bt8k	mfail
	bornot.b	#3, @(1, r1)
	bf8k	mfail

	bornot.b	#4, @(1, r1)
	bf8k	mfail
	bandnot.b	#5, @(1, r1)
	bt8k	mfail
	bornot.b	#6, @(1, r1)
	bf8k	mfail
	bandnot.b	#7, @(1, r1)
	bt8k	mfail

	bandnot.b	#0, @(0, r1)
	bt8k	mfail
	bornot.b	#1, @(0, r1)
	bf8k	mfail
	bandnot.b	#2, @(0, r1)
	bt8k	mfail
	bornot.b	#3, @(0, r1)
	bf8k	mfail

	bornot.b	#4, @(0, r1)
	bf8k	mfail
	bandnot.b	#5, @(0, r1)
	bt8k	mfail
	bornot.b	#6, @(0, r1)
	bf8k	mfail
	bandnot.b	#7, @(0, r1)
	bt8k	mfail

	assertreg _x, r1

	test_gr_a5a5 r0
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


