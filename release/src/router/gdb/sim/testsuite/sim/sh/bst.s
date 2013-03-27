# sh testcase for bst
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	.align 2
_x:	.long	0
_y:	.long	0x55555555

	start

bst_b_imm_disp12_reg:
	set_grs_a5a5
	# Make sure T is true to start.
	sett

	mov.l	x, r1

	bst.b	#0, @(3, r1)
	assertmem _x, 0x1
	bst.b	#1, @(3, r1)
	assertmem _x, 0x3
	bst.b	#2, @(3, r1)
	assertmem _x, 0x7
	bst.b	#3, @(3, r1)
	assertmem _x, 0xf

	bst.b	#4, @(3, r1)
	assertmem _x, 0x1f
	bst.b	#5, @(3, r1)
	assertmem _x, 0x3f
	bst.b	#6, @(3, r1)
	assertmem _x, 0x7f
	bst.b	#7, @(3, r1)
	assertmem _x, 0xff

	bst.b	#0, @(2, r1)
	assertmem _x, 0x1ff
	bst.b	#1, @(2, r1)
	assertmem _x, 0x3ff
	bst.b	#2, @(2, r1)
	assertmem _x, 0x7ff
	bst.b	#3, @(2, r1)
	assertmem _x, 0xfff

	bra	.L2
	nop

	.align 2
x:	.long	_x
y:	.long	_y

.L2:
	bst.b	#4, @(2, r1)
	assertmem _x, 0x1fff
	bst.b	#5, @(2, r1)
	assertmem _x, 0x3fff
	bst.b	#6, @(2, r1)
	assertmem _x, 0x7fff
	bst.b	#7, @(2, r1)
	assertmem _x, 0xffff

	bst.b	#0, @(1, r1)
	assertmem _x, 0x1ffff
	bst.b	#1, @(1, r1)
	assertmem _x, 0x3ffff
	bst.b	#2, @(1, r1)
	assertmem _x, 0x7ffff
	bst.b	#3, @(1, r1)
	assertmem _x, 0xfffff

	bst.b	#4, @(1, r1)
	assertmem _x, 0x1fffff
	bst.b	#5, @(1, r1)
	assertmem _x, 0x3fffff
	bst.b	#6, @(1, r1)
	assertmem _x, 0x7fffff
	bst.b	#7, @(1, r1)
	assertmem _x, 0xffffff

	bst.b	#0, @(0, r1)
	assertmem _x, 0x1ffffff
	bst.b	#1, @(0, r1)
	assertmem _x, 0x3ffffff
	bst.b	#2, @(0, r1)
	assertmem _x, 0x7ffffff
	bst.b	#3, @(0, r1)
	assertmem _x, 0xfffffff

	bst.b	#4, @(0, r1)
	assertmem _x, 0x1fffffff
	bst.b	#5, @(0, r1)
	assertmem _x, 0x3fffffff
	bst.b	#6, @(0, r1)
	assertmem _x, 0x7fffffff
	bst.b	#7, @(0, r1)
	assertmem _x, 0xffffffff

	assertreg _x, r1

bst_imm_reg:
	set_greg 0, r1
	bst	#0, r1
	assertreg 0x1, r1
	bst	#1, r1
	assertreg 0x3, r1
	bst	#2, r1
	assertreg 0x7, r1
	bst	#3, r1
	assertreg 0xf, r1

	bst	#4, r1
	assertreg 0x1f, r1
	bst	#5, r1
	assertreg 0x3f, r1
	bst	#6, r1
	assertreg 0x7f, r1
	bst	#7, r1
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


