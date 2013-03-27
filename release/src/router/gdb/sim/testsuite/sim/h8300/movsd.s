# Hitachi H8 testcase 'movsd'
# mach(): all
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"

	.data
src:	.byte	'h', 'e', 'l', 'l', 'o', 0
dst1:	.byte	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
dst2:	.byte	0, 0, 0, 0, 0, 0, 0, 0, 0, 0

	start
.if (sim_cpu == h8sx)	
movsd_n:#
	# In this test, the transfer will stop after n bytes.
	#
	set_grs_a5a5

	mov	#src,  er5
	mov	#dst1, er6
	mov	#4, r4
	set_ccr_zero
	;; movsd.b disp:16
	movsd.b	fail1:16
;;; 	.word	0x7b84
;;; 	.word	0x02
	
	bra	pass1
fail1:	fail
pass1:	test_cc_clear
	test_gr_a5a5 0
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_h_gr32  0xa5a50000 er4
	test_h_gr32  src+4  er5
	test_h_gr32  dst1+4 er6
	test_gr_a5a5 7

	#
	# Now make sure exactly 4 bytes were transferred.
	cmp.b	@src, @dst1
	bne	fail1:16
	cmp.b	@src+1, @dst1+1
	bne	fail1:16
	cmp.b	@src+2, @dst1+2
	bne	fail1:16
	cmp.b	@src+3, @dst1+3
	bne	fail1:16
	cmp.b	@src+4, @dst1+4
	beq	fail1:16

movsd_s:#
	# In this test, the entire null-terminated string is transferred.
	#
	set_grs_a5a5

	mov	#src,  er5
	mov	#dst2, er6
	mov	#8, r4
	set_ccr_zero
	;; movsd.b disp:16
	movsd.b	pass2:16
;;; 	.word	0x7b84
;;; 	.word	0x10

fail2:	fail
pass2:	test_cc_clear
	test_gr_a5a5 0
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_h_gr32  0xa5a50002 er4
	test_h_gr32  src+6  er5
	test_h_gr32  dst2+6 er6
	test_gr_a5a5 7
	#
	# Now make sure 5 bytes were transferred, and the 6th is zero.
	cmp.b	@src, @dst2
	bne	fail2:16
	cmp.b	@src+1, @dst2+1
	bne	fail2:16
	cmp.b	@src+2, @dst2+2
	bne	fail2:16
	cmp.b	@src+3, @dst2+3
	bne	fail2:16
	cmp.b	@src+4, @dst2+4
	bne	fail2:16
	cmp.b	#0,     @dst2+5
	bne	fail2:16
.endif	
	pass

	exit 0
