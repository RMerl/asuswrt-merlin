# Hitachi H8 testcase 'band', 'bor', 'bxor', 'bld', 'bst', 'bstz'
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
byte_src:	.byte 0xa5
byte_dst:	.byte 0

	start

band_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; band xx:3, reg8
	band	#7, r0l		; this should NOT set the carry flag.
	test_cc_clear
	band	#6, r0l		; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr		; set the carry flag
	band	#7, r0l		; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	band	#6, r0l		; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.

band_imm3_ind:
	set_grs_a5a5
.if (sim_cpu == h8300)
	mov	#byte_src, r1
	set_ccr_zero
	;; band xx:3, ind
	band	#7, @r1		; this should NOT set the carry flag.
	test_cc_clear
	band	#6, @r1		; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr		; set the carry flag
	band	#7, @r1		; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	band	#6, @r1		; this should clear the carry flag
	test_cc_clear
;;; 	test_h_gr16  byte_src r1	;FIXME
.else
	mov	#byte_src, er1
	set_ccr_zero
	;; band xx:3, ind
	band	#7, @er1	; this should NOT set the carry flag.
	test_cc_clear
	band	#6, @er1	; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr		; set the carry flag
	band	#7, @er1	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	band	#6, @er1	; this should clear the carry flag
	test_cc_clear
	test_h_gr32  byte_src er1
.endif				; h8300
	test_gr_a5a5 0		; general registers should not be changed.
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

band_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; band xx:3, aa:8
	band	#7, @0x20:8	; this should NOT set the carry flag.
	test_cc_clear
	band	#6, @0x20:8	; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr		; set the carry flag
	band	#7, @0x20:8	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	band	#6, @0x20:8	; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.

.if (sim_cpu > h8300h)
band_imm3_abs16:
	set_grs_a5a5
	set_ccr_zero
	;; band xx:3, aa:16
	band	#7, @byte_src:16	; this should NOT set the carry flag.
	test_cc_clear
	band	#6, @byte_src:16	; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr			; set the carry flag
	band	#7, @byte_src:16	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	band	#6, @byte_src:16	; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.

band_imm3_abs32:
	set_grs_a5a5
	set_ccr_zero
	;; band xx:3, aa:32
	band	#7, @byte_src:32	; this should NOT set the carry flag.
	test_cc_clear
	band	#6, @byte_src:32	; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr			; set the carry flag
	band	#7, @byte_src:32	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	band	#6, @byte_src:32	; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.
.endif

bor_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; bor xx:3, reg8
	bor	#6, r0l		; this should NOT set the carry flag.
	test_cc_clear

	bor	#7, r0l		; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	orc	#1, ccr		; set the carry flag
	bor	#7, r0l		; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	bor	#6, r0l		; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.

bor_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bor xx:3, aa:8
	bor	#6, @0x20:8	; this should NOT set the carry flag.
	test_cc_clear
	bor	#7, @0x20:8	; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	orc	#1, ccr		; set the carry flag
	bor	#7, @0x20:8	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	bor	#6, @0x20:8	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.

bxor_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; bxor xx:3, reg8
	bxor	#6, r0l		; this should NOT set the carry flag.
	test_cc_clear

	bxor	#7, r0l		; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	orc	#1, ccr		; set the carry flag
	bxor	#6, r0l		; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	bxor	#7, r0l		; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.

bxor_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bxor xx:3, aa:8
	bxor	#6, @0x20:8	; this should NOT set the carry flag.
	test_cc_clear
	bxor	#7, @0x20:8	; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	orc	#1, ccr		; set the carry flag
	bxor	#6, @0x20:8	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	bxor	#7, @0x20:8	; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.

bld_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; bld xx:3, reg8
	bld	#6, r0l		; this should NOT set the carry flag.
	test_cc_clear
	bld	#7, r0l		; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.

bld_imm3_ind:
	set_grs_a5a5
.if (sim_cpu == h8300)
	mov	#byte_src, r1
	set_ccr_zero
	;; bld xx:3, ind
	bld	#6, @r1		; this should NOT set the carry flag.
	test_cc_clear
	bld	#7, @r1		; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
;;; 	test_h_gr16  byte_src r1	;FIXME
.else
	mov	#byte_src, er1
	set_ccr_zero
	;; bld xx:3, ind
	bld	#6, @er1	; this should NOT set the carry flag.
	test_cc_clear
	bld	#7, @er1	; this should NOT set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	test_h_gr32  byte_src er1
.endif				; h8300
	test_gr_a5a5 0		; general registers should not be changed.
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

bld_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bld xx:3, aa:8
	bld	#6, @0x20:8	; this should NOT set the carry flag.
	test_cc_clear
	bld	#7, @0x20:8	; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.

.if (sim_cpu > h8300h)
bld_imm3_abs16:
	set_grs_a5a5
	set_ccr_zero
	;; bld xx:3, aa:16
	bld	#6, @byte_src:16	; this should NOT set the carry flag.
	test_cc_clear
	bld	#7, @byte_src:16	; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.

bld_imm3_abs32:
	set_grs_a5a5
	set_ccr_zero
	;; bld xx:3, aa:32
	bld	#6, @byte_src:32	; this should NOT set the carry flag.
	test_cc_clear
	bld	#7, @byte_src:32	; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.
.endif

bst_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; bst xx:3, reg8
	bst	#7, r0l		; this should clear bit 7
	test_cc_clear
	test_h_gr16 0xa525 r0

	set_ccr_zero
	orc	#1, ccr		; set the carry flag
	bst	#6, r0l		; this should set bit 6
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	test_h_gr16 0xa565 r0

	test_gr_a5a5 1		; Rest of general regs should not be changed.
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

bst_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bst xx:3, aa:8
	bst	#7, @0x20:8	; this should clear bit 7
	test_cc_clear
	mov.b	@0x20, r0l
	test_h_gr16 0xa525 r0

	set_ccr_zero
	orc	#1, ccr		; set the carry flag
	bst	#6, @0x20:8	; this should set bit 6
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	mov.b	@0x20, r0l
	test_h_gr16 0xa565 r0

	test_gr_a5a5 1		; general registers should not be changed.
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
bstz_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bstz xx:3, aa:8
	bstz	#7, @0x20:8	; this should clear bit 7
	test_cc_clear
	mov.b	@0x20, r0l
	test_h_gr16 0xa525 r0

	set_ccr_zero
	orc	#4, ccr		; set the zero flag
	bstz	#6, @0x20:8	; this should set bit 6
	test_carry_clear
	test_ovf_clear
	test_neg_clear
	test_zero_set
	mov.b	@0x20, r0l
	test_h_gr16 0xa565 r0

	test_gr_a5a5 1		; general registers should not be changed.
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif				; h8sx

btst_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; btst xx:3, reg8
	btst	#7, r0l		; this should NOT set the zero flag.
	test_cc_clear
	btst	#6, r0l		; this should set the zero flag.
	test_carry_clear
	test_ovf_clear
	test_neg_clear
	test_zero_set

	test_grs_a5a5		; general registers should not be changed.

btst_imm3_ind:
	set_grs_a5a5
.if (sim_cpu == h8300)
	mov	#byte_src, r1
	set_ccr_zero
	;; btst xx:3, ind
	btst	#7, @r1		; this should NOT set the zero flag.
	test_cc_clear
	btst	#6, @r1		; this should set the zero flag.
	test_carry_clear
	test_ovf_clear
	test_neg_clear
	test_zero_set
;;; 	test_h_gr16  byte_src r1	;FIXME
.else
	mov	#byte_src, er1
	set_ccr_zero
	;; btst xx:3, ind
	btst	#7, @er1	; this should NOT set the zero flag.
	test_cc_clear
	btst	#6, @er1	; this should NOT set the zero flag.
	test_carry_clear
	test_ovf_clear
	test_neg_clear
	test_zero_set
	test_h_gr32  byte_src er1
.endif				; h8300
	test_gr_a5a5 0		; general registers should not be changed.
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

btst_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; btst xx:3, aa:8
	btst	#7, @0x20:8	; this should NOT set the zero flag.
	test_cc_clear
	btst	#6, @0x20:8	; this should set the zero flag.
	test_carry_clear
	test_ovf_clear
	test_neg_clear
	test_zero_set

	test_grs_a5a5		; general registers should not be changed.

.if (sim_cpu > h8300h)
btst_imm3_abs16:
	set_grs_a5a5
	set_ccr_zero
	;; btst xx:3, aa:16
	btst	#7, @byte_src:16	; this should NOT set the zero flag.
	test_cc_clear
	btst	#6, @byte_src:16	; this should set the zero flag.
	test_carry_clear
	test_ovf_clear
	test_neg_clear
	test_zero_set

	test_grs_a5a5		; general registers should not be changed.

btst_imm3_abs32:
	set_grs_a5a5
	set_ccr_zero
	;; btst xx:3, aa:32
	btst	#7, @byte_src:32	; this should NOT set the zero flag.
	test_cc_clear
	btst	#6, @byte_src:32	; this should set the zero flag.
	test_carry_clear
	test_ovf_clear
	test_neg_clear
	test_zero_set

	test_grs_a5a5		; general registers should not be changed.
.endif

	pass
	exit 0
