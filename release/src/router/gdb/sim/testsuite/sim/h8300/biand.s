# Hitachi H8 testcase 'biand', 'bior', 'bixor', 'bild', 'bist', 'bistz'
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

biand_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; biand xx:3, reg8
	biand	#6, r0l		; this should NOT set the carry flag.
	test_cc_clear
	biand	#7, r0l		; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr		; set the carry flag
	biand	#6, r0l		; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	biand	#7, r0l		; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.

biand_imm3_ind:
	set_grs_a5a5
.if (sim_cpu == h8300)
	mov	#byte_src, r1
	set_ccr_zero
	;; biand xx:3, ind
	biand	#6, @r1		; this should NOT set the carry flag.
	test_cc_clear
	biand	#7, @r1		; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr		; set the carry flag
	biand	#6, @r1		; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	biand	#7, @r1		; this should clear the carry flag
	test_cc_clear
;;; 	test_h_gr16  byte_src r1	;FIXME
.else
	mov	#byte_src, er1
	set_ccr_zero
	;; biand xx:3, ind
	biand	#6, @er1	; this should NOT set the carry flag.
	test_cc_clear
	biand	#7, @er1	; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr		; set the carry flag
	biand	#6, @er1	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	biand	#7, @er1	; this should clear the carry flag
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

biand_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; biand xx:3, aa:8
	biand	#6, @0x20:8	; this should NOT set the carry flag.
	test_cc_clear
	biand	#7, @0x20:8	; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr		; set the carry flag
	biand	#6, @0x20:8	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	biand	#7, @0x20:8	; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.

.if (sim_cpu > h8300h)
biand_imm3_abs16:
	set_grs_a5a5
	set_ccr_zero
	;; biand xx:3, aa:16
	biand	#6, @byte_src:16	; this should NOT set the carry flag.
	test_cc_clear
	biand	#7, @byte_src:16	; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr			; set the carry flag
	biand	#6, @byte_src:16	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	biand	#7, @byte_src:16	; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.

biand_imm3_abs32:
	set_grs_a5a5
	set_ccr_zero
	;; biand xx:3, aa:32
	biand	#6, @byte_src:32	; this should NOT set the carry flag.
	test_cc_clear
	biand	#7, @byte_src:32	; this should NOT set the carry flag.
	test_cc_clear

	orc	#1, ccr			; set the carry flag
	biand	#6, @byte_src:32	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	biand	#7, @byte_src:32	; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.
.endif

bior_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; bior xx:3, reg8
	bior	#7, r0l		; this should NOT set the carry flag.
	test_cc_clear

	bior	#6, r0l		; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	orc	#1, ccr		; set the carry flag
	bior	#6, r0l		; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	bior	#7, r0l		; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.

bior_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bior xx:3, aa:8
	bior	#7, @0x20:8	; this should NOT set the carry flag.
	test_cc_clear
	bior	#6, @0x20:8	; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	orc	#1, ccr		; set the carry flag
	bior	#6, @0x20:8	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
	bior	#7, @0x20:8	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.

bixor_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; bixor xx:3, reg8
	bixor	#7, r0l		; this should NOT set the carry flag.
	test_cc_clear

	bixor	#6, r0l		; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	orc	#1, ccr		; set the carry flag
	bixor	#7, r0l		; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	bixor	#6, r0l		; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.

bixor_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bixor xx:3, aa:8
	bixor	#7, @0x20:8	; this should NOT set the carry flag.
	test_cc_clear
	bixor	#6, @0x20:8	; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	orc	#1, ccr		; set the carry flag
	bixor	#7, @0x20:8	; this should NOT clear the carry flag
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	bixor	#6, @0x20:8	; this should clear the carry flag
	test_cc_clear

	test_grs_a5a5		; general registers should not be changed.

bild_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; bild xx:3, reg8
	bild	#7, r0l		; this should NOT set the carry flag.
	test_cc_clear
	bild	#6, r0l		; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.

bild_imm3_ind:
	set_grs_a5a5
.if (sim_cpu == h8300)
	mov	#byte_src, r1
	set_ccr_zero
	;; bild xx:3, ind
	bild	#7, @r1		; this should NOT set the carry flag.
	test_cc_clear
	bild	#6, @r1		; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear
;;; 	test_h_gr16  byte_src r1	;FIXME
.else
	mov	#byte_src, er1
	set_ccr_zero
	;; bild xx:3, ind
	bild	#7, @er1	; this should NOT set the carry flag.
	test_cc_clear
	bild	#6, @er1	; this should NOT set the carry flag.
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

bild_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bild xx:3, aa:8
	bild	#7, @0x20:8	; this should NOT set the carry flag.
	test_cc_clear
	bild	#6, @0x20:8	; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.

.if (sim_cpu > h8300h)
bild_imm3_abs16:
	set_grs_a5a5
	set_ccr_zero
	;; bild xx:3, aa:16
	bild	#7, @byte_src:16	; this should NOT set the carry flag.
	test_cc_clear
	bild	#6, @byte_src:16	; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.

bild_imm3_abs32:
	set_grs_a5a5
	set_ccr_zero
	;; bild xx:3, aa:32
	bild	#7, @byte_src:32	; this should NOT set the carry flag.
	test_cc_clear
	bild	#6, @byte_src:32	; this should set the carry flag.
	test_carry_set
	test_ovf_clear
	test_neg_clear
	test_zero_clear

	test_grs_a5a5		; general registers should not be changed.
.endif

bist_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; bist xx:3, reg8
	bist	#6, r0l		; this should set bit 6
	test_cc_clear
	test_h_gr16 0xa5e5 r0

	set_ccr_zero
	orc	#1, ccr		; set the carry flag
	bist	#7, r0l		; this should clear bit 7
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

bist_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bist xx:3, aa:8
	bist	#6, @0x20:8	; this should set bit 6
	test_cc_clear
	mov.b	@0x20, r0l
	test_h_gr16 0xa5e5 r0

	set_ccr_zero
	orc	#1, ccr		; set the carry flag
	bist	#7, @0x20:8	; this should clear bit 7
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
bistz_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bistz xx:3, aa:8
	bistz	#6, @0x20:8	; this should set bit 6
	test_cc_clear
	mov.b	@0x20, r0l
	test_h_gr16 0xa5e5 r0

	set_ccr_zero
	orc	#4, ccr		; set the zero flag
	bistz	#7, @0x20:8	; this should clear bit 7
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

bnot_imm3_reg8:
	set_grs_a5a5
	set_ccr_zero
	;; bnot xx:3, reg8
	bnot	#7, r0l
	test_cc_clear
	test_h_gr16 0xa525 r0
	set_ccr_zero
	bnot	#6, r0l
	test_cc_clear
	test_h_gr16 0xa565 r0
	set_ccr_zero
	bnot	#5, r0l
	test_cc_clear
	test_h_gr16 0xa545 r0
	set_ccr_zero
	bnot	#4, r0l
	test_cc_clear
	test_h_gr16 0xa555 r0
	set_ccr_zero

	bnot	#4, r0l
	bnot	#5, r0l
	bnot	#6, r0l
	bnot	#7, r0l
	test_cc_clear
	test_grs_a5a5		; general registers should not be changed.

bnot_imm3_abs8:
	set_grs_a5a5
	mov.b	r1l, @0x20
	set_ccr_zero
	;; bnot xx:3, aa:8
	bnot	#7, @0x20:8
	bnot	#6, @0x20:8
	bnot	#5, @0x20:8
	bnot	#4, @0x20:8
	test_cc_clear
	test_grs_a5a5
	mov	@0x20, r0l
	test_h_gr16 0xa555 r0
	
	pass
	exit 0
