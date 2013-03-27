# sh testcase for pdmsb
# mach:	 shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp

	.include "testutils.inc"

	start
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_sreg 0x0, x0
L0:	pdmsb	x0, x1
#	assert_sreg 31<<16, x1
	set_sreg 0x1, x0
L1:	pdmsb	x0, x1
	assert_sreg 30<<16, x1
	set_sreg 0x3, x0
L2:	pdmsb	x0, x1
	assert_sreg 29<<16, x1
	set_sreg 0x7, x0
L3:	pdmsb	x0, x1
	assert_sreg 28<<16, x1
	set_sreg 0xf, x0
L4:	pdmsb	x0, x1
	assert_sreg 27<<16, x1
	set_sreg 0x1f, x0
L5:	pdmsb	x0, x1
	assert_sreg 26<<16, x1
	set_sreg 0x3f, x0
L6:	pdmsb	x0, x1
	assert_sreg 25<<16, x1
	set_sreg 0x7f, x0
L7:	pdmsb	x0, x1
	assert_sreg 24<<16, x1
	set_sreg 0xff, x0
L8:	pdmsb	x0, x1
	assert_sreg 23<<16, x1
	
	set_sreg 0x1ff, x0
L9:	pdmsb	x0, x1
	assert_sreg 22<<16, x1
	set_sreg 0x3ff, x0
L10:	pdmsb	x0, x1
	assert_sreg 21<<16, x1
	set_sreg 0x7ff, x0
L11:	pdmsb	x0, x1
	assert_sreg 20<<16, x1
	set_sreg 0xfff, x0
L12:	pdmsb	x0, x1
	assert_sreg 19<<16, x1
	set_sreg 0x1fff, x0
L13:	pdmsb	x0, x1
	assert_sreg 18<<16, x1
	set_sreg 0x3fff, x0
L14:	pdmsb	x0, x1
	assert_sreg 17<<16, x1
	set_sreg 0x7fff, x0
L15:	pdmsb	x0, x1
	assert_sreg 16<<16, x1
	set_sreg 0xffff, x0
L16:	pdmsb	x0, x1
	assert_sreg 15<<16, x1

	set_sreg 0x1ffff, x0
L17:	pdmsb	x0, x1
	assert_sreg 14<<16, x1
	set_sreg 0x3ffff, x0
L18:	pdmsb	x0, x1
	assert_sreg 13<<16, x1
	set_sreg 0x7ffff, x0
L19:	pdmsb	x0, x1
	assert_sreg 12<<16, x1
	set_sreg 0xfffff, x0
L20:	pdmsb	x0, x1
	assert_sreg 11<<16, x1
	set_sreg 0x1fffff, x0
L21:	pdmsb	x0, x1
	assert_sreg 10<<16, x1
	set_sreg 0x3fffff, x0
L22:	pdmsb	x0, x1
	assert_sreg 9<<16, x1
	set_sreg 0x7fffff, x0
L23:	pdmsb	x0, x1
	assert_sreg 8<<16, x1
	set_sreg 0xffffff, x0
L24:	pdmsb	x0, x1
	assert_sreg 7<<16, x1

	set_sreg 0x1ffffff, x0
L25:	pdmsb	x0, x1
	assert_sreg 6<<16, x1
	set_sreg 0x3ffffff, x0
L26:	pdmsb	x0, x1
	assert_sreg 5<<16, x1
	set_sreg 0x7ffffff, x0
L27:	pdmsb	x0, x1
	assert_sreg 4<<16, x1
	set_sreg 0xfffffff, x0
L28:	pdmsb	x0, x1
	assert_sreg 3<<16, x1
	set_sreg 0x1fffffff, x0
L29:	pdmsb	x0, x1
	assert_sreg 2<<16, x1
	set_sreg 0x3fffffff, x0
L30:	pdmsb	x0, x1
	assert_sreg 1<<16, x1
	set_sreg 0x7fffffff, x0
L31:	pdmsb	x0, x1
	assert_sreg 0<<16, x1
	set_sreg 0xffffffff, x0
L32:	pdmsb	x0, x1
#	assert_sreg 31<<16, x1

	set_sreg 0xfffffffe, x0
L33:	pdmsb	x0, x1
	assert_sreg 30<<16, x1
	set_sreg 0xfffffffc, x0
L34:	pdmsb	x0, x1
	assert_sreg 29<<16, x1
	set_sreg 0xfffffff8, x0
L35:	pdmsb	x0, x1
	assert_sreg 28<<16, x1
	set_sreg 0xfffffff0, x0
L36:	pdmsb	x0, x1
	assert_sreg 27<<16, x1
	set_sreg 0xffffffe0, x0
L37:	pdmsb	x0, x1
	assert_sreg 26<<16, x1
	set_sreg 0xffffffc0, x0
L38:	pdmsb	x0, x1
	assert_sreg 25<<16, x1
	set_sreg 0xffffff80, x0
L39:	pdmsb	x0, x1
	assert_sreg 24<<16, x1
	set_sreg 0xffffff00, x0
L40:	pdmsb	x0, x1
	assert_sreg 23<<16, x1

	set_sreg 0xfffffe00, x0
L41:	pdmsb	x0, x1
	assert_sreg 22<<16, x1
	set_sreg 0xfffffc00, x0
L42:	pdmsb	x0, x1
	assert_sreg 21<<16, x1
	set_sreg 0xfffff800, x0
L43:	pdmsb	x0, x1
	assert_sreg 20<<16, x1
	set_sreg 0xfffff000, x0
L44:	pdmsb	x0, x1
	assert_sreg 19<<16, x1
	set_sreg 0xffffe000, x0
L45:	pdmsb	x0, x1
	assert_sreg 18<<16, x1
	set_sreg 0xffffc000, x0
L46:	pdmsb	x0, x1
	assert_sreg 17<<16, x1
	set_sreg 0xffff8000, x0
L47:	pdmsb	x0, x1
	assert_sreg 16<<16, x1
	set_sreg 0xffff0000, x0
L48:	pdmsb	x0, x1
	assert_sreg 15<<16, x1

	set_sreg 0xfffe0000, x0
L49:	pdmsb	x0, x1
	assert_sreg 14<<16, x1
	set_sreg 0xfffc0000, x0
L50:	pdmsb	x0, x1
	assert_sreg 13<<16, x1
	set_sreg 0xfff80000, x0
L51:	pdmsb	x0, x1
	assert_sreg 12<<16, x1
	set_sreg 0xfff00000, x0
L52:	pdmsb	x0, x1
	assert_sreg 11<<16, x1
	set_sreg 0xffe00000, x0
L53:	pdmsb	x0, x1
	assert_sreg 10<<16, x1
	set_sreg 0xffc00000, x0
L54:	pdmsb	x0, x1
	assert_sreg 9<<16, x1
	set_sreg 0xff800000, x0
L55:	pdmsb	x0, x1
	assert_sreg 8<<16, x1
	set_sreg 0xff000000, x0
L56:	pdmsb	x0, x1
	assert_sreg 7<<16, x1

	set_sreg 0xfe000000, x0
L57:	pdmsb	x0, x1
	assert_sreg 6<<16, x1
	set_sreg 0xfc000000, x0
L58:	pdmsb	x0, x1
	assert_sreg 5<<16, x1
	set_sreg 0xf8000000, x0
L59:	pdmsb	x0, x1
	assert_sreg 4<<16, x1
	set_sreg 0xf0000000, x0
L60:	pdmsb	x0, x1
	assert_sreg 3<<16, x1
	set_sreg 0xe0000000, x0
L61:	pdmsb	x0, x1
	assert_sreg 2<<16, x1
	set_sreg 0xc0000000, x0
L62:	pdmsb	x0, x1
	assert_sreg 1<<16, x1
	set_sreg 0x80000000, x0
L63:	pdmsb	x0, x1
	assert_sreg 0<<16, x1
	set_sreg 0x00000000, x0
L64:	pdmsb	x0, x1
#	assert_sreg 31<<16, x1

	test_grs_a5a5
	assert_sreg	0xa5a5a5a5, y0
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

	pass
	exit 0
