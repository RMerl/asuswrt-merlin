# Hitachi H8 testcase 'exts.w, extu.w'
# mach(): h8300h h8300s h8sx
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf	

	.include "testutils.inc"

	start

	.data
	.align 2
pos:	.word	0xff01
neg:	.word	0x0080

	.text

exts_w_reg16_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w rn16
	mov.b	#1, r0l
	exts.w	r0

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  0xa5a50001 er0	; result of sign extend
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

exts_w_reg16_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w rn16
	mov.b	#0xff, r0l
	exts.w	r0

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32  0xa5a5ffff er0	; result of sign extend
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

extu_w_reg16_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w rn16
	mov.b	#0xff, r0l
	extu.w	r0

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  0xa5a500ff er0	; result of zero extend
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
exts_w_ind_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w @ern
	mov.l	#pos, er1
	exts.w	@er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0001, @pos
	beq	.Lswindp
	fail
.Lswindp:
	mov.w	#0xff01, @pos	; Restore initial value

exts_w_ind_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w @ern
	mov.l	#neg, er1
	exts.w	@er1

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32  neg er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0xff80, @neg
	beq	.Lswindn
	fail
.Lswindn:
	;; Note: leave the value as 0xff80, so that extu has work to do.
	
extu_w_ind_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w @ern
	mov.l	#neg, er1
	extu.w	@er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0080, @neg
	beq	.Luwindn
	fail
.Luwindn:
	;; Note: leave the value as 0x0080, like it started out.

exts_w_postinc_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w @ern+
	mov.l	#pos, er1
	exts.w	@er1+

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos+2 er1	; er1 still contains target address plus 2
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0001, @pos
	beq	.Lswpostincp
	fail
.Lswpostincp:
	mov.w	#0xff01, @pos	; Restore initial value

exts_w_postinc_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w @ern+
	mov.l	#neg, er1
	exts.w	@er1+

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32  neg+2 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0xff80, @neg
	beq	.Lswpostincn
	fail
.Lswpostincn:
	;; Note: leave the value as 0xff80, so that extu has work to do.
	
extu_w_postinc_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w @ern+
	mov.l	#neg, er1
	extu.w	@er1+

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg+2 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0080, @neg
	beq	.Luwpostincn
	fail
.Luwpostincn:
	;; Note: leave the value as 0x0080, like it started out.

exts_w_postdec_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w @ern-
	mov.l	#pos, er1
	exts.w	@er1-

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos-2 er1	; er1 still contains target address plus 2
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0001, @pos
	beq	.Lswpostdecp
	fail
.Lswpostdecp:
	mov.w	#0xff01, @pos	; Restore initial value

exts_w_postdec_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w @ern-
	mov.l	#neg, er1
	exts.w	@er1-

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32  neg-2 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0xff80, @neg
	beq	.Lswpostdecn
	fail
.Lswpostdecn:
	;; Note: leave the value as 0xff80, so that extu has work to do.
	
extu_w_postdec_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w @ern-
	mov.l	#neg, er1
	extu.w	@er1-

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg-2 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0080, @neg
	beq	.Luwpostdecn
	fail
.Luwpostdecn:
	;; Note: leave the value as 0x0080, like it started out.

exts_w_preinc_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w @+ern
	mov.l	#pos-2, er1
	exts.w	@+er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos er1	; er1 still contains target address plus 2
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0001, @pos
	beq	.Lswpreincp
	fail
.Lswpreincp:
	mov.w	#0xff01, @pos	; Restore initial value

exts_w_preinc_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w @+ern
	mov.l	#neg-2, er1
	exts.w	@+er1

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32  neg er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0xff80, @neg
	beq	.Lswpreincn
	fail
.Lswpreincn:
	;; Note: leave the value as 0xff80, so that extu has work to do.
	
extu_w_preinc_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w @+ern
	mov.l	#neg-2, er1
	extu.w	@+er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0080, @neg
	beq	.Luwpreincn
	fail
.Luwpreincn:
	;; Note: leave the value as 0x0080, like it started out.

exts_w_predec_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w @-ern
	mov.l	#pos+2, er1
	exts.w	@-er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos er1	; er1 still contains target address plus 2
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0001, @pos
	beq	.Lswpredecp
	fail
.Lswpredecp:
	mov.w	#0xff01, @pos	; Restore initial value

exts_w_predec_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.w @-ern
	mov.l	#neg+2, er1
	exts.w	@-er1

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32  neg er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0xff80, @neg
	beq	.Lswpredecn
	fail
.Lswpredecn:
	;; Note: leave the value as 0xff80, so that extu has work to do.
	
extu_w_predec_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w @-ern
	mov.l	#neg+2, er1
	extu.w	@-er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0080, @neg
	beq	.Luwpredecn
	fail
.Luwpredecn:
	;; Note: leave the value as 0x0080, like it started out.

extu_w_disp2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w @(dd:2, ern)
	mov.l	#neg-2, er1
	extu.w	@(2:2, er1)

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg-2 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0080, @neg
	beq	.Luwdisp2n
	fail
.Luwdisp2n:
	;; Note: leave the value as 0x0080, like it started out.

extu_w_disp16_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w @(dd:16, ern)
	mov.l	#neg-44, er1
	extu.w	@(44:16, er1)

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg-44 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0080, @neg
	beq	.Luwdisp16n
	fail
.Luwdisp16n:
	;; Note: leave the value as 0x0080, like it started out.

extu_w_disp32_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w @(dd:32, ern)
	mov.l	#neg+444, er1
	extu.w	@(-444:32, er1)

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg+444 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0080, @neg
	beq	.Luwdisp32n
	fail
.Luwdisp32n:
	;; Note: leave the value as 0x0080, like it started out.

extu_w_abs16_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w @aa:16
	extu.w	@neg:16

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0080, @neg
	beq	.Luwabs16n
	fail
.Luwabs16n:
	;; Note: leave the value as 0x0080, like it started out.

extu_w_abs32_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.w @aa:32
	extu.w	@neg:32

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.w	#0x0080, @neg
	beq	.Luwabs32n
	fail
.Luwabs32n:
	;; Note: leave the value as 0x0080, like it started out.

.endif

	pass

	exit 0
