# Hitachi H8 testcase 'mov.l'
# mach(): h8300h h8300s h8sx
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf	

	.include "testutils.inc"

	start

	.data
	.align	4
long_src:
	.long	0x77777777
long_dst:
	.long	0

	.text

	;;
	;; Move long from immediate source
	;; 

.if (sim_cpu == h8sx)
mov_l_imm3_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:3, erd
	mov.l	#0x3:3, er0	; Immediate 3-bit operand
;;;	.word	0x0fb8

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x3 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_l_imm16_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, erd
	mov.l	#0x1234, er0	; Immediate 16-bit operand
;;;	.word	0x7a08
;;;	.word	0x1234

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x1234 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

mov_l_imm32_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:32, erd
	mov.l	#0x12345678, er0	; Immediate 32-bit operand
;;;	.word	0x7a00
;;;	.long	0x12345678

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x12345678 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
mov_l_imm8_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:8, @erd
	mov.l	#long_dst, er1
	mov.l	#0xa5:8, @er1	; Register indirect operand
;;;	.word	0x010d
;;;	.word	0x01a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xa5, @long_dst
	beq	.Lnext1
	fail
.Lnext1:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm8_to_postinc:		; post-increment from imm8 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:8, @erd+
	mov.l	#long_dst, er1
	mov.l	#0xa5:8, @er1+	; Imm8, register post-incr operands.
;;;	.word	0x010d
;;;	.word	0x81a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst+4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xa5, @long_dst
	beq	.Lnext2
	fail
.Lnext2:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm8_to_postdec:		; post-decrement from imm8 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:8, @erd-
	mov.l	#long_dst, er1
	mov.l	#0xa5:8, @er1-	; Imm8, register post-decr operands.
;;;	.word	0x010d
;;;	.word	0xa1a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xa5, @long_dst
	beq	.Lnext3
	fail
.Lnext3:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm8_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:8, @+erd
	mov.l	#long_dst-4, er1
	mov.l	#0xa5:8, @+er1	; Imm8, register pre-incr operands
;;;	.word	0x010d
;;;	.word	0x91a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xa5, @long_dst
	beq	.Lnext4
	fail
.Lnext4:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm8_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:8, @-erd
	mov.l	#long_dst+4, er1
	mov.l	#0xa5:8, @-er1	; Imm8, register pre-decr operands
;;;	.word	0x010d
;;;	.word	0xb1a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xa5, @long_dst
	beq	.Lnext5
	fail
.Lnext5:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm8_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:8, @(dd:2, erd)
	mov.l	#long_dst-12, er1
	mov.l	#0xa5:8, @(12:2, er1)	; Imm8, reg plus 2-bit disp. operand
;;;	.word	0x010d
;;;	.word	0x31a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-12, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xa5, @long_dst
	beq	.Lnext6
	fail
.Lnext6:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm8_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:8, @(dd:16, erd)
	mov.l	#long_dst-4, er1
	mov.l	#0xa5:8, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x010d
;;;	.word	0x6f90
;;;	.word	0x0004

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xa5, @long_dst
	beq	.Lnext7
	fail
.Lnext7:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm8_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:8, @(dd:32, erd)
	mov.l	#long_dst-8, er1
	mov.l	#0xa5:8, @(8:32, er1)	; Register plus 32-bit disp. operand
;;;	.word	0x010d
;;;	.word	0xc9a5
;;;	.long	8

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-8, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xa5, @long_dst
	beq	.Lnext8
	fail
.Lnext8:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm8_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:8, @aa:16
	mov.l	#0xa5:8, @long_dst:16	; 16-bit address-direct operand
;;;	.word	0x010d
;;;	.word	0x40a5
;;;	.word	@long_dst

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xa5, @long_dst
	beq	.Lnext9
	fail
.Lnext9:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm8_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:8, @aa:32
	mov.l	#0xa5:8, @long_dst:32	; 32-bit address-direct operand
;;;	.word	0x010d
;;;	.word	0x48a5
;;;	.long	@long_dst

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xa5, @long_dst
	beq	.Lnext10
	fail
.Lnext10:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm16_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, @erd
	mov.l	#long_dst, er1
	mov.l	#0xdead:16, @er1	; Register indirect operand
;;;	.word	0x7a7c
;;;	.word	0xdead
;;;	.word	0x0100

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xdead, @long_dst
	beq	.Lnext11
	fail
.Lnext11:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm16_to_postinc:		; post-increment from imm16 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, @erd+
	mov.l	#long_dst, er1
	mov.l	#0xdead:16, @er1+	; Imm16, register post-incr operands.
;;;	.word	0x7a7c
;;;	.word	0xdead
;;;	.word	0x8100

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst+4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xdead, @long_dst
	beq	.Lnext12
	fail
.Lnext12:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm16_to_postdec:		; post-decrement from imm16 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, @erd-
	mov.l	#long_dst, er1
	mov.l	#0xdead:16, @er1-	; Imm16, register post-decr operands.
;;;	.word	0x7a7c
;;;	.word	0xdead
;;;	.word	0xa100

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xdead, @long_dst
	beq	.Lnext13
	fail
.Lnext13:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm16_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, @+erd
	mov.l	#long_dst-4, er1
	mov.l	#0xdead:16, @+er1	; Imm16, register pre-incr operands
;;;	.word	0x7a7c
;;;	.word	0xdead
;;;	.word	0x9100

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xdead, @long_dst
	beq	.Lnext14
	fail
.Lnext14:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm16_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, @-erd
	mov.l	#long_dst+4, er1
	mov.l	#0xdead:16, @-er1	; Imm16, register pre-decr operands
;;;	.word	0x7a7c
;;;	.word	0xdead
;;;	.word	0xb100

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xdead, @long_dst
	beq	.Lnext15
	fail
.Lnext15:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm16_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, @(dd:2, erd)
	mov.l	#long_dst-12, er1
	mov.l	#0xdead:16, @(12:2, er1)	; Imm16, reg plus 2-bit disp. operand
;;;	.word	0x7a7c
;;;	.word	0xdead
;;;	.word	0x3100

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-12, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xdead, @long_dst
	beq	.Lnext16
	fail
.Lnext16:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm16_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, @(dd:16, erd)
	mov.l	#long_dst-4, er1
	mov.l	#0xdead:16, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x7a7c
;;;	.word	0xdead
;;;	.word	0xc100
;;;	.word	0x0004

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xdead, @long_dst
	beq	.Lnext17
	fail
.Lnext17:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm16_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, @(dd:32, erd)
	mov.l	#long_dst-8, er1
	mov.l	#0xdead:16, @(8:32, er1)   ; Register plus 32-bit disp. operand
;;;	.word	0x7a7c
;;;	.word	0xdead
;;;	.word	0xc900
;;;	.long	8

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-8, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xdead, @long_dst
	beq	.Lnext18
	fail
.Lnext18:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm16_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, @aa:16
	mov.l	#0xdead:16, @long_dst:16	; 16-bit address-direct operand
;;;	.word	0x7a7c
;;;	.word	0xdead
;;;	.word	0x4000
;;;	.word	@long_dst

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xdead, @long_dst
	beq	.Lnext19
	fail
.Lnext19:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm16_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:16, @aa:32
	mov.l	#0xdead:16, @long_dst:32	; 32-bit address-direct operand
;;;	.word	0x7a7c
;;;	.word	0xdead
;;;	.word	0x4800
;;;	.long	@long_dst

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xdead, @long_dst
	beq	.Lnext20
	fail
.Lnext20:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm32_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:32, @erd
	mov.l	#long_dst, er1
	mov.l	#0xcafedead:32, @er1	; Register indirect operand
;;;	.word	0x7a74
;;;	.long	0xcafedead
;;;	.word	0x0100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xcafedead, @long_dst
	beq	.Lnext21
	fail
.Lnext21:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm32_to_postinc:		; post-increment from imm32 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:32, @erd+
	mov.l	#long_dst, er1
	mov.l	#0xcafedead:32, @er1+	; Imm32, register post-incr operands.
;;;	.word	0x7a74
;;;	.long	0xcafedead
;;;	.word	0x8100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst+4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xcafedead, @long_dst
	beq	.Lnext22
	fail
.Lnext22:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm32_to_postdec:		; post-decrement from imm32 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:32, @erd-
	mov.l	#long_dst, er1
	mov.l	#0xcafedead:32, @er1-	; Imm32, register post-decr operands.
;;;	.word	0x7a74
;;;	.long	0xcafedead
;;;	.word	0xa100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xcafedead, @long_dst
	beq	.Lnext23
	fail
.Lnext23:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm32_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:32, @+erd
	mov.l	#long_dst-4, er1
	mov.l	#0xcafedead:32, @+er1	; Imm32, register pre-incr operands
;;;	.word	0x7a74
;;;	.long	0xcafedead
;;;	.word	0x9100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xcafedead, @long_dst
	beq	.Lnext24
	fail
.Lnext24:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm32_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:32, @-erd
	mov.l	#long_dst+4, er1
	mov.l	#0xcafedead:32, @-er1	; Imm32, register pre-decr operands
;;;	.word	0x7a74
;;;	.long	0xcafedead
;;;	.word	0xb100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xcafedead, @long_dst
	beq	.Lnext25
	fail
.Lnext25:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm32_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:32, @(dd:2, erd)
	mov.l	#long_dst-12, er1
	mov.l	#0xcafedead:32, @(12:2, er1)	; Imm32, reg plus 2-bit disp. operand
;;;	.word	0x7a74
;;;	.long	0xcafedead
;;;	.word	0x3100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-12, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xcafedead, @long_dst
	beq	.Lnext26
	fail
.Lnext26:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm32_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:32, @(dd:16, erd)
	mov.l	#long_dst-4, er1
	mov.l	#0xcafedead:32, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x7a74
;;;	.long	0xcafedead
;;;	.word	0xc100
;;;	.word	0x0004

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xcafedead, @long_dst
	beq	.Lnext27
	fail
.Lnext27:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm32_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:32, @(dd:32, erd)
	mov.l	#long_dst-8, er1
	mov.l	#0xcafedead:32, @(8:32, er1)   ; Register plus 32-bit disp. operand
;;;	.word	0x7a74
;;;	.long	0xcafedead
;;;	.word	0xc900
;;;	.long	8

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-8, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xcafedead, @long_dst
	beq	.Lnext28
	fail
.Lnext28:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm32_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l #xx:32, @aa:16
	mov.l	#0xcafedead:32, @long_dst:16	; 16-bit address-direct operand
;;;	.word	0x7a74
;;;	.long	0xcafedead
;;;	.word	0x4000
;;;	.word	@long_dst

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xcafedead, @long_dst
	beq	.Lnext29
	fail
.Lnext29:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_imm32_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  mov.l #xx:32, @aa:32
	mov.l	#0xcafedead:32, @long_dst:32	; 32-bit address-direct operand
;;;	.word	0x7a74
;;;	.long	0xcafedead
;;;	.word	0x4800
;;;	.long	@long_dst

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0xcafedead, @long_dst
	beq	.Lnext30
	fail
.Lnext30:
	mov.l	#0, @long_dst	; zero it again for the next use.

.endif

	;;
	;; Move long from register source
	;; 

mov_l_reg32_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, erd
	mov.l	#0x12345678, er1
	mov.l	er1, er0	; Register 32-bit operand
;;;	.word	0x0f90

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear
	test_h_gr32 0x12345678 er0
	test_h_gr32 0x12345678 er1	; mov src unchanged

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_l_reg32_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, @erd
	mov.l	#long_dst, er1
	mov.l	er0, @er1	; Register indirect operand
;;;	.word	0x0100
;;;	.word	0x6990

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.l	#0, er0
	mov.l	@long_dst, er0
	cmp.l	er2, er0
	beq	.Lnext44
	fail
.Lnext44:
	mov.l	#0, er0
	mov.l	er0, @long_dst	; zero it again for the next use.

.if (sim_cpu == h8sx)
mov_l_reg32_to_postinc:		; post-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, @erd+
	mov.l	#long_dst, er1
	mov.l	er0, @er1+	; Register post-incr operand
;;;	.word	0x0103
;;;	.word	0x6d90

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst+4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	er2, @long_dst
	beq	.Lnext49
	fail
.Lnext49:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_reg32_to_postdec:		; post-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, @erd-
	mov.l	#long_dst, er1
	mov.l	er0, @er1-	; Register post-decr operand
;;;	.word	0x0101
;;;	.word	0x6d90

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	er2, @long_dst
	beq	.Lnext50
	fail
.Lnext50:
	mov.l	#0, @long_dst	; zero it again for the next use.

mov_l_reg32_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, @+erd
	mov.l	#long_dst-4, er1
	mov.l	er0, @+er1	; Register pre-incr operand
;;;	.word	0x0102
;;;	.word	0x6d90

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	er2, @long_dst
	beq	.Lnext51
	fail
.Lnext51:
	mov.l	#0, @long_dst	; zero it again for the next use.
.endif				; h8sx

mov_l_reg32_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, @-erd
	mov.l	#long_dst+4, er1
	mov.l	er0, @-er1	; Register pre-decr operand
;;;	.word	0x0100
;;;	.word	0x6d90

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.l	#0, er0
	mov.l	@long_dst, er0
	cmp.l	er2, er0
	beq	.Lnext48
	fail
.Lnext48:
	mov.l	#0, er0
	mov.l	er0, @long_dst	; zero it again for the next use.

.if (sim_cpu == h8sx)
mov_l_reg32_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, @(dd:2, erd)
	mov.l	#long_dst-12, er1
	mov.l	er0, @(12:2, er1)	; Register plus 2-bit disp. operand
;;;	.word	0x0103
;;;	.word	0x6990

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-12, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	er2, @long_dst
	beq	.Lnext52
	fail
.Lnext52:
	mov.l	#0, @long_dst	; zero it again for the next use.
.endif				; h8sx

mov_l_reg32_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, @(dd:16, erd)
	mov.l	#long_dst-4, er1
	mov.l	er0, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x0100
;;;	.word	0x6f90
;;;	.word	0x0004

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.l	#0, er0
	mov.l	@long_dst, er0
	cmp.l	er2, er0
	beq	.Lnext45
	fail
.Lnext45:
	mov.l	#0, er0
	mov.l	er0, @long_dst	; zero it again for the next use.

mov_l_reg32_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, @(dd:32, erd)
	mov.l	#long_dst-8, er1
	mov.l	er0, @(8:32, er1)	; Register plus 32-bit disp. operand
;;;	.word	0x7890
;;;	.word	0x6ba0
;;;	.long	8

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	long_dst-8, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.l	#0, er0
	mov.l	@long_dst, er0
	cmp.l	er2, er0
	beq	.Lnext46
	fail
.Lnext46:
	mov.l	#0, er0
	mov.l	er0, @long_dst	; zero it again for the next use.

mov_l_reg32_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, @aa:16
	mov.l	er0, @long_dst:16	; 16-bit address-direct operand
;;;	.word	0x0100
;;;	.word	0x6b80
;;;	.word	@long_dst

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.l	#0, er0
	mov.l	@long_dst, er0
	cmp.l	er0, er1
	beq	.Lnext41
	fail
.Lnext41:
	mov.l	#0, er0
	mov.l	er0, @long_dst	; zero it again for the next use.

mov_l_reg32_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l ers, @aa:32
	mov.l	er0, @long_dst:32	; 32-bit address-direct operand
;;;	.word	0x0100
;;;	.word	0x6ba0
;;;	.long	@long_dst

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.l	#0, er0
	mov.l	@long_dst, er0
	cmp.l	er0, er1
	beq	.Lnext42
	fail
.Lnext42:
	mov.l	#0, er0
	mov.l	er0, @long_dst	; zero it again for the next use.

	;;
	;; Move long to register destination.
	;; 

mov_l_indirect_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @ers, erd
	mov.l	#long_src, er1
	mov.l	@er1, er0	; Register indirect operand
;;;	.word	0x0100
;;;	.word	0x6910

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x77777777 er0

	test_h_gr32	long_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_l_postinc_to_reg32:		; post-increment from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @ers+, erd

	mov.l	#long_src, er1
	mov.l	@er1+, er0	; Register post-incr operand
;;;	.word	0x0100
;;;	.word	0x6d10

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x77777777 er0

	test_h_gr32	long_src+4, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
mov_l_postdec_to_reg32:		; post-decrement from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @ers-, erd

	mov.l	#long_src, er1
	mov.l	@er1-, er0	; Register post-decr operand
;;;	.word	0x0102
;;;	.word	0x6d10

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x77777777 er0

	test_h_gr32	long_src-4, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_l_preinc_to_reg32:		; pre-increment from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @+ers, erd

	mov.l	#long_src-4, er1
	mov.l	@+er1, er0	; Register pre-incr operand
;;;	.word	0x0101
;;;	.word	0x6d10

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x77777777 er0

	test_h_gr32	long_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_l_predec_to_reg32:		; pre-decrement from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @-ers, erd

	mov.l	#long_src+4, er1
	mov.l	@-er1, er0	; Register pre-decr operand
;;;	.word	0x0103
;;;	.word	0x6d10

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x77777777 er0

	test_h_gr32	long_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	
mov_l_disp2_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @(dd:2, ers), erd
	mov.l	#long_src-4, er1
	mov.l	@(4:2, er1), er0	; Register plus 2-bit disp. operand
;;; 	.word	0x0101
;;; 	.word	0x6910

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x77777777 er0	; mov result:	a5a5 | 7777

	test_h_gr32	long_src-4, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif				; h8sx

mov_l_disp16_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @(dd:16, ers), erd
	mov.l	#long_src+0x1234, er1
	mov.l	@(-0x1234:16, er1), er0	; Register plus 16-bit disp. operand
;;;	.word	0x0100
;;;	.word	0x6f10
;;;	.word	-0x1234

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x77777777 er0	; mov result:	a5a5 | 7777

	test_h_gr32	long_src+0x1234, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_l_disp32_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @(dd:32, ers), erd
	mov.l	#long_src+65536, er1
	mov.l	@(-65536:32, er1), er0	; Register plus 32-bit disp. operand
;;;	.word	0x7890
;;;	.word	0x6b20
;;;	.long	-65536

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x77777777 er0	; mov result:	a5a5 | 7777

	test_h_gr32	long_src+65536, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_l_abs16_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @aa:16, erd
	mov.l	@long_src:16, er0	; 16-bit address-direct operand
;;;	.word	0x0100
;;;	.word	0x6b00
;;;	.word	@long_src

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x77777777 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_l_abs32_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @aa:32, erd
	mov.l	@long_src:32, er0	; 32-bit address-direct operand
;;;	.word	0x0100
;;;	.word	0x6b20
;;;	.long	@long_src

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0x77777777 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7


.if (sim_cpu == h8sx)

	;;
	;; Move long from memory to memory
	;; 

mov_l_indirect_to_indirect:	; reg indirect, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @ers, @erd

	mov.l	#long_src, er1
	mov.l	#long_dst, er0
	mov.l	@er1, @er0
;;;	.word	0x0108
;;;	.word	0x0100

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  long_dst er0
	test_h_gr32  long_src er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	@long_src, @long_dst
	beq	.Lnext55
	fail
.Lnext55:
	;; Now clear the destination location, and verify that.
	mov.l	#0, @long_dst
	cmp.l	@long_src, @long_dst
	bne	.Lnext56
	fail
.Lnext56:			; OK, pass on.

mov_l_postinc_to_postinc:	; reg post-increment, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @ers+, @erd+

	mov.l	#long_src, er1
	mov.l	#long_dst, er0
	mov.l	@er1+, @er0+
;;;	.word	0x0108
;;;	.word	0x8180

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  long_dst+4 er0
	test_h_gr32  long_src+4 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	@long_src, @long_dst
	beq	.Lnext65
	fail
.Lnext65:
	;; Now clear the destination location, and verify that.
	mov.l	#0, @long_dst
	cmp.l	@long_src, @long_dst
	bne	.Lnext66
	fail
.Lnext66:			; OK, pass on.

mov_l_postdec_to_postdec:	; reg post-decrement, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @ers-, @erd-

	mov.l	#long_src, er1
	mov.l	#long_dst, er0
	mov.l	@er1-, @er0-
;;;	.word	0x0108
;;;	.word	0xa1a0

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  long_dst-4 er0
	test_h_gr32  long_src-4 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	@long_src, @long_dst
	beq	.Lnext75
	fail
.Lnext75:
	;; Now clear the destination location, and verify that.
	mov.l	#0, @long_dst
	cmp.l	@long_src, @long_dst
	bne	.Lnext76
	fail
.Lnext76:			; OK, pass on.

mov_l_preinc_to_preinc:		; reg pre-increment, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @+ers, @+erd

	mov.l	#long_src-4, er1
	mov.l	#long_dst-4, er0
	mov.l	@+er1, @+er0
;;;	.word	0x0108
;;;	.word	0x9190

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  long_dst er0
	test_h_gr32  long_src er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	@long_src, @long_dst
	beq	.Lnext85
	fail
.Lnext85:
	;; Now clear the destination location, and verify that.
	mov.l	#0, @long_dst
	cmp.l	@long_src, @long_dst
	bne	.Lnext86
	fail
.Lnext86:				; OK, pass on.

mov_l_predec_to_predec:		; reg pre-decrement, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @-ers, @-erd

	mov.l	#long_src+4, er1
	mov.l	#long_dst+4, er0
	mov.l	@-er1, @-er0
;;;	.word	0x0108
;;;	.word	0xb1b0

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  long_dst er0
	test_h_gr32  long_src er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	@long_src, @long_dst
	beq	.Lnext95
	fail
.Lnext95:
	;; Now clear the destination location, and verify that.
	mov.l	#0, @long_dst
	cmp.l	@long_src, @long_dst
	bne	.Lnext96
	fail
.Lnext96:			; OK, pass on.

mov_l_disp2_to_disp2:		; reg 2-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @(dd:2, ers), @(dd:2, erd)

	mov.l	#long_src-4, er1
	mov.l	#long_dst-8, er0
	mov.l	@(4:2, er1), @(8:2, er0)
;;; 	.word	0x0108
;;; 	.word	0x1120

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  long_dst-8 er0
	test_h_gr32  long_src-4 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	@long_src, @long_dst
	beq	.Lnext105
	fail
.Lnext105:
	;; Now clear the destination location, and verify that.
	mov.l	#0, @long_dst
	cmp.l	@long_src, @long_dst
	bne	.Lnext106
	fail
.Lnext106:			; OK, pass on.

mov_l_disp16_to_disp16:		; reg 16-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @(dd:16, ers), @(dd:16, erd)

	mov.l	#long_src-1, er1
	mov.l	#long_dst-2, er0
	mov.l	@(1:16, er1), @(2:16, er0)
;;; 	.word	0x0108
;;; 	.word	0xc1c0
;;; 	.word	0x0001
;;; 	.word	0x0002

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  long_dst-2 er0
	test_h_gr32  long_src-1 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	@long_src, @long_dst
	beq	.Lnext115
	fail
.Lnext115:
	;; Now clear the destination location, and verify that.
	mov.l	#0, @long_dst
	cmp.l	@long_src, @long_dst
	bne	.Lnext116
	fail
.Lnext116:			; OK, pass on.

mov_l_disp32_to_disp32:		; reg 32-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @(dd:32, ers), @(dd:32, erd)

	mov.l	#long_src-1, er1
	mov.l	#long_dst-2, er0
	mov.l	@(1:32, er1), @(2:32, er0)
;;; 	.word	0x0108
;;; 	.word	0xc9c8
;;;	.long	1
;;;	.long	2

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  long_dst-2 er0
	test_h_gr32  long_src-1 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	@long_src, @long_dst
	beq	.Lnext125
	fail
.Lnext125:
	;; Now clear the destination location, and verify that.
	mov.l	#0, @long_dst
	cmp.l	@long_src, @long_dst
	bne	.Lnext126
	fail
.Lnext126:				; OK, pass on.

mov_l_abs16_to_abs16:		; 16-bit absolute addr, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @aa:16, @aa:16

	mov.l	@long_src:16, @long_dst:16
;;; 	.word	0x0108
;;; 	.word	0x4040
;;;	.word	@long_src
;;;	.word	@long_dst

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear


	test_gr_a5a5 0		; Make sure *NO* general registers are changed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	@long_src, @long_dst
	beq	.Lnext135
	fail
.Lnext135:
	;; Now clear the destination location, and verify that.
	mov.l	#0, @long_dst
	cmp.l	@long_src, @long_dst
	bne	.Lnext136
	fail
.Lnext136:				; OK, pass on.

mov_l_abs32_to_abs32:		; 32-bit absolute addr, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.l @aa:32, @aa:32

	mov.l	@long_src:32, @long_dst:32
;;; 	.word	0x0108
;;; 	.word	0x4848
;;;	.long	@long_src
;;;	.long	@long_dst

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure *NO* general registers are changed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	@long_src, @long_dst
	beq	.Lnext145
	fail
.Lnext145:
	;; Now clear the destination location, and verify that.
	mov.l	#0, @long_dst
	cmp.l	@long_src, @long_dst
	bne	.Lnext146
	fail
.Lnext146:				; OK, pass on.


.endif

	pass

	exit 0
