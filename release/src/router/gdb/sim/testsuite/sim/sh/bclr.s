# sh testcase for bclr
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	.align 2
_x:	.long	0xffffffff
_y:	.long	0x55555555

	start

bclr_b_imm_disp12_reg:
	set_grs_a5a5
	mov.l	x, r1

	bclr.b	#0, @(3, r1)
	assertmem _x, 0xfffffffe
	bclr.b	#1, @(3, r1)
	assertmem _x, 0xfffffffc
	bclr.b	#2, @(3, r1)
	assertmem _x, 0xfffffff8
	bclr.b	#3, @(3, r1)
	assertmem _x, 0xfffffff0

	bclr.b	#4, @(3, r1)
	assertmem _x, 0xffffffe0
	bclr.b	#5, @(3, r1)
	assertmem _x, 0xffffffc0
	bclr.b	#6, @(3, r1)
	assertmem _x, 0xffffff80
	bclr.b	#7, @(3, r1)
	assertmem _x, 0xffffff00

	bclr.b	#0, @(2, r1)
	assertmem _x, 0xfffffe00
	bclr.b	#1, @(2, r1)
	assertmem _x, 0xfffffc00
	bclr.b	#2, @(2, r1)
	assertmem _x, 0xfffff800
	bclr.b	#3, @(2, r1)
	assertmem _x, 0xfffff000

	bra	.L2
	nop

	.align 2
x:	.long	_x
y:	.long	_y

.L2:
	bclr.b	#4, @(2, r1)
	assertmem _x, 0xffffe000
	bclr.b	#5, @(2, r1)
	assertmem _x, 0xffffc000
	bclr.b	#6, @(2, r1)
	assertmem _x, 0xffff8000
	bclr.b	#7, @(2, r1)
	assertmem _x, 0xffff0000

	bclr.b	#0, @(1, r1)
	assertmem _x, 0xfffe0000
	bclr.b	#1, @(1, r1)
	assertmem _x, 0xfffc0000
	bclr.b	#2, @(1, r1)
	assertmem _x, 0xfff80000
	bclr.b	#3, @(1, r1)
	assertmem _x, 0xfff00000

	bclr.b	#4, @(1, r1)
	assertmem _x, 0xffe00000
	bclr.b	#5, @(1, r1)
	assertmem _x, 0xffc00000
	bclr.b	#6, @(1, r1)
	assertmem _x, 0xff800000
	bclr.b	#7, @(1, r1)
	assertmem _x, 0xff000000

	bclr.b	#0, @(0, r1)
	assertmem _x, 0xfe000000
	bclr.b	#1, @(0, r1)
	assertmem _x, 0xfc000000
	bclr.b	#2, @(0, r1)
	assertmem _x, 0xf8000000
	bclr.b	#3, @(0, r1)
	assertmem _x, 0xf0000000

	bclr.b	#4, @(0, r1)
	assertmem _x, 0xe0000000
	bclr.b	#5, @(0, r1)
	assertmem _x, 0xc0000000
	bclr.b	#6, @(0, r1)
	assertmem _x, 0x80000000
	bclr.b	#7, @(0, r1)
	assertmem _x, 0x00000000

	assertreg _x, r1

bclr_imm_reg:
	set_greg 0xff, r1
	bclr	#0, r1
	assertreg 0xfe, r1
	bclr	#1, r1
	assertreg 0xfc, r1
	bclr	#2, r1
	assertreg 0xf8, r1
	bclr	#3, r1
	assertreg 0xf0, r1

	bclr	#4, r1
	assertreg 0xe0, r1
	bclr	#5, r1
	assertreg 0xc0, r1
	bclr	#6, r1
	assertreg 0x80, r1
	bclr	#7, r1
	assertreg 0x00, r1

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


