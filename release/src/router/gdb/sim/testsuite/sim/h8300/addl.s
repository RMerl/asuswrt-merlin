# Hitachi H8 testcase 'add.l'
# mach(): h8300h h8300s h8sx
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf	

	.include "testutils.inc"

	# Instructions tested:
	# add.l xx:3, erd
	# add.l xx:16, erd
	# add.l xx:32, erd
	# add.l xx:16, @erd
	# add.l xx:16, @erd+
	# add.l xx:16, @erd-
	# add.l xx:16, @+erd
	# add.l xx:16, @-erd
	# add.l xx:16, @(dd:2, erd)
	# add.l xx:16, @(dd:16, erd)
	# add.l xx:16, @(dd:32, erd)
	# add.l xx:16, @aa:16
	# add.l xx:16, @aa:32
	# add.l xx:32, @erd+
	# add.l xx:32, @erd-
	# add.l xx:32, @+erd
	# add.l xx:32, @-erd
	# add.l xx:32, @(dd:2, erd)
	# add.l xx:32, @(dd:16, erd)
	# add.l xx:32, @(dd:32, erd)
	# add.l xx:32, @aa:16
	# add.l xx:32, @aa:32
	# add.l ers, erd
	# add.l ers, @erd
	# add.l ers, @erd+
	# add.l ers, @erd-
	# add.l ers, @+erd
	# add.l ers, @-erd
	# add.l ers, @(dd:2, erd)
	# add.l ers, @(dd:16, erd)
	# add.l ers, @(dd:32, erd)
	# add.l ers, @aa:16
	# add.l ers, @aa:32
	# add.l ers, erd
	# add.l @ers, erd
	# add.l @ers+, erd
	# add.l @ers-, erd
	# add.l @+ers, erd
	# add.l @-ers, erd
	# add.l @(dd:2, ers), erd
	# add.l @(dd:16, ers), erd
	# add.l @(dd:32, ers), erd
	# add.l @aa:16, erd
	# add.l @aa:32, erd
	# add.l @ers, @erd
	# add.l @ers+, @erd+
	# add.l @ers-, @erd-
	# add.l @+ers, +@erd
	# add.l @-ers, @-erd
	# add.l @(dd:2, ers), @(dd:2, erd)
	# add.l @(dd:16, ers), @(dd:16, erd)
	# add.l @(dd:32, ers), @(dd:32, erd)
	# add.l @aa:16, @aa:16
	# add.l @aa:32, @aa:32

	start

	.data
	.align	4
long_src:
	.long	0x12345678
long_dst:
	.long	0x87654321

	.text

	;;
	;; Add long from immediate source
	;; 

.if (sim_cpu == h8sx)
add_l_imm3_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:3, erd
	add.l	#0x3:3, er0	; Immediate 16-bit operand
;;;	.word	0x0ab8

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a5a8 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

add_l_imm16_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, erd
	add.l	#0x1234, er0	; Immediate 16-bit operand
;;;	.word	0x7a18
;;;	.word	0x1234

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5b7d9 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

add_l_imm32_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:32, erd
	add.l	#0x12345678, er0	; Immediate 32-bit operand
;;;	.word	0x7a10
;;;	.long	0x12345678

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
add_l_imm16_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, @erd
	mov.l	#long_dst, er1
	add.l	#0xdead:16, @er1	; Register indirect operand
;;;	.word	0x010e
;;;	.word	0x0110
;;;	.word	0xdead

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
	cmp.l	#0x876621ce, @long_dst
	beq	.Lnext11
	fail
.Lnext11:
	mov.l	#0x87654321, @long_dst	; Initialize it again for the next use.

add_l_imm16_to_postinc:		; post-increment from imm16 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, @erd+
	mov.l	#long_dst, er1
	add.l	#0xdead:16, @er1+	; Imm16, register post-incr operands.
;;;	.word	0x010e
;;;	.word	0x8110
;;;	.word	0xdead

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
	cmp.l	#0x876621ce, @long_dst
	beq	.Lnext12
	fail
.Lnext12:
	mov.l	#0x87654321, @long_dst	; initialize it again for the next use.

add_l_imm16_to_postdec:		; post-decrement from imm16 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, @erd-
	mov.l	#long_dst, er1
	add.l	#0xdead:16, @er1-	; Imm16, register post-decr operands.
;;;	.word	0x010e
;;;	.word	0xa110
;;;	.word	0xdead

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
	cmp.l	#0x876621ce, @long_dst
	beq	.Lnext13
	fail
.Lnext13:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm16_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, @+erd
	mov.l	#long_dst-4, er1
	add.l	#0xdead:16, @+er1	; Imm16, register pre-incr operands
;;;	.word	0x010e
;;;	.word	0x9110
;;;	.word	0xdead

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
	cmp.l	#0x876621ce, @long_dst
	beq	.Lnext14
	fail
.Lnext14:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm16_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, @-erd
	mov.l	#long_dst+4, er1
	add.l	#0xdead:16, @-er1	; Imm16, register pre-decr operands
;;;	.word	0x010e
;;;	.word	0xb110
;;;	.word	0xdead

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
	cmp.l	#0x876621ce, @long_dst
	beq	.Lnext15
	fail
.Lnext15:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm16_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, @(dd:2, erd)
	mov.l	#long_dst-12, er1
	add.l	#0xdead:16, @(12:2, er1) ; Imm16, reg plus 2-bit disp. operand
;;;	.word	0x010e
;;;	.word	0x3110
;;;	.word	0xdead

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
	cmp.l	#0x876621ce, @long_dst
	beq	.Lnext16
	fail
.Lnext16:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm16_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, @(dd:16, erd)
	mov.l	#long_dst-4, er1
	add.l	#0xdead:16, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x010e
;;;	.word	0xc110
;;;	.word	0xdead
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
	cmp.l	#0x876621ce, @long_dst
	beq	.Lnext17
	fail
.Lnext17:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm16_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, @(dd:32, erd)
	mov.l	#long_dst-8, er1
	add.l	#0xdead:16, @(8:32, er1)   ; Register plus 32-bit disp. operand
;;;	.word	0x010e
;;;	.word	0xc910
;;;	.word	0xdead
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
	cmp.l	#0x876621ce, @long_dst
	beq	.Lnext18
	fail
.Lnext18:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm16_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, @aa:16
	add.l	#0xdead:16, @long_dst:16	; 16-bit address-direct operand
;;;	.word	0x010e
;;;	.word	0x4010
;;;	.word	0xdead
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
	cmp.l	#0x876621ce, @long_dst
	beq	.Lnext19
	fail
.Lnext19:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm16_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:16, @aa:32
	add.l	#0xdead:16, @long_dst:32	; 32-bit address-direct operand
;;;	.word	0x010e
;;;	.word	0x4810
;;;	.word	0xdead
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
	cmp.l	#0x876621ce, @long_dst
	beq	.Lnext20
	fail
.Lnext20:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm32_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:32, @erd
	mov.l	#long_dst, er1
	add.l	#0xcafedead:32, @er1	; Register indirect operand
;;;	.word	0x010e
;;;	.word	0x0118
;;;	.long	0xcafedead

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x526421ce, @long_dst
	beq	.Lnext21
	fail
.Lnext21:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm32_to_postinc:		; post-increment from imm32 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:32, @erd+
	mov.l	#long_dst, er1
	add.l	#0xcafedead:32, @er1+	; Imm32, register post-incr operands.
;;;	.word	0x010e
;;;	.word	0x8118
;;;	.long	0xcafedead

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst+4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x526421ce, @long_dst
	beq	.Lnext22
	fail
.Lnext22:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm32_to_postdec:		; post-decrement from imm32 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:32, @erd-
	mov.l	#long_dst, er1
	add.l	#0xcafedead:32, @er1-	; Imm32, register post-decr operands.
;;;	.word	0x010e
;;;	.word	0xa118
;;;	.long	0xcafedead

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x526421ce, @long_dst
	beq	.Lnext23
	fail
.Lnext23:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm32_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:32, @+erd
	mov.l	#long_dst-4, er1
	add.l	#0xcafedead:32, @+er1	; Imm32, register pre-incr operands
;;;	.word	0x010e
;;;	.word	0x9118
;;;	.long	0xcafedead

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x526421ce, @long_dst
	beq	.Lnext24
	fail
.Lnext24:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm32_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:32, @-erd
	mov.l	#long_dst+4, er1
	add.l	#0xcafedead:32, @-er1	; Imm32, register pre-decr operands
;;;	.word	0x010e
;;;	.word	0xb118
;;;	.long	0xcafedead

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x526421ce, @long_dst
	beq	.Lnext25
	fail
.Lnext25:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm32_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:32, @(dd:2, erd)
	mov.l	#long_dst-12, er1
	add.l	#0xcafedead:32, @(12:2, er1) ; Imm32, reg plus 2-bit disp. operand
;;;	.word	0x010e
;;;	.word	0x3118
;;;	.long	0xcafedead

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-12, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x526421ce, @long_dst
	beq	.Lnext26
	fail
.Lnext26:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm32_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:32, @(dd:16, erd)
	mov.l	#long_dst-4, er1
	add.l	#0xcafedead:32, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x010e
;;;	.word	0xc118
;;;	.long	0xcafedead
;;;	.word	0x0004

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x526421ce, @long_dst
	beq	.Lnext27
	fail
.Lnext27:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm32_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:32, @(dd:32, erd)
	mov.l	#long_dst-8, er1
	add.l	#0xcafedead:32, @(8:32, er1)   ; Register plus 32-bit disp. operand
;;;	.word	0x010e
;;;	.word	0xc918
;;;	.long	0xcafedead
;;;	.long	8

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-8, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x526421ce, @long_dst
	beq	.Lnext28
	fail
.Lnext28:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm32_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l #xx:32, @aa:16
	add.l	#0xcafedead:32, @long_dst:16	; 16-bit address-direct operand
;;;	.word	0x010e
;;;	.word	0x4018
;;;	.long	0xcafedead
;;;	.word	@long_dst

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x526421ce, @long_dst
	beq	.Lnext29
	fail
.Lnext29:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_imm32_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.l #xx:32, @aa:32
	add.l	#0xcafedead:32, @long_dst:32	; 32-bit address-direct operand
;;;	.word	0x010e
;;;	.word	0x4818
;;;	.long	0xcafedead
;;;	.long	@long_dst

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x526421ce, @long_dst
	beq	.Lnext30
	fail
.Lnext30:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.
.endif

	;;
	;; Add long from register source
	;; 

add_l_reg32_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, erd
	mov.l	#0x12345678, er1
	add.l	er1, er0	; Register 32-bit operand
;;;	.word	0x0a90

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear
	
	test_h_gr32 0xb7d9fc1d er0	; add result
	test_h_gr32 0x12345678 er1	; add src unchanged

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
add_l_reg32_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, @erd
	mov.l	#long_dst, er1
	add.l	er0, @er1	; Register indirect operand
;;;	.word	0x0109
;;;	.word	0x0110

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x2d0ae8c6, @long_dst
	beq	.Lnext44
	fail
.Lnext44:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_reg32_to_postinc:		; post-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, @erd+
	mov.l	#long_dst, er1
	add.l	er0, @er1+	; Register post-incr operand
;;;	.word	0x0109
;;;	.word	0x8110

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst+4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x2d0ae8c6, @long_dst
	beq	.Lnext49
	fail
.Lnext49:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_reg32_to_postdec:		; post-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, @erd-
	mov.l	#long_dst, er1
	add.l	er0, @er1-	; Register post-decr operand
;;;	.word	0x0109
;;;	.word	0xa110

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x2d0ae8c6, @long_dst
	beq	.Lnext50
	fail
.Lnext50:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_reg32_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, @+erd
	mov.l	#long_dst-4, er1
	add.l	er0, @+er1	; Register pre-incr operand
;;;	.word	0x0109
;;;	.word	0x9110

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x2d0ae8c6, @long_dst
	beq	.Lnext51
	fail
.Lnext51:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_reg32_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, @-erd
	mov.l	#long_dst+4, er1
	add.l	er0, @-er1	; Register pre-decr operand
;;;	.word	0x0109
;;;	.word	0xb110

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x2d0ae8c6, @long_dst
	beq	.Lnext48
	fail
.Lnext48:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_reg32_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, @(dd:2, erd)
	mov.l	#long_dst-12, er1
	add.l	er0, @(12:2, er1)	; Register plus 2-bit disp. operand
;;;	.word	0x0109
;;;	.word	0x3110

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	long_dst-12, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x2d0ae8c6, @long_dst
	beq	.Lnext52
	fail
.Lnext52:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_reg32_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, @(dd:16, erd)
	mov.l	#long_dst-4, er1
	add.l	er0, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x0109
;;;	.word	0xc110
;;;	.word	0x0004

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_h_gr32	long_dst-4, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x2d0ae8c6, @long_dst
	beq	.Lnext45
	fail
.Lnext45:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_reg32_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, @(dd:32, erd)
	mov.l	#long_dst-8, er1
	add.l	er0, @(8:32, er1)	; Register plus 32-bit disp. operand
;;;	.word	0x0109
;;;	.word	0xc910
;;;	.long	8

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_h_gr32	long_dst-8, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x2d0ae8c6, @long_dst
	beq	.Lnext46
	fail
.Lnext46:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_reg32_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, @aa:16
	add.l	er0, @long_dst:16	; 16-bit address-direct operand
;;;	.word	0x0109
;;;	.word	0x4110
;;;	.word	@long_dst

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set

	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x2d0ae8c6, @long_dst
	beq	.Lnext41
	fail
.Lnext41:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

add_l_reg32_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l ers, @aa:32
	add.l	er0, @long_dst:32	; 32-bit address-direct operand
;;;	.word	0x0109
;;;	.word	0x4910
;;;	.long	@long_dst

	;; test ccr		; H=0 N=0 Z=0 V=1 C=1
	test_neg_clear
	test_zero_clear
	test_ovf_set
	test_carry_set
	
	test_gr_a5a5 0		; Make sure _ALL_ general regs not disturbed
	test_gr_a5a5 1		; (first, because on h8/300 we must use one
	test_gr_a5a5 2		; to examine the destination memory).
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.l	#0x2d0ae8c6, @long_dst
	beq	.Lnext42
	fail
.Lnext42:
	mov.l	#0x87654321, @long_dst	; Re-initialize it for the next use.

	;;
	;; Add long to register destination.
	;; 

add_l_indirect_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @ers, Rd
	mov.l	#long_src, er1
	add.l	@er1, er0	; Register indirect operand
;;;	.word	0x010a
;;;	.word	0x0110

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0

	test_h_gr32	long_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

add_l_postinc_to_reg32:		; post-increment from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @ers+, erd
	mov.l	#long_src, er1
	add.l	@er1+, er0	; Register post-incr operand
;;;	.word	0x010a
;;;	.word	0x8110

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0

	test_h_gr32	long_src+4, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

add_l_postdec_to_reg32:		; post-decrement from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @ers-, erd
	mov.l	#long_src, er1
	add.l	@er1-, er0	; Register post-decr operand
;;;	.word	0x010a
;;;	.word	0xa110

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0

	test_h_gr32	long_src-4, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

add_l_preinc_to_reg32:		; pre-increment from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @+ers, erd
	mov.l	#long_src-4, er1
	add.l	@+er1, er0	; Register pre-incr operand
;;;	.word	0x010a
;;;	.word	0x9110

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0

	test_h_gr32	long_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

add_l_predec_to_reg32:		; pre-decrement from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @-ers, erd
	mov.l	#long_src+4, er1
	add.l	@-er1, er0	; Register pre-decr operand
;;;	.word	0x010a
;;;	.word	0xb110

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0

	test_h_gr32	long_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	
add_l_disp2_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @(dd:2, ers), erd
	mov.l	#long_src-4, er1
	add.l	@(4:2, er1), er0	; Register plus 2-bit disp. operand
;;; 	.word	0x010a
;;; 	.word	0x1110

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0	; mov result:	a5a5 | 7777

	test_h_gr32	long_src-4, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

add_l_disp16_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @(dd:16, ers), erd
	mov.l	#long_src+0x1234, er1
	add.l	@(-0x1234:16, er1), er0	; Register plus 16-bit disp. operand
;;;	.word	0x010a
;;;	.word	0xc110
;;;	.word	-0x1234

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0	; mov result:	a5a5 | 7777

	test_h_gr32	long_src+0x1234, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

add_l_disp32_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @(dd:32, ers), erd
	mov.l	#long_src+65536, er1
	add.l	@(-65536:32, er1), er0	; Register plus 32-bit disp. operand
;;;	.word	0x010a
;;;	.word	0xc910
;;;	.long	-65536

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0	; mov result:	a5a5 | 7777

	test_h_gr32	long_src+65536, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

add_l_abs16_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @aa:16, erd
	add.l	@long_src:16, er0	; 16-bit address-direct operand
;;;	.word	0x010a
;;;	.word	0x4010
;;;	.word	@long_src

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

add_l_abs32_to_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @aa:32, erd
	add.l	@long_src:32, er0	; 32-bit address-direct operand
;;;	.word	0x010a
;;;	.word	0x4810
;;;	.long	@long_src

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xb7d9fc1d er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7


	;;
	;; Add long from memory to memory
	;; 

add_l_indirect_to_indirect:	; reg indirect, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @ers, @erd
	mov.l	#long_src, er1
	mov.l	#long_dst, er0
	add.l	@er1, @er0
;;;	.word	0x0104
;;;	.word	0x691c
;;;	.word	0x0010

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
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
	cmp.l	#0x99999999, @long_dst	; FIXME
	beq	.Lnext55
	fail
.Lnext55:
	;; Now clear the destination location, and verify that.
	mov.l	#0x87654321, @long_dst
	cmp.l	#0x99999999, @long_dst
	bne	.Lnext56
	fail
.Lnext56:			; OK, pass on.

add_l_postinc_to_postinc:	; reg post-increment, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @ers+, @erd+
	mov.l	#long_src, er1
	mov.l	#long_dst, er0
	add.l	@er1+, @er0+
;;;	.word	0x0104
;;;	.word	0x6d1c
;;;	.word	0x8010

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
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
	cmp.l	#0x99999999, @long_dst
	beq	.Lnext65
	fail
.Lnext65:
	;; Now clear the destination location, and verify that.
	mov.l	#0x87654321, @long_dst
	cmp.l	#0x99999999, @long_dst
	bne	.Lnext66
	fail
.Lnext66:			; OK, pass on.

add_l_postdec_to_postdec:	; reg post-decrement, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @ers-, @erd-
	mov.l	#long_src, er1
	mov.l	#long_dst, er0
	add.l	@er1-, @er0-
;;;	.word	0x0106
;;;	.word	0x6d1c
;;;	.word	0xa010

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
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
	cmp.l	#0x99999999, @long_dst
	beq	.Lnext75
	fail
.Lnext75:
	;; Now clear the destination location, and verify that.
	mov.l	#0x87654321, @long_dst
	cmp.l	#0x99999999, @long_dst
	bne	.Lnext76
	fail
.Lnext76:			; OK, pass on.

add_l_preinc_to_preinc:		; reg pre-increment, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @+ers, @+erd
	mov.l	#long_src-4, er1
	mov.l	#long_dst-4, er0
	add.l	@+er1, @+er0
;;;	.word	0x0105
;;;	.word	0x6d1c
;;;	.word	0x9010

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
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
	cmp.l	#0x99999999, @long_dst
	beq	.Lnext85
	fail
.Lnext85:
	;; Now clear the destination location, and verify that.
	mov.l	#0x87654321, @long_dst
	cmp.l	#0x99999999, @long_dst
	bne	.Lnext86
	fail
.Lnext86:				; OK, pass on.

add_l_predec_to_predec:		; reg pre-decrement, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @-ers, @-erd
	mov.l	#long_src+4, er1
	mov.l	#long_dst+4, er0
	add.l	@-er1, @-er0
;;;	.word	0x0107
;;;	.word	0x6d1c
;;;	.word	0xb010

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
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
	cmp.l	#0x99999999, @long_dst
	beq	.Lnext95
	fail
.Lnext95:
	;; Now clear the destination location, and verify that.
	mov.l	#0x87654321, @long_dst
	cmp.l	#0x99999999, @long_dst
	bne	.Lnext96
	fail
.Lnext96:			; OK, pass on.

add_l_disp2_to_disp2:		; reg 2-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @(dd:2, ers), @(dd:2, erd)
	mov.l	#long_src-4, er1
	mov.l	#long_dst-8, er0
	add.l	@(4:2, er1), @(8:2, er0)
;;; 	.word	0x0105
;;;	.word	0x691c
;;; 	.word	0x2010

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
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
	cmp.l	#0x99999999, @long_dst
	beq	.Lnext105
	fail
.Lnext105:
	;; Now clear the destination location, and verify that.
	mov.l	#0x87654321, @long_dst
	cmp.l	#0x99999999, @long_dst
	bne	.Lnext106
	fail
.Lnext106:			; OK, pass on.

add_l_disp16_to_disp16:		; reg 16-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @(dd:16, ers), @(dd:16, erd)
	mov.l	#long_src-1, er1
	mov.l	#long_dst-2, er0
	add.l	@(1:16, er1), @(2:16, er0)
;;; 	.word	0x0104
;;;	.word	0x6f1c
;;; 	.word	0x0001
;;; 	.word	0xc010
;;; 	.word	0x0002

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
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
	cmp.l	#0x99999999, @long_dst
	beq	.Lnext115
	fail
.Lnext115:
	;; Now clear the destination location, and verify that.
	mov.l	#0x87654321, @long_dst
	cmp.l	#0x99999999, @long_dst
	bne	.Lnext116
	fail
.Lnext116:			; OK, pass on.

add_l_disp32_to_disp32:		; reg 32-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @(dd:32, ers), @(dd:32, erd)
	mov.l	#long_src-1, er1
	mov.l	#long_dst-2, er0
	add.l	@(1:32, er1), @(2:32, er0)
;;; 	.word	0x7894
;;;	.word	0x6b2c
;;; 	.word	0xc9c8
;;;	.long	1
;;;	.word	0xc810
;;;	.long	2

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
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
	cmp.l	#0x99999999, @long_dst
	beq	.Lnext125
	fail
.Lnext125:
	;; Now clear the destination location, and verify that.
	mov.l	#0x87654321, @long_dst
	cmp.l	#0x99999999, @long_dst
	bne	.Lnext126
	fail
.Lnext126:				; OK, pass on.

add_l_abs16_to_abs16:		; 16-bit absolute addr, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @aa:16, @aa:16
	add.l	@long_src:16, @long_dst:16
;;; 	.word	0x0104
;;;	.word	0x6b0c
;;;	.word	@long_src
;;; 	.word	0x4010
;;;	.word	@long_dst

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
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
	cmp.l	#0x99999999, @long_dst
	beq	.Lnext135
	fail
.Lnext135:
	;; Now clear the destination location, and verify that.
	mov.l	#0x87654321, @long_dst
	cmp.l	#0x99999999, @long_dst
	bne	.Lnext136
	fail
.Lnext136:				; OK, pass on.

add_l_abs32_to_abs32:		; 32-bit absolute addr, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; add.l @aa:32, @aa:32
	add.l	@long_src:32, @long_dst:32
;;; 	.word	0x0104
;;;	.word	0x6b2c
;;;	.long	@long_src
;;; 	.word	0x4810
;;;	.long	@long_dst

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
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
	cmp.l	#0x99999999, @long_dst
	beq	.Lnext145
	fail
.Lnext145:
	;; Now clear the destination location, and verify that.
	mov.l	#0x87654321, @long_dst
	cmp.l	#0x99999999, @long_dst
	bne	.Lnext146
	fail
.Lnext146:				; OK, pass on.

.endif

	pass

	exit 0
