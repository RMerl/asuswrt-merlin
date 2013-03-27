# sh testcase for bset
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	.align 2
_x:	.long	0
_y:	.long	0x55555555

	start

bset_b_imm_disp12_reg:
	set_grs_a5a5
	mov.l	x, r1

	bset.b	#0, @(3, r1)
	assertmem _x, 0x1
	bset.b	#1, @(3, r1)
	assertmem _x, 0x3
	bset.b	#2, @(3, r1)
	assertmem _x, 0x7
	bset.b	#3, @(3, r1)
	assertmem _x, 0xf

	bset.b	#4, @(3, r1)
	assertmem _x, 0x1f
	bset.b	#5, @(3, r1)
	assertmem _x, 0x3f
	bset.b	#6, @(3, r1)
	assertmem _x, 0x7f
	bset.b	#7, @(3, r1)
	assertmem _x, 0xff

	bset.b	#0, @(2, r1)
	assertmem _x, 0x1ff
	bset.b	#1, @(2, r1)
	assertmem _x, 0x3ff
	bset.b	#2, @(2, r1)
	assertmem _x, 0x7ff
	bset.b	#3, @(2, r1)
	assertmem _x, 0xfff

	bra	.L2
	nop

	.align 2
x:	.long	_x
y:	.long	_y

.L2:
	bset.b	#4, @(2, r1)
	assertmem _x, 0x1fff
	bset.b	#5, @(2, r1)
	assertmem _x, 0x3fff
	bset.b	#6, @(2, r1)
	assertmem _x, 0x7fff
	bset.b	#7, @(2, r1)
	assertmem _x, 0xffff

	bset.b	#0, @(1, r1)
	assertmem _x, 0x1ffff
	bset.b	#1, @(1, r1)
	assertmem _x, 0x3ffff
	bset.b	#2, @(1, r1)
	assertmem _x, 0x7ffff
	bset.b	#3, @(1, r1)
	assertmem _x, 0xfffff

	bset.b	#4, @(1, r1)
	assertmem _x, 0x1fffff
	bset.b	#5, @(1, r1)
	assertmem _x, 0x3fffff
	bset.b	#6, @(1, r1)
	assertmem _x, 0x7fffff
	bset.b	#7, @(1, r1)
	assertmem _x, 0xffffff

	bset.b	#0, @(0, r1)
	assertmem _x, 0x1ffffff
	bset.b	#1, @(0, r1)
	assertmem _x, 0x3ffffff
	bset.b	#2, @(0, r1)
	assertmem _x, 0x7ffffff
	bset.b	#3, @(0, r1)
	assertmem _x, 0xfffffff

	bset.b	#4, @(0, r1)
	assertmem _x, 0x1fffffff
	bset.b	#5, @(0, r1)
	assertmem _x, 0x3fffffff
	bset.b	#6, @(0, r1)
	assertmem _x, 0x7fffffff
	bset.b	#7, @(0, r1)
	assertmem _x, 0xffffffff

	assertreg _x, r1

bset_imm_reg:
	set_greg 0, r1
	bset	#0, r1
	assertreg 0x1, r1
	bset	#1, r1
	assertreg 0x3, r1
	bset	#2, r1
	assertreg 0x7, r1
	bset	#3, r1
	assertreg 0xf, r1

	bset	#4, r1
	assertreg 0x1f, r1
	bset	#5, r1
	assertreg 0x3f, r1
	bset	#6, r1
	assertreg 0x7f, r1
	bset	#7, r1
	assertreg 0xff, r1

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


