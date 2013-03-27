# Hitachi H8 testcase 'shlr'
# mach(): all
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
byte_dest:	.byte	0xa5
	.align 2
word_dest:	.word	0xa5a5
	.align 4
long_dest:	.long	0xa5a5a5a5

	.text

shlr_b_reg8_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.b	r0l		; shift right logical by one
;;;	.word	0x1108

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr16 0xa552 r0	; 1010 0101 -> 0101 0010
.if (sim_cpu)
	test_h_gr32 0xa5a5a552 er0
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shlr_b_ind_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shlr.b	@er0	; shift right logical by one, indirect
;;;	.word	0x7d00
;;;	.word	0x1100

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0101 0010
	cmp.b	#0x52, @byte_dest
	beq	.Lbind1
	fail
.Lbind1:
	mov.b	#0xa5, @byte_dest

shlr_b_postinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shlr.b	@er0+	; shift right logical by one, postinc
;;;	.word	0x0174
;;;	.word	0x6c08
;;;	.word	0x1100

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest+1 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0101 0010
	cmp.b	#0x52, @byte_dest
	beq	.Lbpostinc1
	fail
.Lbpostinc1:
	mov.b	#0xa5, @byte_dest

shlr_b_postdec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shlr.b	@er0-	; shift right logical by one, postdec
;;;	.word	0x0176
;;;	.word	0x6c08
;;;	.word	0x1100

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-1 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0101 0010
	cmp.b	#0x52, @byte_dest
	beq	.Lbpostdec1
	fail
.Lbpostdec1:
	mov.b	#0xa5, @byte_dest

shlr_b_preinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-1, er0
	shlr.b	@+er0	; shift right logical by one, preinc
;;;	.word	0x0175
;;;	.word	0x6c08
;;;	.word	0x1100

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0101 0010
	cmp.b	#0x52, @byte_dest
	beq	.Lbpreinc1
	fail
.Lbpreinc1:
	mov.b	#0xa5, @byte_dest

shlr_b_predec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest+1, er0
	shlr.b	@-er0	; shift right logical by one, predec
;;;	.word	0x0177
;;;	.word	0x6c08
;;;	.word	0x1100

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0101 0010
	cmp.b	#0x52, @byte_dest
	beq	.Lbpredec1
	fail
.Lbpredec1:
	mov.b	#0xa5, @byte_dest

shlr_b_disp2_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-2, er0
	shlr.b	@(2:2, er0)	; shift right logical by one, disp2
;;;	.word	0x0176
;;;	.word	0x6808
;;;	.word	0x1100

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0101 0010
	cmp.b	#0x52, @byte_dest
	beq	.Lbdisp21
	fail
.Lbdisp21:
	mov.b	#0xa5, @byte_dest

shlr_b_disp16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-44, er0
	shlr.b	@(44:16, er0)	; shift right logical by one, disp16
;;;	.word	0x0174
;;;	.word	0x6e08
;;;	.word	44
;;;	.word	0x1100

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0101 0010
	cmp.b	#0x52, @byte_dest
	beq	.Lbdisp161
	fail
.Lbdisp161:
	mov.b	#0xa5, @byte_dest

shlr_b_disp32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-666, er0
	shlr.b	@(666:32, er0)	; shift right logical by one, disp32
;;;	.word	0x7884
;;;	.word	0x6a28
;;; 	.long	666
;;;	.word	0x1100

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0101 0010
	cmp.b	#0x52, @byte_dest
	beq	.Lbdisp321
	fail
.Lbdisp321:
	mov.b	#0xa5, @byte_dest

shlr_b_abs16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.b	@byte_dest:16	; shift right logical by one, abs16
;;;	.word	0x6a18
;;;	.word	byte_dest
;;;	.word	0x1100

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0101 0010
	cmp.b	#0x52, @byte_dest
	beq	.Lbabs161
	fail
.Lbabs161:
	mov.b	#0xa5, @byte_dest

shlr_b_abs32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.b	@byte_dest:32	; shift right logical by one, abs32
;;;	.word	0x6a38
;;; 	.long	byte_dest
;;;	.word	0x1100

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0101 0010
	cmp.b	#0x52, @byte_dest
	beq	.Lbabs321
	fail
.Lbabs321:
	mov.b	#0xa5, @byte_dest
.endif

shlr_b_reg8_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.b	#2, r0l		; shift right logical by two
;;;	.word	0x1148

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	test_h_gr16 0xa529 r0	; 1010 0101 -> 0010 1001
.if (sim_cpu)
	test_h_gr32 0xa5a5a529 er0
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shlr_b_ind_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shlr.b	#2, @er0	; shift right logical by two, indirect
;;;	.word	0x7d00
;;;	.word	0x1140

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0010 1001
	cmp.b	#0x29, @byte_dest
	beq	.Lbind2
	fail
.Lbind2:
	mov.b	#0xa5, @byte_dest

shlr_b_postinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shlr.b	#2, @er0+	; shift right logical by two, postinc
;;;	.word	0x0174
;;;	.word	0x6c08
;;;	.word	0x1140

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest+1 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0010 1001
	cmp.b	#0x29, @byte_dest
	beq	.Lbpostinc2
	fail
.Lbpostinc2:
	mov.b	#0xa5, @byte_dest

shlr_b_postdec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shlr.b	#2, @er0-	; shift right logical by two, postdec
;;;	.word	0x0176
;;;	.word	0x6c08
;;;	.word	0x1140

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-1 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0010 1001
	cmp.b	#0x29, @byte_dest
	beq	.Lbpostdec2
	fail
.Lbpostdec2:
	mov.b	#0xa5, @byte_dest

shlr_b_preinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-1, er0
	shlr.b	#2, @+er0	; shift right logical by two, preinc
;;;	.word	0x0175
;;;	.word	0x6c08
;;;	.word	0x1140

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0010 1001
	cmp.b	#0x29, @byte_dest
	beq	.Lbpreinc2
	fail
.Lbpreinc2:
	mov.b	#0xa5, @byte_dest

shlr_b_predec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest+1, er0
	shlr.b	#2, @-er0	; shift right logical by two, predec
;;;	.word	0x0177
;;;	.word	0x6c08
;;;	.word	0x1140

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0010 1001
	cmp.b	#0x29, @byte_dest
	beq	.Lbpredec2
	fail
.Lbpredec2:
	mov.b	#0xa5, @byte_dest

shlr_b_disp2_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-2, er0
	shlr.b	#2, @(2:2, er0)	; shift right logical by two, disp2
;;;	.word	0x0176
;;;	.word	0x6808
;;;	.word	0x1140

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0010 1001
	cmp.b	#0x29, @byte_dest
	beq	.Lbdisp22
	fail
.Lbdisp22:
	mov.b	#0xa5, @byte_dest

shlr_b_disp16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-44, er0
	shlr.b	#2, @(44:16, er0)	; shift right logical by two, disp16
;;;	.word	0x0174
;;;	.word	0x6e08
;;;	.word	44
;;;	.word	0x1140

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0010 1001
	cmp.b	#0x29, @byte_dest
	beq	.Lbdisp162
	fail
.Lbdisp162:
	mov.b	#0xa5, @byte_dest

shlr_b_disp32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-666, er0
	shlr.b	#2, @(666:32, er0)	; shift right logical by two, disp32
;;;	.word	0x7884
;;;	.word	0x6a28
;;; 	.long	666
;;;	.word	0x1140

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0010 1001
	cmp.b	#0x29, @byte_dest
	beq	.Lbdisp322
	fail
.Lbdisp322:
	mov.b	#0xa5, @byte_dest

shlr_b_abs16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.b	#2, @byte_dest:16	; shift right logical by two, abs16
;;;	.word	0x6a18
;;;	.word	byte_dest
;;;	.word	0x1140

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0010 1001
	cmp.b	#0x29, @byte_dest
	beq	.Lbabs162
	fail
.Lbabs162:
	mov.b	#0xa5, @byte_dest

shlr_b_abs32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.b	#2, @byte_dest:32	; shift right logical by two, abs32
;;;	.word	0x6a38
;;; 	.long	byte_dest
;;;	.word	0x1140

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0010 1001
	cmp.b	#0x29, @byte_dest
	beq	.Lbabs322
	fail
.Lbabs322:
	mov.b	#0xa5, @byte_dest

shlr_b_reg8_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.b	#4, r0l		; shift right logical by four
;;;	.word	0x11a8

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr16 0xa50a r0	; 1010 0101 -> 0000 1010 
	test_h_gr32 0xa5a5a50a er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shlr_b_reg8_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#5, r0h
	shlr.b	r0h, r0l	; shift right logical by register value

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr16 0x0505 r0	; 1010 0101 -> 0000 0101
	test_h_gr32 0xa5a50505 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shlr_b_ind_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shlr.b	#4, @er0	; shift right logical by four, indirect
;;;	.word	0x7d00
;;;	.word	0x11a0

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0000 1010 
	cmp.b	#0x0a, @byte_dest
	beq	.Lbind4
	fail
.Lbind4:
	mov.b	#0xa5, @byte_dest

shlr_b_postinc_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shlr.b	#4, @er0+	; shift right logical by four, postinc
;;;	.word	0x0174
;;;	.word	0x6c08
;;;	.word	0x11a0

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest+1 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0000 1010 
	cmp.b	#0x0a, @byte_dest
	beq	.Lbpostinc4
	fail
.Lbpostinc4:
	mov.b	#0xa5, @byte_dest

shlr_b_postdec_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shlr.b	#4, @er0-	; shift right logical by four, postdec
;;;	.word	0x0176
;;;	.word	0x6c08
;;;	.word	0x11a0

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-1 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0000 1010 
	cmp.b	#0x0a, @byte_dest
	beq	.Lbpostdec4
	fail
.Lbpostdec4:
	mov.b	#0xa5, @byte_dest

shlr_b_preinc_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-1, er0
	shlr.b	#4, @+er0	; shift right logical by four, preinc
;;;	.word	0x0175
;;;	.word	0x6c08
;;;	.word	0x11a0

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0000 1010 
	cmp.b	#0x0a, @byte_dest
	beq	.Lbpreinc4
	fail
.Lbpreinc4:
	mov.b	#0xa5, @byte_dest

shlr_b_predec_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest+1, er0
	shlr.b	#4, @-er0	; shift right logical by four, predec
;;;	.word	0x0177
;;;	.word	0x6c08
;;;	.word	0x11a0

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0000 1010 
	cmp.b	#0x0a, @byte_dest
	beq	.Lbpredec4
	fail
.Lbpredec4:
	mov.b	#0xa5, @byte_dest

shlr_b_disp2_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-2, er0
	shlr.b	#4, @(2:2, er0)	; shift right logical by four, disp2
;;;	.word	0x0176
;;;	.word	0x6808
;;;	.word	0x11a0

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0000 1010 
	cmp.b	#0x0a, @byte_dest
	beq	.Lbdisp24
	fail
.Lbdisp24:
	mov.b	#0xa5, @byte_dest

shlr_b_disp16_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-44, er0
	shlr.b	#4, @(44:16, er0)	; shift right logical by four, disp16
;;;	.word	0x0174
;;;	.word	0x6e08
;;;	.word	44
;;;	.word	0x11a0

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0000 1010 
	cmp.b	#0x0a, @byte_dest
	beq	.Lbdisp164
	fail
.Lbdisp164:
	mov.b	#0xa5, @byte_dest

shlr_b_disp32_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-666, er0
	shlr.b	#4, @(666:32, er0)	; shift right logical by four, disp32
;;;	.word	0x7884
;;;	.word	0x6a28
;;; 	.long	666
;;;	.word	0x11a0

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 byte_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0000 1010 
	cmp.b	#0x0a, @byte_dest
	beq	.Lbdisp324
	fail
.Lbdisp324:
	mov.b	#0xa5, @byte_dest

shlr_b_abs16_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.b	#4, @byte_dest:16	; shift right logical by four, abs16
;;;	.word	0x6a18
;;;	.word	byte_dest
;;;	.word	0x11a0

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0000 1010 
	cmp.b	#0x0a, @byte_dest
	beq	.Lbabs164
	fail
.Lbabs164:
	mov.b	#0xa5, @byte_dest

shlr_b_abs32_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.b	#4, @byte_dest:32	; shift right logical by four, abs32
;;;	.word	0x6a38
;;; 	.long	byte_dest
;;;	.word	0x11a0

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0000 1010 
	cmp.b	#0x0a, @byte_dest
	beq	.Lbabs324
	fail
.Lbabs324:
	mov.b	#0xa5, @byte_dest
.endif

.if (sim_cpu == h8sx)
shlr_w_imm5_1:	
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	#15:5, r0	; shift right logical by 5-bit immediate
;;;	.word	0x038f
;;;	.word	0x1110

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	; 1010 0101 1010 0101 -> 0000 0000 0000 0001
	test_h_gr32 0xa5a50001 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

.if (sim_cpu)			; Not available in h8300 mode
shlr_w_reg16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	r0		; shift right logical by one
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	test_h_gr16 0x52d2 r0	; 1010 0101 1010 0101 -> 0101 0010 1101 0010
	test_h_gr32 0xa5a552d2 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shlr_w_ind_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	@er0	; shift right logical by one, indirect
;;;	.word	0x7d80
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0101 0010 1101 0010 
	cmp.w	#0x52d2, @word_dest
	beq	.Lwind1
	fail
.Lwind1:
	mov.w	#0xa5a5, @word_dest

shlr_w_postinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	@er0+	; shift right logical by one, postinc
;;;	.word	0x0154
;;;	.word	0x6d08
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest+2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0101 0010 1101 0010 
	cmp.w	#0x52d2, @word_dest
	beq	.Lwpostinc1
	fail
.Lwpostinc1:
	mov.w	#0xa5a5, @word_dest

shlr_w_postdec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	@er0-	; shift right logical by one, postdec
;;;	.word	0x0156
;;;	.word	0x6d08
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0101 0010 1101 0010 
	cmp.w	#0x52d2, @word_dest
	beq	.Lwpostdec1
	fail
.Lwpostdec1:
	mov.w	#0xa5a5, @word_dest

shlr_w_preinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-2, er0
	shlr.w	@+er0	; shift right logical by one, preinc
;;;	.word	0x0155
;;;	.word	0x6d08
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0101 0010 1101 0010 
	cmp.w	#0x52d2, @word_dest
	beq	.Lwpreinc1
	fail
.Lwpreinc1:
	mov.w	#0xa5a5, @word_dest

shlr_w_predec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest+2, er0
	shlr.w	@-er0	; shift right logical by one, predec
;;;	.word	0x0157
;;;	.word	0x6d08
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0101 0010 1101 0010 
	cmp.w	#0x52d2, @word_dest
	beq	.Lwpredec1
	fail
.Lwpredec1:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp2_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-4, er0
	shlr.w	@(4:2, er0)	; shift right logical by one, disp2
;;;	.word	0x0156
;;;	.word	0x6908
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0101 0010 1101 0010 
	cmp.w	#0x52d2, @word_dest
	beq	.Lwdisp21
	fail
.Lwdisp21:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-44, er0
	shlr.w	@(44:16, er0)	; shift right logical by one, disp16
;;;	.word	0x0154
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0101 0010 1101 0010 
	cmp.w	#0x52d2, @word_dest
	beq	.Lwdisp161
	fail
.Lwdisp161:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-666, er0
	shlr.w	@(666:32, er0)	; shift right logical by one, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0101 0010 1101 0010 
	cmp.w	#0x52d2, @word_dest
	beq	.Lwdisp321
	fail
.Lwdisp321:
	mov.w	#0xa5a5, @word_dest

shlr_w_abs16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	@word_dest:16	; shift right logical by one, abs16
;;;	.word	0x6b18
;;;	.word	word_dest
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0101 0010 1101 0010 
	cmp.w	#0x52d2, @word_dest
	beq	.Lwabs161
	fail
.Lwabs161:
	mov.w	#0xa5a5, @word_dest

shlr_w_abs32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	@word_dest:32	; shift right logical by one, abs32
;;;	.word	0x6b38
;;; 	.long	word_dest
;;;	.word	0x1110

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0101 0010 1101 0010 
	cmp.w	#0x52d2, @word_dest
	beq	.Lwabs321
	fail
.Lwabs321:
	mov.w	#0xa5a5, @word_dest
.endif
	
shlr_w_reg16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	#2, r0		; shift right logical by two
;;;	.word	0x1150

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr16 0x2969 r0	; 1010 0101 1010 0101 -> 0010 1001 0110 1001
	test_h_gr32 0xa5a52969 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shlr_w_ind_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	#2, @er0	; shift right logical by two, indirect
;;;	.word	0x7d80
;;;	.word	0x1150

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0010 1001 0110 1001  
	cmp.w	#0x2969, @word_dest
	beq	.Lwind2
	fail
.Lwind2:
	mov.w	#0xa5a5, @word_dest

shlr_w_postinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	#2, @er0+	; shift right logical by two, postinc
;;;	.word	0x0154
;;;	.word	0x6d08
;;;	.word	0x1150

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest+2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0010 1001 0110 1001  
	cmp.w	#0x2969, @word_dest
	beq	.Lwpostinc2
	fail
.Lwpostinc2:
	mov.w	#0xa5a5, @word_dest

shlr_w_postdec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	#2, @er0-	; shift right logical by two, postdec
;;;	.word	0x0156
;;;	.word	0x6d08
;;;	.word	0x1150

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0010 1001 0110 1001  
	cmp.w	#0x2969, @word_dest
	beq	.Lwpostdec2
	fail
.Lwpostdec2:
	mov.w	#0xa5a5, @word_dest

shlr_w_preinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-2, er0
	shlr.w	#2, @+er0	; shift right logical by two, preinc
;;;	.word	0x0155
;;;	.word	0x6d08
;;;	.word	0x1150

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0010 1001 0110 1001  
	cmp.w	#0x2969, @word_dest
	beq	.Lwpreinc2
	fail
.Lwpreinc2:
	mov.w	#0xa5a5, @word_dest

shlr_w_predec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest+2, er0
	shlr.w	#2, @-er0	; shift right logical by two, predec
;;;	.word	0x0157
;;;	.word	0x6d08
;;;	.word	0x1150

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0010 1001 0110 1001  
	cmp.w	#0x2969, @word_dest
	beq	.Lwpredec2
	fail
.Lwpredec2:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp2_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-4, er0
	shlr.w	#2, @(4:2, er0)	; shift right logical by two, disp2
;;;	.word	0x0156
;;;	.word	0x6908
;;;	.word	0x1150

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0010 1001 0110 1001  
	cmp.w	#0x2969, @word_dest
	beq	.Lwdisp22
	fail
.Lwdisp22:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-44, er0
	shlr.w	#2, @(44:16, er0)	; shift right logical by two, disp16
;;;	.word	0x0154
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x1150

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0010 1001 0110 1001  
	cmp.w	#0x2969, @word_dest
	beq	.Lwdisp162
	fail
.Lwdisp162:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-666, er0
	shlr.w	#2, @(666:32, er0)	; shift right logical by two, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x1150

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0010 1001 0110 1001  
	cmp.w	#0x2969, @word_dest
	beq	.Lwdisp322
	fail
.Lwdisp322:
	mov.w	#0xa5a5, @word_dest

shlr_w_abs16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	#2, @word_dest:16	; shift right logical by two, abs16
;;;	.word	0x6b18
;;;	.word	word_dest
;;;	.word	0x1150

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0010 1001 0110 1001  
	cmp.w	#0x2969, @word_dest
	beq	.Lwabs162
	fail
.Lwabs162:
	mov.w	#0xa5a5, @word_dest

shlr_w_abs32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	#2, @word_dest:32	; shift right logical by two, abs32
;;;	.word	0x6b38
;;; 	.long	word_dest
;;;	.word	0x1150

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0010 1001 0110 1001  
	cmp.w	#0x2969, @word_dest
	beq	.Lwabs322
	fail
.Lwabs322:
	mov.w	#0xa5a5, @word_dest
	
shlr_w_reg16_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	#4, r0		; shift right logical by four
;;;	.word	0x1120

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr16 0x0a5a r0	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	test_h_gr32 0xa5a50a5a er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shlr_w_reg16_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#5, r1l
	shlr.w	r1l, r0		; shift right logical by register value

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr16  0x052d r0	; 1010 0101 1010 0101 -> 0000 0101 0010 1101
	test_h_gr32  0xa5a5052d er0
	test_h_gr32  0xa5a5a505 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shlr_w_ind_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	#4, @er0	; shift right logical by four, indirect
;;;	.word	0x7d80
;;;	.word	0x1120

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	cmp.w	#0x0a5a, @word_dest
	beq	.Lwind4
	fail
.Lwind4:
	mov.w	#0xa5a5, @word_dest

shlr_w_postinc_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	#4, @er0+	; shift right logical by four, postinc
;;;	.word	0x0154
;;;	.word	0x6d08
;;;	.word	0x1120

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest+2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	cmp.w	#0x0a5a, @word_dest
	beq	.Lwpostinc4
	fail
.Lwpostinc4:
	mov.w	#0xa5a5, @word_dest

shlr_w_postdec_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	#4, @er0-	; shift right logical by four, postdec
;;;	.word	0x0156
;;;	.word	0x6d08
;;;	.word	0x1120

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	cmp.w	#0x0a5a, @word_dest
	beq	.Lwpostdec4
	fail
.Lwpostdec4:
	mov.w	#0xa5a5, @word_dest

shlr_w_preinc_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-2, er0
	shlr.w	#4, @+er0	; shift right logical by four, preinc
;;;	.word	0x0155
;;;	.word	0x6d08
;;;	.word	0x1120

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	cmp.w	#0x0a5a, @word_dest
	beq	.Lwpreinc4
	fail
.Lwpreinc4:
	mov.w	#0xa5a5, @word_dest

shlr_w_predec_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest+2, er0
	shlr.w	#4, @-er0	; shift right logical by four, predec
;;;	.word	0x0157
;;;	.word	0x6d08
;;;	.word	0x1120

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	cmp.w	#0x0a5a, @word_dest
	beq	.Lwpredec4
	fail
.Lwpredec4:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp2_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-4, er0
	shlr.w	#4, @(4:2, er0)	; shift right logical by four, disp2
;;;	.word	0x0156
;;;	.word	0x6908
;;;	.word	0x1120

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	cmp.w	#0x0a5a, @word_dest
	beq	.Lwdisp24
	fail
.Lwdisp24:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp16_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-44, er0
	shlr.w	#4, @(44:16, er0)	; shift right logical by four, disp16
;;;	.word	0x0154
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x1120

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	cmp.w	#0x0a5a, @word_dest
	beq	.Lwdisp164
	fail
.Lwdisp164:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp32_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-666, er0
	shlr.w	#4, @(666:32, er0)	; shift right logical by four, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x1120

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	cmp.w	#0x0a5a, @word_dest
	beq	.Lwdisp324
	fail
.Lwdisp324:
	mov.w	#0xa5a5, @word_dest

shlr_w_abs16_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	#4, @word_dest:16	; shift right logical by four, abs16
;;;	.word	0x6b18
;;;	.word	word_dest
;;;	.word	0x1120

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	cmp.w	#0x0a5a, @word_dest
	beq	.Lwabs164
	fail
.Lwabs164:
	mov.w	#0xa5a5, @word_dest

shlr_w_abs32_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	#4, @word_dest:32	; shift right logical by four, abs32
;;;	.word	0x6b38
;;; 	.long	word_dest
;;;	.word	0x1120

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 1010 0101 1010 
	cmp.w	#0x0a5a, @word_dest
	beq	.Lwabs324
	fail
.Lwabs324:
	mov.w	#0xa5a5, @word_dest

shlr_w_reg16_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	#8, r0		; shift right logical by eight
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr16 0x00a5 r0	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	test_h_gr32 0xa5a500a5 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shlr_w_ind_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	#8, @er0	; shift right logical by eight, indirect
;;;	.word	0x7d80
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	cmp.w	#0x00a5, @word_dest
	beq	.Lwind8
	fail
.Lwind8:
	mov.w	#0xa5a5, @word_dest

shlr_w_postinc_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	#8, @er0+	; shift right logical by eight, postinc
;;;	.word	0x0154
;;;	.word	0x6d08
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest+2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	cmp.w	#0x00a5, @word_dest
	beq	.Lwpostinc8
	fail
.Lwpostinc8:
	mov.w	#0xa5a5, @word_dest

shlr_w_postdec_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shlr.w	#8, @er0-	; shift right logical by eight, postdec
;;;	.word	0x0156
;;;	.word	0x6d08
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	cmp.w	#0x00a5, @word_dest
	beq	.Lwpostdec8
	fail
.Lwpostdec8:
	mov.w	#0xa5a5, @word_dest

shlr_w_preinc_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-2, er0
	shlr.w	#8, @+er0	; shift right logical by eight, preinc
;;;	.word	0x0155
;;;	.word	0x6d08
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	cmp.w	#0x00a5, @word_dest
	beq	.Lwpreinc8
	fail
.Lwpreinc8:
	mov.w	#0xa5a5, @word_dest

shlr_w_predec_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest+2, er0
	shlr.w	#8, @-er0	; shift right logical by eight, predec
;;;	.word	0x0157
;;;	.word	0x6d08
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	cmp.w	#0x00a5, @word_dest
	beq	.Lwpredec8
	fail
.Lwpredec8:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp2_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-4, er0
	shlr.w	#8, @(4:2, er0)	; shift right logical by eight, disp2
;;;	.word	0x0156
;;;	.word	0x6908
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	cmp.w	#0x00a5, @word_dest
	beq	.Lwdisp28
	fail
.Lwdisp28:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp16_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-44, er0
	shlr.w	#8, @(44:16, er0)	; shift right logical by eight, disp16
;;;	.word	0x0154
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	cmp.w	#0x00a5, @word_dest
	beq	.Lwdisp168
	fail
.Lwdisp168:
	mov.w	#0xa5a5, @word_dest

shlr_w_disp32_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-666, er0
	shlr.w	#8, @(666:32, er0)	; shift right logical by eight, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 word_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	cmp.w	#0x00a5, @word_dest
	beq	.Lwdisp328
	fail
.Lwdisp328:
	mov.w	#0xa5a5, @word_dest

shlr_w_abs16_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	#8, @word_dest:16	; shift right logical by eight, abs16
;;;	.word	0x6b18
;;;	.word	word_dest
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	cmp.w	#0x00a5, @word_dest
	beq	.Lwabs168
	fail
.Lwabs168:
	mov.w	#0xa5a5, @word_dest

shlr_w_abs32_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.w	#8, @word_dest:32	; shift right logical by eight, abs32
;;;	.word	0x6b38
;;; 	.long	word_dest
;;;	.word	0x1160

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0000 0000 1010 0101 
	cmp.w	#0x00a5, @word_dest
	beq	.Lwabs328
	fail
.Lwabs328:
	mov.w	#0xa5a5, @word_dest

shlr_l_imm5_1:	
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#31:5, er0	; shift right logical by 5-bit immediate
;;;	.word	0x0399
;;;	.word	0x1130

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 0000 0000 0000 0000 0000 0000 0000 0001
	test_h_gr32 0x1 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

shlr_l_reg32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	er0		; shift right logical by one, register
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 0101 0010 1101 0010 1101 0010 1101 0010
	test_h_gr32 0x52d2d2d2 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shlr_l_ind_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	@er0	; shift right logical by one, indirect
;;;	.word	0x0104
;;;	.word	0x6908
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0x52d2d2d2, @long_dest
	beq	.Llind1
	fail
.Llind1:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_postinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	@er0+	; shift right logical by one, postinc
;;;	.word	0x0104
;;;	.word	0x6d08
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest+4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0x52d2d2d2, @long_dest
	beq	.Llpostinc1
	fail
.Llpostinc1:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_postdec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	@er0-	; shift right logical by one, postdec
;;;	.word	0x0106
;;;	.word	0x6d08
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0x52d2d2d2, @long_dest
	beq	.Llpostdec1
	fail
.Llpostdec1:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_preinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-4, er0
	shlr.l	@+er0	; shift right logical by one, preinc
;;;	.word	0x0105
;;;	.word	0x6d08
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0x52d2d2d2, @long_dest
	beq	.Llpreinc1
	fail
.Llpreinc1:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_predec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest+4, er0
	shlr.l	@-er0	; shift right logical by one, predec
;;;	.word	0x0107
;;;	.word	0x6d08
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0x52d2d2d2, @long_dest
	beq	.Llpredec1
	fail
.Llpredec1:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp2_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-8, er0
	shlr.l	@(8:2, er0)	; shift right logical by one, disp2
;;;	.word	0x0106
;;;	.word	0x6908
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-8 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0x52d2d2d2, @long_dest
	beq	.Lldisp21
	fail
.Lldisp21:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-44, er0
	shlr.l	@(44:16, er0)	; shift right logical by one, disp16
;;;	.word	0x0104
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0x52d2d2d2, @long_dest
	beq	.Lldisp161
	fail
.Lldisp161:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-666, er0
	shlr.l	@(666:32, er0)	; shift right logical by one, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0x52d2d2d2, @long_dest
	beq	.Lldisp321
	fail
.Lldisp321:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_abs16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	@long_dest:16	; shift right logical by one, abs16
;;;	.word	0x0104
;;;	.word	0x6b08
;;;	.word	long_dest
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0x52d2d2d2, @long_dest
	beq	.Llabs161
	fail
.Llabs161:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_abs32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	@long_dest:32	; shift right logical by one, abs32
;;;	.word	0x0104
;;;	.word	0x6b28
;;; 	.long	long_dest
;;;	.word	0x1130

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0x52d2d2d2, @long_dest
	beq	.Llabs321
	fail
.Llabs321:
	mov	#0xa5a5a5a5, @long_dest
.endif

shlr_l_reg32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#2, er0		; shift right logical by two, register
;;;	.word	0x1170

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	; 1010 0101 1010 0101 1010 0101 1010 0101
	; -> 0010 1001 0110 1001 0110 1001 0110 1001
	test_h_gr32 0x29696969 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)

shlr_l_ind_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#2, @er0	; shift right logical by two, indirect
;;;	.word	0x0104
;;;	.word	0x6908
;;;	.word	0x1170

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0010 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0x29696969, @long_dest
	beq	.Llind2
	fail
.Llind2:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_postinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#2, @er0+	; shift right logical by two, postinc
;;;	.word	0x0104
;;;	.word	0x6d08
;;;	.word	0x1170

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest+4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0010 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0x29696969, @long_dest
	beq	.Llpostinc2
	fail
.Llpostinc2:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_postdec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#2, @er0-	; shift right logical by two, postdec
;;;	.word	0x0106
;;;	.word	0x6d08
;;;	.word	0x1170

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0010 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0x29696969, @long_dest
	beq	.Llpostdec2
	fail
.Llpostdec2:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_preinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-4, er0
	shlr.l	#2, @+er0	; shift right logical by two, preinc
;;;	.word	0x0105
;;;	.word	0x6d08
;;;	.word	0x1170

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0010 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0x29696969, @long_dest
	beq	.Llpreinc2
	fail
.Llpreinc2:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_predec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest+4, er0
	shlr.l	#2, @-er0	; shift right logical by two, predec
;;;	.word	0x0107
;;;	.word	0x6d08
;;;	.word	0x1170

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0010 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0x29696969, @long_dest
	beq	.Llpredec2
	fail
.Llpredec2:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp2_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-8, er0
	shlr.l	#2, @(8:2, er0)	; shift right logical by two, disp2
;;;	.word	0x0106
;;;	.word	0x6908
;;;	.word	0x1170

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-8 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0010 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0x29696969, @long_dest
	beq	.Lldisp22
	fail
.Lldisp22:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-44, er0
	shlr.l	#2, @(44:16, er0)	; shift right logical by two, disp16
;;;	.word	0x0104
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x1170

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0010 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0x29696969, @long_dest
	beq	.Lldisp162
	fail
.Lldisp162:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-666, er0
	shlr.l	#2, @(666:32, er0)	; shift right logical by two, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x1170

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0010 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0x29696969, @long_dest
	beq	.Lldisp322
	fail
.Lldisp322:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_abs16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#2, @long_dest:16	; shift right logical by two, abs16
;;;	.word	0x0104
;;;	.word	0x6b08
;;;	.word	long_dest
;;;	.word	0x1170

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0010 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0x29696969, @long_dest
	beq	.Llabs162
	fail
.Llabs162:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_abs32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#2, @long_dest:32	; shift right logical by two, abs32
;;;	.word	0x0104
;;;	.word	0x6b28
;;; 	.long	long_dest
;;;	.word	0x1170

	test_carry_clear		; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0010 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0x29696969, @long_dest
	beq	.Llabs322
	fail
.Llabs322:
	mov	#0xa5a5a5a5, @long_dest
	
shlr_l_reg32_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#4, er0		; shift right logical by four, register
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	; 1010 0101 1010 0101 1010 0101 1010 0101
	; -> 0000 1010 0101 1010 0101 1010 0101 1010 
	test_h_gr32 0x0a5a5a5a er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shlr_l_reg32_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#5, r1l
	shlr.l	r1l, er0	; shift right logical by value of register

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	; 1010 0101 1010 0101 1010 0101 1010 0101
	; -> 0000 0101 0010 1101 0010 1101 0010 1101
	test_h_gr32  0x052d2d2d er0
	test_h_gr32  0xa5a5a505 er1

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shlr_l_ind_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#4, @er0	; shift right logical by four, indirect
;;;	.word	0x0104
;;;	.word	0x6908
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 1010 0101 1010 0101 1010 0101 1010
	cmp.l	#0x0a5a5a5a, @long_dest
	beq	.Llind4
	fail
.Llind4:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_postinc_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#4, @er0+	; shift right logical by four, postinc
;;;	.word	0x0104
;;;	.word	0x6d08
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest+4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 1010 0101 1010 0101 1010 0101 1010
	cmp.l	#0x0a5a5a5a, @long_dest
	beq	.Llpostinc4
	fail
.Llpostinc4:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_postdec_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#4, @er0-	; shift right logical by four, postdec
;;;	.word	0x0106
;;;	.word	0x6d08
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 1010 0101 1010 0101 1010 0101 1010
	cmp.l	#0x0a5a5a5a, @long_dest
	beq	.Llpostdec4
	fail
.Llpostdec4:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_preinc_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-4, er0
	shlr.l	#4, @+er0	; shift right logical by four, preinc
;;;	.word	0x0105
;;;	.word	0x6d08
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 1010 0101 1010 0101 1010 0101 1010
	cmp.l	#0x0a5a5a5a, @long_dest
	beq	.Llpreinc4
	fail
.Llpreinc4:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_predec_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest+4, er0
	shlr.l	#4, @-er0	; shift right logical by four, predec
;;;	.word	0x0107
;;;	.word	0x6d08
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 1010 0101 1010 0101 1010 0101 1010
	cmp.l	#0x0a5a5a5a, @long_dest
	beq	.Llpredec4
	fail
.Llpredec4:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp2_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-8, er0
	shlr.l	#4, @(8:2, er0)	; shift right logical by four, disp2
;;;	.word	0x0106
;;;	.word	0x6908
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-8 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 1010 0101 1010 0101 1010 0101 1010
	cmp.l	#0x0a5a5a5a, @long_dest
	beq	.Lldisp24
	fail
.Lldisp24:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp16_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-44, er0
	shlr.l	#4, @(44:16, er0)	; shift right logical by four, disp16
;;;	.word	0x0104
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 1010 0101 1010 0101 1010 0101 1010
	cmp.l	#0x0a5a5a5a, @long_dest
	beq	.Lldisp164
	fail
.Lldisp164:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp32_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-666, er0
	shlr.l	#4, @(666:32, er0)	; shift right logical by four, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 1010 0101 1010 0101 1010 0101 1010
	cmp.l	#0x0a5a5a5a, @long_dest
	beq	.Lldisp324
	fail
.Lldisp324:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_abs16_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#4, @long_dest:16	; shift right logical by four, abs16
;;;	.word	0x0104
;;;	.word	0x6b08
;;;	.word	long_dest
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 1010 0101 1010 0101 1010 0101 1010
	cmp.l	#0x0a5a5a5a, @long_dest
	beq	.Llabs164
	fail
.Llabs164:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_abs32_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#4, @long_dest:32	; shift right logical by four, abs32
;;;	.word	0x0104
;;;	.word	0x6b28
;;; 	.long	long_dest
;;;	.word	0x1138

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 1010 0101 1010 0101 1010 0101 1010
	cmp.l	#0x0a5a5a5a, @long_dest
	beq	.Llabs324
	fail
.Llabs324:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_reg32_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#8, er0		; shift right logical by eight, register
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	; 1010 0101 1010 0101 1010 0101 1010 0101
	; -> 0000 0000 1010 0101 1010 0101 1010 0101 
	test_h_gr32 0x00a5a5a5 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shlr_l_ind_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#8, @er0	; shift right logical by eight, indirect
;;;	.word	0x0104
;;;	.word	0x6908
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 1010 0101 1010 0101 1010 0101
	cmp.l	#0x00a5a5a5, @long_dest
	beq	.Llind8
	fail
.Llind8:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_postinc_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#8, @er0+	; shift right logical by eight, postinc
;;;	.word	0x0104
;;;	.word	0x6d08
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest+4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 1010 0101 1010 0101 1010 0101
	cmp.l	#0x00a5a5a5, @long_dest
	beq	.Llpostinc8
	fail
.Llpostinc8:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_postdec_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#8, @er0-	; shift right logical by eight, postdec
;;;	.word	0x0106
;;;	.word	0x6d08
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 1010 0101 1010 0101 1010 0101
	cmp.l	#0x00a5a5a5, @long_dest
	beq	.Llpostdec8
	fail
.Llpostdec8:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_preinc_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-4, er0
	shlr.l	#8, @+er0	; shift right logical by eight, preinc
;;;	.word	0x0105
;;;	.word	0x6d08
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 1010 0101 1010 0101 1010 0101
	cmp.l	#0x00a5a5a5, @long_dest
	beq	.Llpreinc8
	fail
.Llpreinc8:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_predec_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest+4, er0
	shlr.l	#8, @-er0	; shift right logical by eight, predec
;;;	.word	0x0107
;;;	.word	0x6d08
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 1010 0101 1010 0101 1010 0101
	cmp.l	#0x00a5a5a5, @long_dest
	beq	.Llpredec8
	fail
.Llpredec8:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp2_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-8, er0
	shlr.l	#8, @(8:2, er0)	; shift right logical by eight, disp2
;;;	.word	0x0106
;;;	.word	0x6908
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-8 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 1010 0101 1010 0101 1010 0101
	cmp.l	#0x00a5a5a5, @long_dest
	beq	.Lldisp28
	fail
.Lldisp28:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp16_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-44, er0
	shlr.l	#8, @(44:16, er0)	; shift right logical by eight, disp16
;;;	.word	0x0104
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 1010 0101 1010 0101 1010 0101
	cmp.l	#0x00a5a5a5, @long_dest
	beq	.Lldisp168
	fail
.Lldisp168:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp32_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-666, er0
	shlr.l	#8, @(666:32, er0)	; shift right logical by eight, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 1010 0101 1010 0101 1010 0101
	cmp.l	#0x00a5a5a5, @long_dest
	beq	.Lldisp328
	fail
.Lldisp328:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_abs16_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#8, @long_dest:16	; shift right logical by eight, abs16
;;;	.word	0x0104
;;;	.word	0x6b08
;;;	.word	long_dest
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 1010 0101 1010 0101 1010 0101
	cmp.l	#0x00a5a5a5, @long_dest
	beq	.Llabs168
	fail
.Llabs168:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_abs32_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#8, @long_dest:32	; shift right logical by eight, abs32
;;;	.word	0x0104
;;;	.word	0x6b28
;;; 	.long	long_dest
;;;	.word	0x1178

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 1010 0101 1010 0101 1010 0101
	cmp.l	#0x00a5a5a5, @long_dest
	beq	.Llabs328
	fail
.Llabs328:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_reg32_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#16, er0	; shift right logical by sixteen, register
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	test_h_gr32 0x0000a5a5 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shlr_l_ind_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#16, @er0	; shift right logical by sixteen, indirect
;;;	.word	0x0104
;;;	.word	0x6908
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	cmp.l	#0x0000a5a5, @long_dest
	beq	.Llind16
	fail
.Llind16:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_postinc_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#16, @er0+	; shift right logical by sixteen, postinc
;;;	.word	0x0104
;;;	.word	0x6d08
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest+4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	cmp.l	#0x0000a5a5, @long_dest
	beq	.Llpostinc16
	fail
.Llpostinc16:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_postdec_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shlr.l	#16, @er0-	; shift right logical by sixteen, postdec
;;;	.word	0x0106
;;;	.word	0x6d08
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	cmp.l	#0x0000a5a5, @long_dest
	beq	.Llpostdec16
	fail
.Llpostdec16:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_preinc_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-4, er0
	shlr.l	#16, @+er0	; shift right logical by sixteen, preinc
;;;	.word	0x0105
;;;	.word	0x6d08
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	cmp.l	#0x0000a5a5, @long_dest
	beq	.Llpreinc16
	fail
.Llpreinc16:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_predec_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest+4, er0
	shlr.l	#16, @-er0	; shift right logical by sixteen, predec
;;;	.word	0x0107
;;;	.word	0x6d08
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	cmp.l	#0x0000a5a5, @long_dest
	beq	.Llpredec16
	fail
.Llpredec16:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp2_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-8, er0
	shlr.l	#16, @(8:2, er0)	; shift right logical by 16, dest2
;;;	.word	0x0106
;;;	.word	0x6908
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-8 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	cmp.l	#0x0000a5a5, @long_dest
	beq	.Lldisp216
	fail
.Lldisp216:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp16_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-44, er0
	shlr.l	#16, @(44:16, er0)	; shift right logical by 16, disp16
;;;	.word	0x0104
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	cmp.l	#0x0000a5a5, @long_dest
	beq	.Lldisp1616
	fail
.Lldisp1616:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_disp32_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-666, er0
	shlr.l	#16, @(666:32, er0)	; shift right logical by 16, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32 long_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	cmp.l	#0x0000a5a5, @long_dest
	beq	.Lldisp3216
	fail
.Lldisp3216:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_abs16_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#16, @long_dest:16	; shift right logical by 16, abs16
;;;	.word	0x0104
;;;	.word	0x6b08
;;;	.word	long_dest
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	cmp.l	#0x0000a5a5, @long_dest
	beq	.Llabs1616
	fail
.Llabs1616:
	mov	#0xa5a5a5a5, @long_dest

shlr_l_abs32_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shlr.l	#16, @long_dest:32	; shift right logical by 16, abs32
;;;	.word	0x0104
;;;	.word	0x6b28
;;; 	.long	long_dest
;;;	.word	0x11f8

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 0000 0000 0000 0000 1010 0101 1010 0101
	cmp.l	#0x0000a5a5, @long_dest
	beq	.Llabs3216
	fail
.Llabs3216:
	mov	#0xa5a5a5a5, @long_dest
.endif
.endif
	pass

	exit 0

