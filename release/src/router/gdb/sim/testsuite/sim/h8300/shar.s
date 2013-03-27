# Hitachi H8 testcase 'shar'
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

shar_b_reg8_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.b	r0l		; shift right arithmetic by one
;;;	.word	0x1188

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr16 0xa5d2 r0	; 1010 0101 -> 1101 0010
.if (sim_cpu)
	test_h_gr32 0xa5a5a5d2 er0
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shar_b_ind_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shar.b	@er0	; shift right arithmetic by one, indirect
;;;	.word	0x7d00
;;;	.word	0x1180

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1101 0010
	cmp.b	#0xd2, @byte_dest
	beq	.Lbind1
	fail
.Lbind1:
	mov.b	#0xa5, @byte_dest

shar_b_postinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shar.b	@er0+	; shift right arithmetic by one, postinc
;;;	.word	0x0174
;;;	.word	0x6c08
;;;	.word	0x1180

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest+1 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1101 0010
	cmp.b	#0xd2, @byte_dest
	beq	.Lbpostinc1
	fail
.Lbpostinc1:
	mov.b	#0xa5, @byte_dest

shar_b_postdec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shar.b	@er0-	; shift right arithmetic by one, postdec
;;;	.word	0x0176
;;;	.word	0x6c08
;;;	.word	0x1180

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest-1 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1101 0010
	cmp.b	#0xd2, @byte_dest
	beq	.Lbpostdec1
	fail
.Lbpostdec1:
	mov.b	#0xa5, @byte_dest

shar_b_preinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-1, er0
	shar.b	@+er0	; shift right arithmetic by one, preinc
;;;	.word	0x0175
;;;	.word	0x6c08
;;;	.word	0x1180

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1101 0010
	cmp.b	#0xd2, @byte_dest
	beq	.Lbpreinc1
	fail
.Lbpreinc1:
	mov.b	#0xa5, @byte_dest

shar_b_predec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest+1, er0
	shar.b	@-er0	; shift right arithmetic by one, predec
;;;	.word	0x0177
;;;	.word	0x6c08
;;;	.word	0x1180

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1101 0010
	cmp.b	#0xd2, @byte_dest
	beq	.Lbpredec1
	fail
.Lbpredec1:
	mov.b	#0xa5, @byte_dest

shar_b_disp2_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-2, er0
	shar.b	@(2:2, er0)	; shift right arithmetic by one, disp2
;;;	.word	0x0176
;;;	.word	0x6808
;;;	.word	0x1180

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1101 0010
	cmp.b	#0xd2, @byte_dest
	beq	.Lbdisp21
	fail
.Lbdisp21:
	mov.b	#0xa5, @byte_dest

shar_b_disp16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-44, er0
	shar.b	@(44:16, er0)	; shift right arithmetic by one, disp16
;;;	.word	0x0174
;;;	.word	0x6e08
;;;	.word	44
;;;	.word	0x1180

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1101 0010
	cmp.b	#0xd2, @byte_dest
	beq	.Lbdisp161
	fail
.Lbdisp161:
	mov.b	#0xa5, @byte_dest

shar_b_disp32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-666, er0
	shar.b	@(666:32, er0)	; shift right arithmetic by one, disp32
;;;	.word	0x7884
;;;	.word	0x6a28
;;; 	.long	666
;;;	.word	0x1180

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1101 0010
	cmp.b	#0xd2, @byte_dest
	beq	.Lbdisp321
	fail
.Lbdisp321:
	mov.b	#0xa5, @byte_dest

shar_b_abs16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.b	@byte_dest:16	; shift right arithmetic by one, abs16
;;;	.word	0x6a18
;;;	.word	byte_dest
;;;	.word	0x1180

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1101 0010
	cmp.b	#0xd2, @byte_dest
	beq	.Lbabs161
	fail
.Lbabs161:
	mov.b	#0xa5, @byte_dest

shar_b_abs32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.b	@byte_dest:32	; shift right arithmetic by one, abs32
;;;	.word	0x6a38
;;; 	.long	byte_dest
;;;	.word	0x1180

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1101 0010
	cmp.b	#0xd2, @byte_dest
	beq	.Lbabs321
	fail
.Lbabs321:
	mov.b	#0xa5, @byte_dest
.endif

shar_b_reg8_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.b	#2, r0l		; shift right arithmetic by two
;;;	.word	0x11c8

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set
	test_h_gr16 0xa5e9 r0	; 1010 0101 -> 1110 1001
.if (sim_cpu)
	test_h_gr32 0xa5a5a5e9 er0
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shar_b_ind_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shar.b	#2, @er0	; shift right arithmetic by two, indirect
;;;	.word	0x7d00
;;;	.word	0x11c0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1110 1001
	cmp.b	#0xe9, @byte_dest
	beq	.Lbind2
	fail
.Lbind2:
	mov.b	#0xa5, @byte_dest

shar_b_postinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shar.b	#2, @er0+	; shift right arithmetic by two, postinc
;;;	.word	0x0174
;;;	.word	0x6c08
;;;	.word	0x11c0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest+1 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1110 1001
	cmp.b	#0xe9, @byte_dest
	beq	.Lbpostinc2
	fail
.Lbpostinc2:
	mov.b	#0xa5, @byte_dest

shar_b_postdec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	shar.b	#2, @er0-	; shift right arithmetic by two, postdec
;;;	.word	0x0176
;;;	.word	0x6c08
;;;	.word	0x11c0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest-1 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1110 1001
	cmp.b	#0xe9, @byte_dest
	beq	.Lbpostdec2
	fail
.Lbpostdec2:
	mov.b	#0xa5, @byte_dest

shar_b_preinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-1, er0
	shar.b	#2, @+er0	; shift right arithmetic by two, preinc
;;;	.word	0x0175
;;;	.word	0x6c08
;;;	.word	0x11c0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1110 1001
	cmp.b	#0xe9, @byte_dest
	beq	.Lbpreinc2
	fail
.Lbpreinc2:
	mov.b	#0xa5, @byte_dest

shar_b_predec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest+1, er0
	shar.b	#2, @-er0	; shift right arithmetic by two, predec
;;;	.word	0x0177
;;;	.word	0x6c08
;;;	.word	0x11c0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1110 1001
	cmp.b	#0xe9, @byte_dest
	beq	.Lbpredec2
	fail
.Lbpredec2:
	mov.b	#0xa5, @byte_dest

shar_b_disp2_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-2, er0
	shar.b	#2, @(2:2, er0)	; shift right arithmetic by two, disp2
;;;	.word	0x0176
;;;	.word	0x6808
;;;	.word	0x11c0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1110 1001
	cmp.b	#0xe9, @byte_dest
	beq	.Lbdisp22
	fail
.Lbdisp22:
	mov.b	#0xa5, @byte_dest

shar_b_disp16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-44, er0
	shar.b	#2, @(44:16, er0)	; shift right arithmetic by two, disp16
;;;	.word	0x0174
;;;	.word	0x6e08
;;;	.word	44
;;;	.word	0x11c0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1110 1001
	cmp.b	#0xe9, @byte_dest
	beq	.Lbdisp162
	fail
.Lbdisp162:
	mov.b	#0xa5, @byte_dest

shar_b_disp32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest-666, er0
	shar.b	#2, @(666:32, er0)	; shift right arithmetic by two, disp32
;;;	.word	0x7884
;;;	.word	0x6a28
;;; 	.long	666
;;;	.word	0x11c0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1110 1001
	cmp.b	#0xe9, @byte_dest
	beq	.Lbdisp322
	fail
.Lbdisp322:
	mov.b	#0xa5, @byte_dest

shar_b_abs16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.b	#2, @byte_dest:16	; shift right arithmetic by two, abs16
;;;	.word	0x6a18
;;;	.word	byte_dest
;;;	.word	0x11c0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1110 1001
	cmp.b	#0xe9, @byte_dest
	beq	.Lbabs162
	fail
.Lbabs162:
	mov.b	#0xa5, @byte_dest

shar_b_abs32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.b	#2, @byte_dest:32	; shift right arithmetic by two, abs32
;;;	.word	0x6a38
;;; 	.long	byte_dest
;;;	.word	0x11c0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1110 1001
	cmp.b	#0xe9, @byte_dest
	beq	.Lbabs322
	fail
.Lbabs322:
	mov.b	#0xa5, @byte_dest
.endif

.if (sim_cpu)			; Not available in h8300 mode
shar_w_reg16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.w	r0		; shift right arithmetic by one
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set
	test_h_gr16 0xd2d2 r0	; 1010 0101 1010 0101 -> 1101 0010 1101 0010
	test_h_gr32 0xa5a5d2d2 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shar_w_ind_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shar.w	@er0	; shift right arithmetic by one, indirect
;;;	.word	0x7d80
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1101 0010 1101 0010 
	cmp.w	#0xd2d2, @word_dest
	beq	.Lwind1
	fail
.Lwind1:
	mov.w	#0xa5a5, @word_dest

shar_w_postinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shar.w	@er0+	; shift right arithmetic by one, postinc
;;;	.word	0x0154
;;;	.word	0x6d08
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest+2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1101 0010 1101 0010 
	cmp.w	#0xd2d2, @word_dest
	beq	.Lwpostinc1
	fail
.Lwpostinc1:
	mov.w	#0xa5a5, @word_dest

shar_w_postdec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shar.w	@er0-	; shift right arithmetic by one, postdec
;;;	.word	0x0156
;;;	.word	0x6d08
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1101 0010 1101 0010 
	cmp.w	#0xd2d2, @word_dest
	beq	.Lwpostdec1
	fail
.Lwpostdec1:
	mov.w	#0xa5a5, @word_dest

shar_w_preinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-2, er0
	shar.w	@+er0	; shift right arithmetic by one, preinc
;;;	.word	0x0155
;;;	.word	0x6d08
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1101 0010 1101 0010 
	cmp.w	#0xd2d2, @word_dest
	beq	.Lwpreinc1
	fail
.Lwpreinc1:
	mov.w	#0xa5a5, @word_dest

shar_w_predec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest+2, er0
	shar.w	@-er0	; shift right arithmetic by one, predec
;;;	.word	0x0157
;;;	.word	0x6d08
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1101 0010 1101 0010 
	cmp.w	#0xd2d2, @word_dest
	beq	.Lwpredec1
	fail
.Lwpredec1:
	mov.w	#0xa5a5, @word_dest

shar_w_disp2_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-4, er0
	shar.w	@(4:2, er0)	; shift right arithmetic by one, disp2
;;;	.word	0x0156
;;;	.word	0x6908
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1101 0010 1101 0010 
	cmp.w	#0xd2d2, @word_dest
	beq	.Lwdisp21
	fail
.Lwdisp21:
	mov.w	#0xa5a5, @word_dest

shar_w_disp16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-44, er0
	shar.w	@(44:16, er0)	; shift right arithmetic by one, disp16
;;;	.word	0x0154
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1101 0010 1101 0010 
	cmp.w	#0xd2d2, @word_dest
	beq	.Lwdisp161
	fail
.Lwdisp161:
	mov.w	#0xa5a5, @word_dest

shar_w_disp32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-666, er0
	shar.w	@(666:32, er0)	; shift right arithmetic by one, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1101 0010 1101 0010 
	cmp.w	#0xd2d2, @word_dest
	beq	.Lwdisp321
	fail
.Lwdisp321:
	mov.w	#0xa5a5, @word_dest

shar_w_abs16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.w	@word_dest:16	; shift right arithmetic by one, abs16
;;;	.word	0x6b18
;;;	.word	word_dest
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1101 0010 1101 0010 
	cmp.w	#0xd2d2, @word_dest
	beq	.Lwabs161
	fail
.Lwabs161:
	mov.w	#0xa5a5, @word_dest

shar_w_abs32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.w	@word_dest:32	; shift right arithmetic by one, abs32
;;;	.word	0x6b38
;;; 	.long	word_dest
;;;	.word	0x1190

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1101 0010 1101 0010 
	cmp.w	#0xd2d2, @word_dest
	beq	.Lwabs321
	fail
.Lwabs321:
	mov.w	#0xa5a5, @word_dest
.endif
	
shar_w_reg16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.w	#2, r0		; shift right arithmetic by two
;;;	.word	0x11d0

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr16 0xe969 r0	; 1010 0101 1010 0101 -> 1110 1001 0110 1001
	test_h_gr32 0xa5a5e969 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shar_w_ind_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shar.w	#2, @er0	; shift right arithmetic by two, indirect
;;;	.word	0x7d80
;;;	.word	0x11d0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1110 1001 0110 1001  
	cmp.w	#0xe969, @word_dest
	beq	.Lwind2
	fail
.Lwind2:
	mov.w	#0xa5a5, @word_dest

shar_w_postinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shar.w	#2, @er0+	; shift right arithmetic by two, postinc
;;;	.word	0x0154
;;;	.word	0x6d08
;;;	.word	0x11d0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest+2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1110 1001 0110 1001  
	cmp.w	#0xe969, @word_dest
	beq	.Lwpostinc2
	fail
.Lwpostinc2:
	mov.w	#0xa5a5, @word_dest

shar_w_postdec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest, er0
	shar.w	#2, @er0-	; shift right arithmetic by two, postdec
;;;	.word	0x0156
;;;	.word	0x6d08
;;;	.word	0x11d0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest-2 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1110 1001 0110 1001  
	cmp.w	#0xe969, @word_dest
	beq	.Lwpostdec2
	fail
.Lwpostdec2:
	mov.w	#0xa5a5, @word_dest

shar_w_preinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-2, er0
	shar.w	#2, @+er0	; shift right arithmetic by two, preinc
;;;	.word	0x0155
;;;	.word	0x6d08
;;;	.word	0x11d0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1110 1001 0110 1001  
	cmp.w	#0xe969, @word_dest
	beq	.Lwpreinc2
	fail
.Lwpreinc2:
	mov.w	#0xa5a5, @word_dest

shar_w_predec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest+2, er0
	shar.w	#2, @-er0	; shift right arithmetic by two, predec
;;;	.word	0x0157
;;;	.word	0x6d08
;;;	.word	0x11d0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1110 1001 0110 1001  
	cmp.w	#0xe969, @word_dest
	beq	.Lwpredec2
	fail
.Lwpredec2:
	mov.w	#0xa5a5, @word_dest

shar_w_disp2_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-4, er0
	shar.w	#2, @(4:2, er0)	; shift right arithmetic by two, disp2
;;;	.word	0x0156
;;;	.word	0x6908
;;;	.word	0x11d0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1110 1001 0110 1001  
	cmp.w	#0xe969, @word_dest
	beq	.Lwdisp22
	fail
.Lwdisp22:
	mov.w	#0xa5a5, @word_dest

shar_w_disp16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-44, er0
	shar.w	#2, @(44:16, er0)	; shift right arithmetic by two, disp16
;;;	.word	0x0154
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x11d0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1110 1001 0110 1001  
	cmp.w	#0xe969, @word_dest
	beq	.Lwdisp162
	fail
.Lwdisp162:
	mov.w	#0xa5a5, @word_dest

shar_w_disp32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#word_dest-666, er0
	shar.w	#2, @(666:32, er0)	; shift right arithmetic by two, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x11d0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  word_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1110 1001 0110 1001  
	cmp.w	#0xe969, @word_dest
	beq	.Lwdisp322
	fail
.Lwdisp322:
	mov.w	#0xa5a5, @word_dest

shar_w_abs16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.w	#2, @word_dest:16	; shift right arithmetic by two, abs16
;;;	.word	0x6b18
;;;	.word	word_dest
;;;	.word	0x11d0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1110 1001 0110 1001  
	cmp.w	#0xe969, @word_dest
	beq	.Lwabs162
	fail
.Lwabs162:
	mov.w	#0xa5a5, @word_dest

shar_w_abs32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.w	#2, @word_dest:32	; shift right arithmetic by two, abs32
;;;	.word	0x6b38
;;; 	.long	word_dest
;;;	.word	0x11d0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1110 1001 0110 1001  
	cmp.w	#0xe969, @word_dest
	beq	.Lwabs322
	fail
.Lwabs322:
	mov.w	#0xa5a5, @word_dest
.endif

shar_l_reg32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.l	er0		; shift right arithmetic by one, register
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 1101 0010 1101 0010 1101 0010 1101 0010
	test_h_gr32  0xd2d2d2d2 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shar_l_ind_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shar.l	@er0	; shift right arithmetic by one, indirect
;;;	.word	0x0104
;;;	.word	0x6908
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0xd2d2d2d2, @long_dest
	beq	.Llind1
	fail
.Llind1:
	mov	#0xa5a5a5a5, @long_dest

shar_l_postinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shar.l	@er0+	; shift right arithmetic by one, postinc
;;;	.word	0x0104
;;;	.word	0x6d08
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest+4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0xd2d2d2d2, @long_dest
	beq	.Llpostinc1
	fail
.Llpostinc1:
	mov	#0xa5a5a5a5, @long_dest

shar_l_postdec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shar.l	@er0-	; shift right arithmetic by one, postdec
;;;	.word	0x0106
;;;	.word	0x6d08
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0xd2d2d2d2, @long_dest
	beq	.Llpostdec1
	fail
.Llpostdec1:
	mov	#0xa5a5a5a5, @long_dest

shar_l_preinc_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-4, er0
	shar.l	@+er0	; shift right arithmetic by one, preinc
;;;	.word	0x0105
;;;	.word	0x6d08
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0xd2d2d2d2, @long_dest
	beq	.Llpreinc1
	fail
.Llpreinc1:
	mov	#0xa5a5a5a5, @long_dest

shar_l_predec_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest+4, er0
	shar.l	@-er0	; shift right arithmetic by one, predec
;;;	.word	0x0107
;;;	.word	0x6d08
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0xd2d2d2d2, @long_dest
	beq	.Llpredec1
	fail
.Llpredec1:
	mov	#0xa5a5a5a5, @long_dest

shar_l_disp2_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-8, er0
	shar.l	@(8:2, er0)	; shift right arithmetic by one, disp2
;;;	.word	0x0106
;;;	.word	0x6908
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest-8 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0xd2d2d2d2, @long_dest
	beq	.Lldisp21
	fail
.Lldisp21:
	mov	#0xa5a5a5a5, @long_dest

shar_l_disp16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-44, er0
	shar.l	@(44:16, er0)	; shift right arithmetic by one, disp16
;;;	.word	0x0104
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0xd2d2d2d2, @long_dest
	beq	.Lldisp161
	fail
.Lldisp161:
	mov	#0xa5a5a5a5, @long_dest

shar_l_disp32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-666, er0
	shar.l	@(666:32, er0)	; shift right arithmetic by one, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0xd2d2d2d2, @long_dest
	beq	.Lldisp321
	fail
.Lldisp321:
	mov	#0xa5a5a5a5, @long_dest

shar_l_abs16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.l	@long_dest:16	; shift right arithmetic by one, abs16
;;;	.word	0x0104
;;;	.word	0x6b08
;;;	.word	long_dest
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0xd2d2d2d2, @long_dest
	beq	.Llabs161
	fail
.Llabs161:
	mov	#0xa5a5a5a5, @long_dest

shar_l_abs32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.l	@long_dest:32	; shift right arithmetic by one, abs32
;;;	.word	0x0104
;;;	.word	0x6b28
;;; 	.long	long_dest
;;;	.word	0x11b0

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1101 0010 1101 0010 1101 0010 1101 0010
	cmp.l	#0xd2d2d2d2, @long_dest
	beq	.Llabs321
	fail
.Llabs321:
	mov	#0xa5a5a5a5, @long_dest
.endif

shar_l_reg32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.l	#2, er0		; shift right arithmetic by two, register
;;;	.word	0x11f0

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set
	; 1010 0101 1010 0101 1010 0101 1010 0101
	; -> 1110 1001 0110 1001 0110 1001 0110 1001
	test_h_gr32  0xe9696969 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)

shar_l_ind_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shar.l	#2, @er0	; shift right arithmetic by two, indirect
;;;	.word	0x0104
;;;	.word	0x6908
;;;	.word	0x11f0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1110 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0xe9696969, @long_dest
	beq	.Llind2
	fail
.Llind2:
	mov	#0xa5a5a5a5, @long_dest

shar_l_postinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shar.l	#2, @er0+	; shift right arithmetic by two, postinc
;;;	.word	0x0104
;;;	.word	0x6d08
;;;	.word	0x11f0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest+4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1110 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0xe9696969, @long_dest
	beq	.Llpostinc2
	fail
.Llpostinc2:
	mov	#0xa5a5a5a5, @long_dest

shar_l_postdec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest, er0
	shar.l	#2, @er0-	; shift right arithmetic by two, postdec
;;;	.word	0x0106
;;;	.word	0x6d08
;;;	.word	0x11f0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest-4 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1110 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0xe9696969, @long_dest
	beq	.Llpostdec2
	fail
.Llpostdec2:
	mov	#0xa5a5a5a5, @long_dest

shar_l_preinc_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-4, er0
	shar.l	#2, @+er0	; shift right arithmetic by two, preinc
;;;	.word	0x0105
;;;	.word	0x6d08
;;;	.word	0x11f0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1110 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0xe9696969, @long_dest
	beq	.Llpreinc2
	fail
.Llpreinc2:
	mov	#0xa5a5a5a5, @long_dest

shar_l_predec_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest+4, er0
	shar.l	#2, @-er0	; shift right arithmetic by two, predec
;;;	.word	0x0107
;;;	.word	0x6d08
;;;	.word	0x11f0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1110 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0xe9696969, @long_dest
	beq	.Llpredec2
	fail
.Llpredec2:
	mov	#0xa5a5a5a5, @long_dest

shar_l_disp2_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-8, er0
	shar.l	#2, @(8:2, er0)	; shift right arithmetic by two, disp2
;;;	.word	0x0106
;;;	.word	0x6908
;;;	.word	0x11f0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest-8 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1110 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0xe9696969, @long_dest
	beq	.Lldisp22
	fail
.Lldisp22:
	mov	#0xa5a5a5a5, @long_dest

shar_l_disp16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-44, er0
	shar.l	#2, @(44:16, er0)	; shift right arithmetic by two, disp16
;;;	.word	0x0104
;;;	.word	0x6f08
;;;	.word	44
;;;	.word	0x11f0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest-44 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1110 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0xe9696969, @long_dest
	beq	.Lldisp162
	fail
.Lldisp162:
	mov	#0xa5a5a5a5, @long_dest

shar_l_disp32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#long_dest-666, er0
	shar.l	#2, @(666:32, er0)	; shift right arithmetic by two, disp32
;;;	.word	0x7884
;;;	.word	0x6b28
;;; 	.long	666
;;;	.word	0x11f0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  long_dest-666 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1110 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0xe9696969, @long_dest
	beq	.Lldisp322
	fail
.Lldisp322:
	mov	#0xa5a5a5a5, @long_dest

shar_l_abs16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.l	#2, @long_dest:16	; shift right arithmetic by two, abs16
;;;	.word	0x0104
;;;	.word	0x6b08
;;;	.word	long_dest
;;;	.word	0x11f0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1110 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0xe9696969, @long_dest
	beq	.Llabs162
	fail
.Llabs162:
	mov	#0xa5a5a5a5, @long_dest

shar_l_abs32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shar.l	#2, @long_dest:32	; shift right arithmetic by two, abs32
;;;	.word	0x0104
;;;	.word	0x6b28
;;; 	.long	long_dest
;;;	.word	0x11f0

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1110 1001 0110 1001 0110 1001 0110 1001
	cmp.l	#0xe9696969, @long_dest
	beq	.Llabs322
	fail
.Llabs322:
	mov	#0xa5a5a5a5, @long_dest
	
.endif
.endif
	pass

	exit 0

