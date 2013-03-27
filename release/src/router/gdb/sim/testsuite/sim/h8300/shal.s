# Hitachi H8 testcase 'shal'
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

shal_b_reg8_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shal.b	r0l		; shift left arithmetic by one
;;;	.word	0x1088

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
;	test_ovf_clear		; FIXME
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

shal_b_reg8_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shal.b	#2, r0l		; shift left arithmetic by two
;;; 	.word	0x10c8

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
;	test_ovf_clear		; FIXME
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

.if (sim_cpu)			; Not available in h8300 mode
shal_w_reg16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shal.w	r0		; shift left arithmetic by one
;;;	.word	0x1090

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
;	test_ovf_clear		; FIXME
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

shal_w_reg16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shal.w	#2, r0		; shift left arithmetic by two
;;;	.word	0x10d0

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
;	test_ovf_clear		; FIXME
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

shal_l_reg32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shal.l	er0		; shift left arithmetic by one
;;;	.word	10b0

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
;	test_ovf_clear		; FIXME
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

shal_l_reg32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	shal.l	#2, er0		; shift left arithmetic by two
;;;	.word	0x10f0

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
;	test_ovf_clear		; FIXME
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

.endif

	pass

	exit 0

