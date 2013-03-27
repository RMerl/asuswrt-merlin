# sh testcase for all mov.[bwl] instructions
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	.align 2
_lsrc:	.long	0x55555555
_wsrc:	.long	0x55550000
_bsrc:	.long	0x55000000

	.align 2
_ldst:	.long	0
_wdst:	.long	0
_bdst:	.long	0


	start

movb_disp12_reg:	# Test 8-bit @(disp12,gr) -> gr
	set_grs_a5a5
	mov.l	bsrc, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	mov.b	@(444,r1), r2

	assertreg _bsrc-444, r1
	assertreg 0x55, r2

movb_reg_disp12:	# Test 8-bit gr -> @(disp12,gr)
	set_grs_a5a5
	mov.l	bdst, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	mov.b	r2, @(444,r1)

	assertreg _bdst-444, r1
	assertmem _bdst, 0xa5000000

movw_disp12_reg:	# Test 16-bit @(disp12,gr) -> gr
	set_grs_a5a5
	mov.l	wsrc, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	mov.w	@(444,r1), r2

	assertreg _wsrc-444, r1
	assertreg 0x5555, r2

movw_reg_disp12:	# Test 16-bit gr -> @(disp12,gr)
	set_grs_a5a5
	mov.l	wdst, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	mov.w	r2, @(444,r1)

	assertreg _wdst-444, r1
	assertmem _wdst, 0xa5a50000

movl_disp12_reg:	# Test 32-bit @(disp12,gr) -> gr
	set_grs_a5a5
	mov.l	lsrc, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	mov.l	@(444,r1), r2

	assertreg _lsrc-444, r1
	assertreg 0x55555555, r2

movl_reg_disp12:	# Test 32-bit gr -> @(disp12,gr)
	set_grs_a5a5
	mov.l	ldst, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	add	#-111, r1
	mov.l	r2, @(444,r1)

	assertreg _ldst-444, r1
	assertmem _ldst, 0xa5a5a5a5

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

lsrc:	.long _lsrc
wsrc:	.long _wsrc
bsrc:	.long _bsrc

ldst:	.long _ldst
wdst:	.long _wdst
bdst:	.long _bdst

