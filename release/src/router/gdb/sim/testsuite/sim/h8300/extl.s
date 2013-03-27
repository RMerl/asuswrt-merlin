# Hitachi H8 testcase 'exts.l, extu.l'
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
	.align 4
pos:	.long	0xffff0001
neg:	.long	0x00008000

pos2:	.long	0xffffff01
neg2:	.long	0x00000080

	.text

exts_l_reg32_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l ern32
	mov.w	#1, r0
	exts.l	er0

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  0x00000001 er0	; result of sign extend
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

exts_l_reg32_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l ern32
	mov.w	#0xffff, r0
	exts.l	er0

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32  0xffffffff er0	; result of sign extend
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

extu_l_reg32_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l ern32
	mov.w	#0xffff, r0
	extu.l	er0

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  0x0000ffff er0	; result of zero extend
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
exts_l_ind_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l @ern32
	mov.l	#pos, er1
	exts.l	@er1

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
	cmp.l	#0x00000001, @pos
	beq	.Lslindp
	fail
.Lslindp:
	mov.l	#0xffff0001, @pos	; Restore initial value

exts_l_ind_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l @ern32
	mov.l	#neg, er1
	exts.l	@er1

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
        cmp.l   #0xffff8000, @neg
	beq     .Lslindn
	fail
.Lslindn:
;;;  Note:	 leave the value as 0xffff8000, so that extu has work to do.
	
extu_l_ind_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l @ern32
	mov.l	#neg, er1
	extu.l	@er1

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
        cmp.l   #0x00008000, @neg
	beq     .Lulindn
	fail
.Lulindn:
;;;  Note:	 leave the value as 0x00008000, so that extu has work to do.

exts_l_postinc_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l @ern32+
	mov.l	#pos, er1
	exts.l	@er1+

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos+4 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.l	#0x00000001, @pos
	beq	.Lslpostincp
	fail
.Lslpostincp:
	mov.l	#0xffff0001, @pos	; Restore initial value

exts_l_postinc_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l @ern32+
	mov.l	#neg, er1
	exts.l	@er1+

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32  neg+4 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0xffff8000, @neg
	beq     .Lslpostincn
	fail
.Lslpostincn:
;;;  Note:	 leave the value as 0xffff8000, so that extu has work to do.
	
extu_l_postinc_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l @ern32+
	mov.l	#neg, er1
	extu.l	@er1+

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg+4 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00008000, @neg
	beq     .Lulpostincn
	fail
.Lulpostincn:
;;;  Note:	 leave the value as 0x00008000, so that extu has work to do.

exts_l_postdec_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l @ern32-
	mov.l	#pos, er1
	exts.l	@er1-

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos-4 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.l	#0x00000001, @pos
	beq	.Lslpostdecp
	fail
.Lslpostdecp:
	mov.l	#0xffff0001, @pos	; Restore initial value

exts_l_postdec_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l @ern32-
	mov.l	#neg, er1
	exts.l	@er1-

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32  neg-4 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0xffff8000, @neg
	beq     .Lslpostdecn
	fail
.Lslpostdecn:
;;;  Note:	 leave the value as 0xffff8000, so that extu has work to do.
	
extu_l_postdec_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l @ern32-
	mov.l	#neg, er1
	extu.l	@er1-

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg-4 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00008000, @neg
	beq     .Lulpostdecn
	fail
.Lulpostdecn:
;;;  Note:	 leave the value as 0x00008000, so that extu has work to do.

exts_l_preinc_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l @+ern32
	mov.l	#pos-4, er1
	exts.l	@+er1

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
	cmp.l	#0x00000001, @pos
	beq	.Lslpreincp
	fail
.Lslpreincp:
	mov.l	#0xffff0001, @pos	; Restore initial value

exts_l_preinc_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l @+ern32
	mov.l	#neg-4, er1
	exts.l	@+er1

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
        cmp.l   #0xffff8000, @neg
	beq     .Lslpreincn
	fail
.Lslpreincn:
;;;  Note:	 leave the value as 0xffff8000, so that extu has work to do.
	
extu_l_preinc_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l @+ern32
	mov.l	#neg-4, er1
	extu.l	@+er1

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
        cmp.l   #0x00008000, @neg
	beq     .Lulpreincn
	fail
.Lulpreincn:
;;;  Note:	 leave the value as 0x00008000, so that extu has work to do.

exts_l_predec_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l @-ern32
	mov.l	#pos+4, er1
	exts.l	@-er1

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
	cmp.l	#0x00000001, @pos
	beq	.Lslpredecp
	fail
.Lslpredecp:
	mov.l	#0xffff0001, @pos	; Restore initial value

exts_l_predec_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l @-ern32
	mov.l	#neg+4, er1
	exts.l	@-er1

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
        cmp.l   #0xffff8000, @neg
	beq     .Lslpredecn
	fail
.Lslpredecn:
;;;  Note:	 leave the value as 0xffff8000, so that extu has work to do.
	
extu_l_predec_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l @-ern32
	mov.l	#neg+4, er1
	extu.l	@-er1

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
        cmp.l   #0x00008000, @neg
	beq     .Lulpredecn
	fail
.Lulpredecn:
;;;  Note:	 leave the value as 0x00008000, so that extu has work to do.

extu_l_disp2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l @(dd:2, ern32)
	mov.l	#neg-8, er1
	extu.l	@(8:2, er1)

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg-8 er1	; er1 still contains target address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00008000, @neg
	beq     .Luldisp2n
	fail
.Luldisp2n:
;;;  Note:	 leave the value as 0x00008000, so that extu has work to do.

extu_l_disp16_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l @(dd:16, ern32)
	mov.l	#neg-44, er1
	extu.l	@(44:16, er1)

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
        cmp.l   #0x00008000, @neg
	beq     .Luldisp16n
	fail
.Luldisp16n:
;;;  Note:	 leave the value as 0x00008000, so that extu has work to do.

extu_l_disp32_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l @(dd:32, ern32)
	mov.l	#neg+444, er1
	extu.l	@(-444:32, er1)

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
        cmp.l   #0x00008000, @neg
	beq     .Luldisp32n
	fail
.Luldisp32n:
;;;  Note:	 leave the value as 0x00008000, so that extu has work to do.

extu_l_abs16_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l @aa:16
	extu.l	@neg:16

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
        cmp.l   #0x00008000, @neg
	beq     .Lulabs16n
	fail
.Lulabs16n:
;;;  Note:	 leave the value as 0x00008000, so that extu has work to do.

extu_l_abs32_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l @aa:32
	extu.l	@neg:32

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
        cmp.l   #0x00008000, @neg
	beq     .Lulabs32n
	fail
.Lulabs32n:
;;;  Note:	 leave the value as 0x00008000, so that extu has work to do.



	#
	# exts #2, nn
	#

exts_l_reg32_2_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, ern32
	mov.b	#1, r0l
	exts.l	#2, er0

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  0x00000001 er0	; result of sign extend
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

exts_l_reg32_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, ern32
	mov.b	#0xff, r0l
	exts.l	#2, er0

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_ovf_clear
	test_zero_clear
	test_carry_clear

	test_h_gr32  0xffffffff er0	; result of sign extend
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

extu_l_reg32_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, ern32
	mov.b	#0xff, r0l
	extu.l	#2, er0

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  0x000000ff er0	; result of zero extend
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

exts_l_ind_2_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, @ern32
	mov.l	#pos2, er1
	exts.l	#2, @er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos2 er1	; result of sign extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.l	#0x00000001, @pos2
	beq	.Lslindp2
	fail
.Lslindp2:
	mov.l	#0xffffff01, @pos2	; Restore initial value

exts_l_ind_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, @ern32
	mov.l	#neg2, er1
	exts.l	#2, @er1

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_ovf_clear
	test_zero_clear
	test_carry_clear

	test_h_gr32  neg2 er1	; result of sign extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0xffffff80, @neg2
	beq     .Lslindn2
	fail
.Lslindn2:
;;;  Note:	 leave the value as 0xffffff80, so that extu has work to do.

extu_l_ind_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, @ern32
	mov.l	#neg2, er1
	extu.l	#2, @er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg2 er1	; result of zero extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00000080, @neg2
	beq     .Lulindn2
	fail
.Lulindn2:
;;;  Note:	 leave the value as 0x00000080, like it started out.

exts_l_postinc_2_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, @ern32+
	mov.l	#pos2, er1
	exts.l	#2, @er1+

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos2+4 er1	; result of sign extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.l	#0x00000001, @pos2
	beq	.Lslpostincp2
	fail
.Lslpostincp2:
	mov.l	#0xffffff01, @pos2	; Restore initial value

exts_l_postinc_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, @ern32+
	mov.l	#neg2, er1
	exts.l	#2, @er1+

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_ovf_clear
	test_zero_clear
	test_carry_clear

	test_h_gr32  neg2+4 er1	; result of sign extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0xffffff80, @neg2
	beq     .Lslpostincn2
	fail
.Lslpostincn2:
;;;  Note:	 leave the value as 0xffffff80, so that extu has work to do.

extu_l_postinc_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, @ern32+
	mov.l	#neg2, er1
	extu.l	#2, @er1+

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg2+4 er1	; result of zero extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00000080, @neg2
	beq     .Lulpostincn2
	fail
.Lulpostincn2:
;;;  Note:	 leave the value as 0x00000080, like it started out.

exts_l_postdec_2_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, @ern32-
	mov.l	#pos2, er1
	exts.l	#2, @er1-

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos2-4 er1	; result of sign extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.l	#0x00000001, @pos2
	beq	.Lslpostdecp2
	fail
.Lslpostdecp2:
	mov.l	#0xffffff01, @pos2	; Restore initial value

exts_l_postdec_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, @ern32-
	mov.l	#neg2, er1
	exts.l	#2, @er1-

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_ovf_clear
	test_zero_clear
	test_carry_clear

	test_h_gr32  neg2-4 er1	; result of sign extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0xffffff80, @neg2
	beq     .Lslpostdecn2
	fail
.Lslpostdecn2:
;;;  Note:	 leave the value as 0xffffff80, so that extu has work to do.

extu_l_postdec_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, @ern32-
	mov.l	#neg2, er1
	extu.l	#2, @er1-

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg2-4 er1	; result of zero extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00000080, @neg2
	beq     .Lulpostdecn2
	fail
.Lulpostdecn2:
;;;  Note:	 leave the value as 0x00000080, like it started out.

exts_l_preinc_2_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, @+ern32
	mov.l	#pos2-4, er1
	exts.l	#2, @+er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos2 er1	; result of sign extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.l	#0x00000001, @pos2
	beq	.Lslpreincp2
	fail
.Lslpreincp2:
	mov.l	#0xffffff01, @pos2	; Restore initial value

exts_l_preinc_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, @+ern32
	mov.l	#neg2-4, er1
	exts.l	#2, @+er1

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_ovf_clear
	test_zero_clear
	test_carry_clear

	test_h_gr32  neg2 er1	; result of sign extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0xffffff80, @neg2
	beq     .Lslpreincn2
	fail
.Lslpreincn2:
;;;  Note:	 leave the value as 0xffffff80, so that extu has work to do.

extu_l_preinc_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, @+ern32
	mov.l	#neg2-4, er1
	extu.l	#2, @+er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg2 er1	; result of zero extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00000080, @neg2
	beq     .Lulpreincn2
	fail
.Lulpreincn2:
;;;  Note:	 leave the value as 0x00000080, like it started out.

exts_l_predec_2_p:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, @-ern32
	mov.l	#pos2+4, er1
	exts.l	#2, @-er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  pos2 er1	; result of sign extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	cmp.l	#0x00000001, @pos2
	beq	.Lslpredecp2
	fail
.Lslpredecp2:
	mov.l	#0xffffff01, @pos2	; Restore initial value

exts_l_predec_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; exts.l #2, @-ern32
	mov.l	#neg2+4, er1
	exts.l	#2, @-er1

	;; Test ccr		H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_ovf_clear
	test_zero_clear
	test_carry_clear

	test_h_gr32  neg2 er1	; result of sign extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0xffffff80, @neg2
	beq     .Lslpredecn2
	fail
.Lslpredecn2:
;;;  Note:	 leave the value as 0xffffff80, so that extu has work to do.

extu_l_predec_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, @-ern32
	mov.l	#neg2+4, er1
	extu.l	#2, @-er1

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg2 er1	; result of zero extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00000080, @neg2
	beq     .Lulpredecn2
	fail
.Lulpredecn2:
;;;  Note:	 leave the value as 0x00000080, like it started out.

extu_l_disp2_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, @(dd:2, ern32)
	mov.l	#neg2-8, er1
	extu.l	#2, @(8:2, er1)

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg2-8 er1	; result of zero extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00000080, @neg2
	beq     .Luldisp2n2
	fail
.Luldisp2n2:
;;;  Note:	 leave the value as 0x00000080, like it started out.

extu_l_disp16_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, @(dd:16, ern32)
	mov.l	#neg2-44, er1
	extu.l	#2, @(44:16, er1)

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg2-44 er1	; result of zero extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00000080, @neg2
	beq     .Luldisp16n2
	fail
.Luldisp16n2:
;;;  Note:	 leave the value as 0x00000080, like it started out.

extu_l_disp32_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, @(dd:32, ern32)
	mov.l	#neg2+444, er1
	extu.l	#2, @(-444:32, er1)

	;; Test ccr		H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_h_gr32  neg2+444 er1	; result of zero extend
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
        cmp.l   #0x00000080, @neg2
	beq     .Luldisp32n2
	fail
.Luldisp32n2:
;;;  Note:	 leave the value as 0x00000080, like it started out.

extu_l_abs16_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, @aa:16
	extu.l	#2, @neg2:16

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
        cmp.l   #0x00000080, @neg2
	beq     .Lulabs16n2
	fail
.Lulabs16n2:
;;;  Note:	 leave the value as 0x00000080, like it started out.

extu_l_abs32_2_n:
	set_grs_a5a5
	set_ccr_zero
	;; extu.l #2, @aa:32
	extu.l	#2, @neg2:32

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
        cmp.l   #0x00000080, @neg2
	beq     .Lulabs32n2
	fail
.Lulabs32n2:
;;;  Note:	 leave the value as 0x00000080, like it started out.

.endif

	pass

	exit 0




