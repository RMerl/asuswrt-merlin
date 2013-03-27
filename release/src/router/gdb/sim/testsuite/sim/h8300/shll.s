# Hitachi H8 testcase 'shll'
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

shll_b_reg8_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.b	r0l		; shift left logical by one
;;;	.word	0x1008

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	test_h_gr16 0xa54a r0	; 1010 0101 -> 0100 1010
.if (sim_cpu)
	test_h_gr32 0xa5a5a54a er0
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shll_b_reg8_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.b	#2, r0l		; shift left logical by two
;;; 	.word	0x1048

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr16 0xa594 r0	; 1010 0101 -> 1001 0100
.if (sim_cpu)
	test_h_gr32 0xa5a5a594 er0
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shll_b_reg8_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.b	#4, r0l		; shift left logical by four
;;;	.word	0x10a8

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	test_h_gr16 0xa550 r0	; 1010 0101 -> 0101 0000
	test_h_gr32 0xa5a5a550 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shll_b_reg8_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#5, r0h
	shll.b	r0h, r0l	; shift left logical by register value

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set
	test_h_gr16 0x05a0 r0	; 1010 0101 -> 1010 0000
	test_h_gr32 0xa5a505a0 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

.if (sim_cpu)			; Not available in h8300 mode
shll_w_reg16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.w	r0		; shift left logical by one
;;;	.word	0x1010

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	test_h_gr16 0x4b4a r0	; 1010 0101 1010 0101 -> 0100 1011 0100 1010
	test_h_gr32 0xa5a54b4a er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shll_w_reg16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.w	#2, r0		; shift left logical by two
;;;	.word	0x1050

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set
	test_h_gr16 0x9694 r0	; 1010 0101 1010 0101 -> 1001 0110 1001 0100
	test_h_gr32 0xa5a59694 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shll_w_reg16_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.w	#4, r0		; shift left logical by four
;;;	.word	0x1020

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	test_h_gr16 0x5a50 r0	; 1010 0101 1010 0101 -> 0101 1010 0101 0000
	test_h_gr32 0xa5a55a50 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shll_w_reg16_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.w	#8, r0		; shift left logical by eight
;;;	.word	0x1060

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set
	test_h_gr16 0xa500 r0	; 1010 0101 1010 0101 -> 1010 0101 0000 0000
	test_h_gr32 0xa5a5a500 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shll_w_reg16_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#5, r0h
	shll.w	r0h, r0		; shift left logical by register value

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set
	test_h_gr16 0xb4a0 r0	; 1010 0101 1010 0101 -> 1011 0100 1010 0000
	test_h_gr32 0xa5a5b4a0 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

shll_l_reg32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.l	er0		; shift left logical by one
;;;	.word	1030

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 0100 1011 0100 1011 0100 1011 0100 1010
	test_h_gr32 0x4b4b4b4a er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shll_l_reg32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.l	#2, er0		; shift left logical by two
;;;	.word	0x1070

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set
	; 1010 0101 1010 0101 1010 0101 1010 0101
	; -> 1001 0110 1001 0110 1001 0110 1001 0100
	test_h_gr32 0x96969694 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
shll_l_reg32_4:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.l	#4, er0		; shift left logical by four
;;;	.word	0x1038

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_clear
	; 1010 0101 1010 0101 1010 0101 1010 0101
	; -> 0101 1010 0101 1010 0101 1010 0101 0000
	test_h_gr32 0x5a5a5a50 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shll_l_reg32_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.l	#8, er0		; shift left logical by eight
;;;	.word	0x1078

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set
	test_h_gr16 0xa500 r0	
	; 1010 0101 1010 0101 1010 0101 1010 0101
	; -> 1010 0101 1010 0101 1010 0101 0000 0000
	test_h_gr32 0xa5a5a500 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shll_l_reg32_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shll.l	#16, er0	; shift left logical by sixteen
;;;	.word	0x10f8

	test_carry_set		; H=0 N=1 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_set
	; 1010 0101 1010 0101 1010 0101 1010 0101
	;; -> 1010 0101 1010 0101 0000 0000 0000 0000
	test_h_gr32 0xa5a50000 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

shll_l_reg32_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#5, r1l
	shll.l	r1l, er0	; shift left logical by register value

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set
	; 1010 0101 1010 0101 1010 0101 1010 0101
	; -> 1011 0100 1011 0100 1011 0100 1010 0000
	test_h_gr32  0xb4b4b4a0 er0

	test_h_gr32  0xa5a5a505 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif
.endif

	pass

	exit 0

