# Hitachi H8 testcase 'mov.w'
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
	.align	2
word_src:
	.word	0x7777
word_dst:
	.word	0

	.text

	;;
	;; Move word from immediate source
	;; 

.if (sim_cpu == h8sx)
mov_w_imm3_to_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:3, rd
	mov.w	#0x3:3, r0	; Immediate 3-bit operand
;;;	.word	0x0f30

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a50003 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

mov_w_imm16_to_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, rd
	mov.w	#0x1234, r0	; Immediate 16-bit operand
;;;	.word	0x7900
;;;	.word	0x1234

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a51234 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
mov_w_imm4_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:4, @aa:16
	mov.w	#0xf:4, @word_dst:16	; 4-bit imm to 16-bit address-direct 
;;;	.word	0x6bdf
;;;	.word	@word_dst

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
	cmp.w	#0xf, @word_dst
	beq	.Lnext21
	fail
.Lnext21:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm4_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:4, @aa:32
	mov.w	#0xf:4, @word_dst:32	; 4-bit imm to 32-bit address-direct 
;;;	.word	0x6bff
;;;	.long	@word_dst

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
	cmp.w	#0xf, @word_dst
	beq	.Lnext22
	fail
.Lnext22:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm8_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:8, @erd
	mov.l	#word_dst, er1
	mov.w	#0xa5:8, @er1	; Register indirect operand
;;;	.word	0x015d
;;;	.word	0x01a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xa5, @word_dst
	beq	.Lnext1
	fail
.Lnext1:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm8_to_postinc:		; post-increment from imm8 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:8, @erd+
	mov.l	#word_dst, er1
	mov.w	#0xa5:8, @er1+	; Imm8, register post-incr operands.
;;;	.word	0x015d
;;;	.word	0x81a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst+2, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xa5, @word_dst
	beq	.Lnext2
	fail
.Lnext2:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm8_to_postdec:		; post-decrement from imm8 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:8, @erd-
	mov.l	#word_dst, er1
	mov.w	#0xa5:8, @er1-	; Imm8, register post-decr operands.
;;;	.word	0x015d
;;;	.word	0xa1a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst-2, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xa5, @word_dst
	beq	.Lnext3
	fail
.Lnext3:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm8_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:8, @+erd
	mov.l	#word_dst-2, er1
	mov.w	#0xa5:8, @+er1	; Imm8, register pre-incr operands
;;;	.word	0x015d
;;;	.word	0x91a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xa5, @word_dst
	beq	.Lnext4
	fail
.Lnext4:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm8_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:8, @-erd
	mov.l	#word_dst+2, er1
	mov.w	#0xa5:8, @-er1	; Imm8, register pre-decr operands
;;;	.word	0x015d
;;;	.word	0xb1a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xa5, @word_dst
	beq	.Lnext5
	fail
.Lnext5:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm8_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:8, @(dd:2, erd)
	mov.l	#word_dst-6, er1
	mov.w	#0xa5:8, @(6:2, er1)	; Imm8, reg plus 2-bit disp. operand
;;;	.word	0x015d
;;;	.word	0x31a5

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst-6, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xa5, @word_dst
	beq	.Lnext6
	fail
.Lnext6:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm8_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:8, @(dd:16, erd)
	mov.l	#word_dst-4, er1
	mov.w	#0xa5:8, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x015d
;;;	.word	0x6f90
;;;	.word	0x0004

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xa5, @word_dst
	beq	.Lnext7
	fail
.Lnext7:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm8_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:8, @(dd:32, erd)
	mov.l	#word_dst-8, er1
	mov.w	#0xa5:8, @(8:32, er1)	; Register plus 32-bit disp. operand
;;;	.word	0x015d
;;;	.word	0xc9a5
;;;	.long	8

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst-8, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xa5, @word_dst
	beq	.Lnext8
	fail
.Lnext8:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm8_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:8, @aa:16
	mov.w	#0xa5:8, @word_dst:16	; 16-bit address-direct operand
;;;	.word	0x015d
;;;	.word	0x40a5
;;;	.word	@word_dst

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
	cmp.w	#0xa5, @word_dst
	beq	.Lnext9
	fail
.Lnext9:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm8_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:8, @aa:32
	mov.w	#0xa5:8, @word_dst:32	; 32-bit address-direct operand
;;;	.word	0x015d
;;;	.word	0x48a5
;;;	.long	@word_dst

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
	cmp.w	#0xa5, @word_dst
	beq	.Lnext10
	fail
.Lnext10:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm16_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, @erd
	mov.l	#word_dst, er1
	mov.w	#0xdead:16, @er1	; Register indirect operand
;;;	.word	0x7974
;;;	.word	0xdead
;;;	.word	0x0100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xdead, @word_dst
	beq	.Lnext11
	fail
.Lnext11:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm16_to_postinc:		; post-increment from imm16 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, @erd+
	mov.l	#word_dst, er1
	mov.w	#0xdead:16, @er1+	; Imm16, register post-incr operands.
;;;	.word	0x7974
;;;	.word	0xdead
;;;	.word	0x8100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst+2, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xdead, @word_dst
	beq	.Lnext12
	fail
.Lnext12:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm16_to_postdec:		; post-decrement from imm16 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, @erd-
	mov.l	#word_dst, er1
	mov.w	#0xdead:16, @er1-	; Imm16, register post-decr operands.
;;;	.word	0x7974
;;;	.word	0xdead
;;;	.word	0xa100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst-2, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xdead, @word_dst
	beq	.Lnext13
	fail
.Lnext13:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm16_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, @+erd
	mov.l	#word_dst-2, er1
	mov.w	#0xdead:16, @+er1	; Imm16, register pre-incr operands
;;;	.word	0x7974
;;;	.word	0xdead
;;;	.word	0x9100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xdead, @word_dst
	beq	.Lnext14
	fail
.Lnext14:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm16_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, @-erd
	mov.l	#word_dst+2, er1
	mov.w	#0xdead:16, @-er1	; Imm16, register pre-decr operands
;;;	.word	0x7974
;;;	.word	0xdead
;;;	.word	0xb100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xdead, @word_dst
	beq	.Lnext15
	fail
.Lnext15:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm16_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, @(dd:2, erd)
	mov.l	#word_dst-6, er1
	mov.w	#0xdead:16, @(6:2, er1)	; Imm16, reg plus 2-bit disp. operand
;;;	.word	0x7974
;;;	.word	0xdead
;;;	.word	0x3100

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst-6, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xdead, @word_dst
	beq	.Lnext16
	fail
.Lnext16:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm16_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, @(dd:16, erd)
	mov.l	#word_dst-4, er1
	mov.w	#0xdead:16, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x7974
;;;	.word	0xdead
;;;	.word	0xc100
;;;	.word	0x0004

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xdead, @word_dst
	beq	.Lnext17
	fail
.Lnext17:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm16_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, @(dd:32, erd)
	mov.l	#word_dst-8, er1
	mov.w	#0xdead:16, @(8:32, er1)   ; Register plus 32-bit disp. operand
;;;	.word	0x7974
;;;	.word	0xdead
;;;	.word	0xc900
;;;	.long	8

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst-8, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	#0xdead, @word_dst
	beq	.Lnext18
	fail
.Lnext18:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm16_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, @aa:16
	mov.w	#0xdead:16, @word_dst:16	; 16-bit address-direct operand
;;;	.word	0x7974
;;;	.word	0xdead
;;;	.word	0x4000
;;;	.word	@word_dst

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
	cmp.w	#0xdead, @word_dst
	beq	.Lnext19
	fail
.Lnext19:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_imm16_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w #xx:16, @aa:32
	mov.w	#0xdead:16, @word_dst:32	; 32-bit address-direct operand
;;;	.word	0x7974
;;;	.word	0xdead
;;;	.word	0x4800
;;;	.long	@word_dst

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
	cmp.w	#0xdead, @word_dst
	beq	.Lnext20
	fail
.Lnext20:
	mov.w	#0, @word_dst	; zero it again for the next use.
.endif

	;;
	;; Move word from register source
	;; 

mov_w_reg16_to_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, erd
	mov.w	#0x1234, r1
	mov.w	r1, r0		; Register 16-bit operand
;;;	.word	0x0d10

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear
	test_h_gr16 0x1234 r0
	test_h_gr16 0x1234 r1	; mov src unchanged
.if (sim_cpu)
	test_h_gr32 0xa5a51234 er0
	test_h_gr32 0xa5a51234 er1	; mov src unchanged
.endif
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7


mov_w_reg16_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, @erd
	mov.l	#word_dst, er1
	mov.w	r0, @er1	; Register indirect operand
;;;	.word	0x6990

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.w	#0, r0
	mov.w	@word_dst, r0
	cmp.w	r2, r0
	beq	.Lnext44
	fail
.Lnext44:
	mov.w	#0, r0
	mov.w	r0, @word_dst	; zero it again for the next use.

.if (sim_cpu == h8sx)
mov_w_reg16_to_postinc:		; post-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, @erd+
	mov.l	#word_dst, er1
	mov.w	r0, @er1+	; Register post-incr operand
;;;	.word	0x0153
;;;	.word	0x6d90

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst+2, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	r2, @word_dst
	beq	.Lnext49
	fail
.Lnext49:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_reg16_to_postdec:		; post-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, @erd-
	mov.l	#word_dst, er1
	mov.w	r0, @er1-	; Register post-decr operand
;;;	.word	0x0151
;;;	.word	0x6d90

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst-2, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	r2, @word_dst
	beq	.Lnext50
	fail
.Lnext50:
	mov.w	#0, @word_dst	; zero it again for the next use.

mov_w_reg16_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, @+erd
	mov.l	#word_dst-2, er1
	mov.w	r0, @+er1	; Register pre-incr operand
;;;	.word	0x0152
;;;	.word	0x6d90

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	r2, @word_dst
	beq	.Lnext51
	fail
.Lnext51:
	mov.w	#0, @word_dst	; zero it again for the next use.
.endif

mov_w_reg16_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, @-erd
	mov.l	#word_dst+2, er1
	mov.w	r0, @-er1	; Register pre-decr operand
;;;	.word	0x6d90

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.w	#0, r0
	mov.w	@word_dst, r0
	cmp.w	r2, r0
	beq	.Lnext48
	fail
.Lnext48:
	mov.w	#0, r0
	mov.w	r0, @word_dst	; zero it again for the next use.

.if (sim_cpu == h8sx)
mov_w_reg16_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, @(dd:2, erd)
	mov.l	#word_dst-6, er1
	mov.w	r0, @(6:2, er1)	; Register plus 2-bit disp. operand
;;;	.word	0x0153
;;;	.word	0x6990

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	word_dst-6, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	r2, @word_dst
	beq	.Lnext52
	fail
.Lnext52:
	mov.w	#0, @word_dst	; zero it again for the next use.
.endif

mov_w_reg16_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, @(dd:16, erd)
	mov.l	#word_dst-4, er1
	mov.w	r0, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x6f90
;;;	.word	0x0004

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	word_dst-4, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.w	#0, r0
	mov.w	@word_dst, r0
	cmp.w	r2, r0
	beq	.Lnext45
	fail
.Lnext45:
	mov.w	#0, r0
	mov.w	r0, @word_dst	; zero it again for the next use.

mov_w_reg16_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, @(dd:32, erd)
	mov.l	#word_dst-8, er1
	mov.w	r0, @(8:32, er1)	; Register plus 32-bit disp. operand
;;;	.word	0x7810
;;;	.word	0x6ba0
;;;	.long	8

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	word_dst-8, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.w	#0, r0
	mov.w	@word_dst, r0
	cmp.w	r2, r0
	beq	.Lnext46
	fail
.Lnext46:
	mov.w	#0, r0
	mov.w	r0, @word_dst	; zero it again for the next use.

mov_w_reg16_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, @aa:16
	mov.w	r0, @word_dst:16	; 16-bit address-direct operand
;;;	.word	0x6b80
;;;	.word	@word_dst

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
	mov.w	#0, r0
	mov.w	@word_dst, r0
	cmp.w	r0, r1
	beq	.Lnext41
	fail
.Lnext41:
	mov.w	#0, r0
	mov.w	r0, @word_dst	; zero it again for the next use.

mov_w_reg16_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w ers, @aa:32
	mov.w	r0, @word_dst:32	; 32-bit address-direct operand
;;;	.word	0x6ba0
;;;	.long	@word_dst

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
	mov.w	#0, r0
	mov.w	@word_dst, r0
	cmp.w	r0, r1
	beq	.Lnext42
	fail
.Lnext42:
	mov.w	#0, r0
	mov.w	r0, @word_dst	; zero it again for the next use.

	;;
	;; Move word to register destination.
	;; 

mov_w_indirect_to_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @ers, rd
	mov.l	#word_src, er1
	mov.w	@er1, r0	; Register indirect operand
;;;	.word	0x6910

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a57777 er0

	test_h_gr32	word_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_w_postinc_to_reg16:		; post-increment from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @ers+, rd

	mov.l	#word_src, er1
	mov.w	@er1+, r0	; Register post-incr operand
;;;	.word	0x6d10

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a57777 er0

	test_h_gr32	word_src+2, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
mov_w_postdec_to_reg16:		; post-decrement from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @ers-, rd

	mov.l	#word_src, er1
	mov.w	@er1-, r0	; Register post-decr operand
;;;	.word	0x0152
;;;	.word	0x6d10

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a57777 er0

	test_h_gr32	word_src-2, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_w_preinc_to_reg16:		; pre-increment from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @+ers, rd

	mov.l	#word_src-2, er1
	mov.w	@+er1, r0	; Register pre-incr operand
;;;	.word	0x0151
;;;	.word	0x6d10

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a57777 er0

	test_h_gr32	word_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_w_predec_to_reg16:		; pre-decrement from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @-ers, rd

	mov.l	#word_src+2, er1
	mov.w	@-er1, r0	; Register pre-decr operand
;;;	.word	0x0153
;;;	.word	0x6d10

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a57777 er0

	test_h_gr32	word_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	
mov_w_disp2_to_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @(dd:2, ers), rd
	mov.l	#word_src-2, er1
	mov.w	@(2:2, er1), r0	; Register plus 2-bit disp. operand
;;; 	.word	0x0151
;;; 	.word	0x6910

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a57777 er0	; mov result:	a5a5 | 7777

	test_h_gr32	word_src-2, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

mov_w_disp16_to_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @(dd:16, ers), rd
	mov.l	#word_src+0x1234, er1
	mov.w	@(-0x1234:16, er1), r0	; Register plus 16-bit disp. operand
;;;	.word	0x6f10
;;;	.word	-0x1234

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a57777 er0	; mov result:	a5a5 | 7777

	test_h_gr32	word_src+0x1234, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_w_disp32_to_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @(dd:32, ers), rd
	mov.l	#word_src+65536, er1
	mov.w	@(-65536:32, er1), r0	; Register plus 32-bit disp. operand
;;;	.word	0x7810
;;;	.word	0x6b20
;;;	.long	-65536

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a57777 er0	; mov result:	a5a5 | 7777

	test_h_gr32	word_src+65536, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_w_abs16_to_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @aa:16, rd
	mov.w	@word_src:16, r0	; 16-bit address-direct operand
;;;	.word	0x6b00
;;;	.word	@word_src

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a57777 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_w_abs32_to_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @aa:32, rd
	mov.w	@word_src:32, r0	; 32-bit address-direct operand
;;;	.word	0x6b20
;;;	.long	@word_src

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a57777 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)

	;;
	;; Move word from memory to memory
	;; 

mov_w_indirect_to_indirect:	; reg indirect, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @ers, @erd

	mov.l	#word_src, er1
	mov.l	#word_dst, er0
	mov.w	@er1, @er0
;;;	.word	0x0158
;;;	.word	0x0100

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  word_dst er0
	test_h_gr32  word_src er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	@word_src, @word_dst
	beq	.Lnext55
	fail
.Lnext55:
	;; Now clear the destination location, and verify that.
	mov.w	#0, @word_dst
	cmp.w	@word_src, @word_dst
	bne	.Lnext56
	fail
.Lnext56:			; OK, pass on.

mov_w_postinc_to_postinc:	; reg post-increment, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @ers+, @erd+

	mov.l	#word_src, er1
	mov.l	#word_dst, er0
	mov.w	@er1+, @er0+
;;;	.word	0x0158
;;;	.word	0x8180

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  word_dst+2 er0
	test_h_gr32  word_src+2 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	@word_src, @word_dst
	beq	.Lnext65
	fail
.Lnext65:
	;; Now clear the destination location, and verify that.
	mov.w	#0, @word_dst
	cmp.w	@word_src, @word_dst
	bne	.Lnext66
	fail
.Lnext66:			; OK, pass on.

mov_w_postdec_to_postdec:	; reg post-decrement, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @ers-, @erd-

	mov.l	#word_src, er1
	mov.l	#word_dst, er0
	mov.w	@er1-, @er0-
;;;	.word	0x0158
;;;	.word	0xa1a0

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  word_dst-2 er0
	test_h_gr32  word_src-2 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	@word_src, @word_dst
	beq	.Lnext75
	fail
.Lnext75:
	;; Now clear the destination location, and verify that.
	mov.w	#0, @word_dst
	cmp.w	@word_src, @word_dst
	bne	.Lnext76
	fail
.Lnext76:			; OK, pass on.

mov_w_preinc_to_preinc:		; reg pre-increment, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @+ers, @+erd

	mov.l	#word_src-2, er1
	mov.l	#word_dst-2, er0
	mov.w	@+er1, @+er0
;;;	.word	0x0158
;;;	.word	0x9190

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  word_dst er0
	test_h_gr32  word_src er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	@word_src, @word_dst
	beq	.Lnext85
	fail
.Lnext85:
	;; Now clear the destination location, and verify that.
	mov.w	#0, @word_dst
	cmp.w	@word_src, @word_dst
	bne	.Lnext86
	fail
.Lnext86:				; OK, pass on.

mov_w_predec_to_predec:		; reg pre-decrement, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @-ers, @-erd

	mov.l	#word_src+2, er1
	mov.l	#word_dst+2, er0
	mov.w	@-er1, @-er0
;;;	.word	0x0158
;;;	.word	0xb1b0

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  word_dst er0
	test_h_gr32  word_src er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	@word_src, @word_dst
	beq	.Lnext95
	fail
.Lnext95:
	;; Now clear the destination location, and verify that.
	mov.w	#0, @word_dst
	cmp.w	@word_src, @word_dst
	bne	.Lnext96
	fail
.Lnext96:			; OK, pass on.

mov_w_disp2_to_disp2:		; reg 2-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @(dd:2, ers), @(dd:2, erd)

	mov.l	#word_src-2, er1
	mov.l	#word_dst-4, er0
	mov.w	@(2:2, er1), @(4:2, er0)
;;; 	.word	0x0158
;;; 	.word	0x1120

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  word_dst-4 er0
	test_h_gr32  word_src-2 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	@word_src, @word_dst
	beq	.Lnext105
	fail
.Lnext105:
	;; Now clear the destination location, and verify that.
	mov.w	#0, @word_dst
	cmp.w	@word_src, @word_dst
	bne	.Lnext106
	fail
.Lnext106:			; OK, pass on.

mov_w_disp16_to_disp16:		; reg 16-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @(dd:16, ers), @(dd:16, erd)

	mov.l	#word_src-1, er1
	mov.l	#word_dst-2, er0
	mov.w	@(1:16, er1), @(2:16, er0)
;;; 	.word	0x0158
;;; 	.word	0xc1c0
;;; 	.word	0x0001
;;; 	.word	0x0002

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  word_dst-2 er0
	test_h_gr32  word_src-1 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	@word_src, @word_dst
	beq	.Lnext115
	fail
.Lnext115:
	;; Now clear the destination location, and verify that.
	mov.w	#0, @word_dst
	cmp.w	@word_src, @word_dst
	bne	.Lnext116
	fail
.Lnext116:			; OK, pass on.

mov_w_disp32_to_disp32:		; reg 32-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @(dd:32, ers), @(dd:32, erd)

	mov.l	#word_src-1, er1
	mov.l	#word_dst-2, er0
	mov.w	@(1:32, er1), @(2:32, er0)
;;; 	.word	0x0158
;;; 	.word	0xc9c8
;;;	.long	1
;;;	.long	2

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  word_dst-2 er0
	test_h_gr32  word_src-1 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.w	@word_src, @word_dst
	beq	.Lnext125
	fail
.Lnext125:
	;; Now clear the destination location, and verify that.
	mov.w	#0, @word_dst
	cmp.w	@word_src, @word_dst
	bne	.Lnext126
	fail
.Lnext126:				; OK, pass on.

mov_w_abs16_to_abs16:		; 16-bit absolute addr, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @aa:16, @aa:16

	mov.w	@word_src:16, @word_dst:16
;;; 	.word	0x0158
;;; 	.word	0x4040
;;;	.word	@word_src
;;;	.word	@word_dst

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
	cmp.w	@word_src, @word_dst
	beq	.Lnext135
	fail
.Lnext135:
	;; Now clear the destination location, and verify that.
	mov.w	#0, @word_dst
	cmp.w	@word_src, @word_dst
	bne	.Lnext136
	fail
.Lnext136:				; OK, pass on.

mov_w_abs32_to_abs32:		; 32-bit absolute addr, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.w @aa:32, @aa:32

	mov.w	@word_src:32, @word_dst:32
;;; 	.word	0x0158
;;; 	.word	0x4848
;;;	.long	@word_src
;;;	.long	@word_dst

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
	cmp.w	@word_src, @word_dst
	beq	.Lnext145
	fail
.Lnext145:
	;; Now clear the destination location, and verify that.
	mov.w	#0, @word_dst
	cmp.w	@word_src, @word_dst
	bne	.Lnext146
	fail
.Lnext146:				; OK, pass on.


.endif

	pass

	exit 0
