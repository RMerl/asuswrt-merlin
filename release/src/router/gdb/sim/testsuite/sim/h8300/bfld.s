# Hitachi H8 testcase 'bfld', 'bfst'
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
byte_src:	.byte	0xa5
byte_dst:	.byte	0

	start

.if (sim_cpu == h8sx)
bfld_imm8_ind:
	set_grs_a5a5
	mov	#byte_src, er2

	;; bfld #xx:8, @ers, rd8
	set_ccr_zero
	bfld	#1, @er2, r1l
	test_cc_clear
	test_h_gr8 1 r1l

	set_ccr_zero
	bfld	#2, @er2, r1l
	test_cc_clear
	test_h_gr8 0 r1l

	set_ccr_zero
	bfld	#7, @er2, r1l
	test_cc_clear
	test_h_gr8 5 r1l

	set_ccr_zero
	bfld	#0x10, @er2, r1l
	test_cc_clear
	test_h_gr8 0 r1l

	set_ccr_zero
	bfld	#0x20, @er2, r1l
	test_cc_clear
	test_h_gr8 1 r1l

	set_ccr_zero
	bfld	#0xf0, @er2, r1l
	test_cc_clear
	test_h_gr8 0xa r1l

	test_h_gr32 0xa5a5a5a5 er0
	test_h_gr32 0xa5a5a50a er1
	test_h_gr32 byte_src   er2
	test_h_gr32 0xa5a5a5a5 er3
	test_h_gr32 0xa5a5a5a5 er4
	test_h_gr32 0xa5a5a5a5 er5
	test_h_gr32 0xa5a5a5a5 er6
	test_h_gr32 0xa5a5a5a5 er7

bfld_imm8_abs16:
	set_grs_a5a5

	;; bfld #xx:8, @aa:16, rd8
	set_ccr_zero
	bfld	#0x80, @byte_src:16, r1l
	test_cc_clear
	test_h_gr8 1 r1l

	set_ccr_zero
	bfld	#0x40, @byte_src:16, r1l
	test_cc_clear
	test_h_gr8 0 r1l

	set_ccr_zero
	bfld	#0xe0, @byte_src:16, r1l
	test_cc_clear
	test_h_gr8 0x5 r1l

	set_ccr_zero
	bfld	#0x3c, @byte_src:16, r1l
	test_cc_clear
	test_h_gr8 9 r1l

	set_ccr_zero
	bfld	#0xfe, @byte_src:16, r1l
	test_cc_clear
	test_h_gr8 0x52 r1l

	set_ccr_zero
	bfld	#0, @byte_src:16, r1l
	test_cc_clear
	test_h_gr8 0 r1l

	test_h_gr32 0xa5a5a5a5 er0
	test_h_gr32 0xa5a5a500 er1
	test_h_gr32 0xa5a5a5a5 er2
	test_h_gr32 0xa5a5a5a5 er3
	test_h_gr32 0xa5a5a5a5 er4
	test_h_gr32 0xa5a5a5a5 er5
	test_h_gr32 0xa5a5a5a5 er6
	test_h_gr32 0xa5a5a5a5 er7

bfst_imm8_ind:
	set_grs_a5a5
	mov	#byte_dst, er2

	;; bfst rd8, #xx:8, @ers
	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #1, @er2
;;; 	.word	0x7d20
;;; 	.word	0xf901

	test_cc_clear
	cmp.b	#1, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #2, @er2
;;; 	.word	0x7d20
;;; 	.word	0xf902

	test_cc_clear
	cmp.b	#2, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #7, @er2
;;; 	.word	0x7d20
;;; 	.word	0xf907

	test_cc_clear
	cmp.b	#5, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #0x10, @er2
;;; 	.word	0x7d20
;;; 	.word	0xf910

	test_cc_clear
	cmp.b	#0x10, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #0x20, @er2
;;; 	.word	0x7d20
;;; 	.word	0xf920

	test_cc_clear
	cmp.b	#0x20, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #0xf0, @er2
;;; 	.word	0x7d20
;;; 	.word	0xf9f0

	test_cc_clear
	cmp.b	#0x50, @byte_dst
	bne	fail1:16

	test_h_gr32 0xa5a5a5a5 er0
	test_h_gr32 0xa5a5a5a5 er1
	test_h_gr32 byte_dst   er2
	test_h_gr32 0xa5a5a5a5 er3
	test_h_gr32 0xa5a5a5a5 er4
	test_h_gr32 0xa5a5a5a5 er5
	test_h_gr32 0xa5a5a5a5 er6
	test_h_gr32 0xa5a5a5a5 er7

bfst_imm8_abs32:
	set_grs_a5a5

	;; bfst #xx:8, @aa:32, rd8
	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #0x80, @byte_dst:32
;;; 	.word	0x6a38
;;; 	.long	byte_dst
;;; 	.word	0xf980

	test_cc_clear
	cmp.b	#0x80, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #0x40, @byte_dst:32
;;; 	.word	0x6a38
;;; 	.long	byte_dst
;;; 	.word	0xf940

	test_cc_clear
	cmp.b	#0x40, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #0xe0, @byte_dst:32
;;; 	.word	0x6a38
;;; 	.long	byte_dst
;;; 	.word	0xf9e0

	test_cc_clear
	cmp.b	#0xa0, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #0x3c, @byte_dst:32
;;; 	.word	0x6a38
;;; 	.long	byte_dst
;;; 	.word	0xf93c

	test_cc_clear
	cmp.b	#0x14, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #0xfe, @byte_dst:32
;;; 	.word	0x6a38
;;; 	.long	byte_dst
;;; 	.word	0xf9fe

	test_cc_clear
	cmp.b	#0x4a, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #0, @byte_dst:32
;;; 	.word	0x6a38
;;; 	.long	byte_dst
;;; 	.word	0xf900

	test_cc_clear
	cmp.b	#0x0, @byte_dst
	bne	fail1:16

	mov.b	#0, @byte_dst
	set_ccr_zero
	bfst	r1l, #0x38, @byte_dst:32
;;; 	.word	0x6a38
;;; 	.long	byte_dst
;;; 	.word	0xf938

	test_cc_clear
	cmp.b	#0x28, @byte_dst
	bne	fail1:16

	;;
	;; Now let's do one in which the bits in the destination
	;; are appropriately combined with the bits in the source.
	;;

	mov.b	#0xc3, @byte_dst
	set_ccr_zero
	bfst	r1l, #0x3c, @byte_dst:32
;;; 	.word	0x6a38
;;; 	.long	byte_dst
;;; 	.word	0xf93c

	test_cc_clear
	cmp.b	#0xd7, @byte_dst
	bne	fail1:16

	test_grs_a5a5

.endif
	pass

	exit 0

fail1:	fail
	
