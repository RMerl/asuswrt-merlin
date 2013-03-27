# sh testcase for bldnot
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	.align 2
_x:	.long	0xa5a5a5a5
_y:	.long	0x55555555

	start

bldnot_b_imm_disp12_reg:
	set_grs_a5a5
	mov.l	x, r1

	bldnot.b	#0, @(0, r1)
	bt8k	mfail
	bldnot.b	#1, @(0, r1)
	bf8k	mfail
	bldnot.b	#2, @(0, r1)
	bt8k	mfail
	bldnot.b	#3, @(0, r1)
	bf8k	mfail

	bldnot.b	#4, @(0, r1)
	bf8k	mfail
	bldnot.b	#5, @(0, r1)
	bt8k	mfail
	bldnot.b	#6, @(0, r1)
	bf8k	mfail
	bldnot.b	#7, @(0, r1)
	bt8k	mfail

	bldnot.b	#0, @(1, r1)
	bt8k	mfail
	bldnot.b	#1, @(1, r1)
	bf8k	mfail
	bldnot.b	#2, @(1, r1)
	bt8k	mfail
	bldnot.b	#3, @(1, r1)
	bf8k	mfail

	bldnot.b	#4, @(1, r1)
	bf8k	mfail
	bldnot.b	#5, @(1, r1)
	bt8k	mfail
	bldnot.b	#6, @(1, r1)
	bf8k	mfail
	bldnot.b	#7, @(1, r1)
	bt8k	mfail

	bldnot.b	#0, @(2, r1)
	bt8k	mfail
	bldnot.b	#1, @(2, r1)
	bf8k	mfail
	bldnot.b	#2, @(2, r1)
	bt8k	mfail
	bldnot.b	#3, @(2, r1)
	bf8k	mfail

	bldnot.b	#4, @(2, r1)
	bf8k	mfail
	bldnot.b	#5, @(2, r1)
	bt8k	mfail
	bldnot.b	#6, @(2, r1)
	bf8k	mfail
	bldnot.b	#7, @(2, r1)
	bt8k	mfail

	bldnot.b	#0, @(3, r1)
	bt8k	mfail
	bldnot.b	#1, @(3, r1)
	bf8k	mfail
	bldnot.b	#2, @(3, r1)
	bt8k	mfail
	bldnot.b	#3, @(3, r1)
	bf8k	mfail

	bldnot.b	#4, @(3, r1)
	bf8k	mfail
	bldnot.b	#5, @(3, r1)
	bt8k	mfail
	bldnot.b	#6, @(3, r1)
	bf8k	mfail
	bldnot.b	#7, @(3, r1)
	bt8k	mfail

	assertreg _x, r1
	set_greg 0xa5a5a5a5, r1

	test_grs_a5a5

	pass

	exit 0

	.align 2
x:	.long	_x
y:	.long	_y

