# Hitachi H8 testcase 'movmd'
# mach(): h8sx
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"

	.data
byte_src:
	.byte	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
byte_dst:
	.byte	0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0

	.align 2
word_src:
	.word	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
word_dst:
	.word	0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0

	.align 4
long_src:
	.long	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
long_dst:
	.long	0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0

	start
.if (sim_cpu == h8sx)
movmd_b:#
	# Byte block transfer
	#
	set_grs_a5a5

	mov	#byte_src, er5
	mov	#byte_dst, er6
	mov	#10, r4
	set_ccr_zero
	;; movmd.b
	movmd.b
;;; 	.word	0x7b94

	test_cc_clear
	test_gr_a5a5 0
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_h_gr32  0xa5a50000  er4
	test_h_gr32  byte_src+10 er5
	test_h_gr32  byte_dst+10 er6
	test_gr_a5a5 7

	#
	# Now make sure exactly 10 bytes were transferred.
	memcmp	byte_src byte_dst 10
	cmp.b	#0, @byte_dst+10
	beq	.L0
	fail
.L0:

movmd_w:#
	# Word block transfer
	#
	set_grs_a5a5

	mov	#word_src, er5
	mov	#word_dst, er6
	mov	#10, r4
	set_ccr_zero
	;; movmd.w
	movmd.w
;;; 	.word	0x7ba4

	test_cc_clear
	test_gr_a5a5 0
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_h_gr32  0xa5a50000  er4
	test_h_gr32  word_src+20 er5
	test_h_gr32  word_dst+20 er6
	test_gr_a5a5 7

	#
	# Now make sure exactly 20 bytes were transferred.
	memcmp	word_src word_dst 20
	cmp.w	#0, @word_dst+20
	beq	.L1
	fail
.L1:

movmd_l:#
	# Long block transfer
	#
	set_grs_a5a5

	mov	#long_src, er5
	mov	#long_dst, er6
	mov	#10, r4
	set_ccr_zero
	;; movmd.b
	movmd.l
;;; 	.word	0x7bb4

	test_cc_clear
	test_gr_a5a5 0
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_h_gr32  0xa5a50000  er4
	test_h_gr32  long_src+40 er5
	test_h_gr32  long_dst+40 er6
	test_gr_a5a5 7

	#
	# Now make sure exactly 40 bytes were transferred.
	memcmp	long_src long_dst 40
	cmp.l	#0, @long_dst+40
	beq	.L2
	fail
.L2:

.endif	
	pass

	exit 0
