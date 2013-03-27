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
	.align	4
byte_src:
	.byte	0x77
byte_dst:
	.byte	0

	.text

	;;
	;; Move byte from immediate source
	;; 

.if (sim_cpu == h8sx)
mov_b_imm8_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, rd
	mov.b	#0x77:8, r0l	; Immediate 3-bit operand
;;;	.word	0xf877

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

.if (sim_cpu == h8sx)
mov_b_imm4_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:4, @aa:16
	mov.b	#0xf:4, @byte_dst:16	; 16-bit address-direct operand
;;;	.word	0x6adf
;;;	.word	@byte_dst

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
	cmp.b	#0xf, @byte_dst
	beq	.Lnext21
	fail
.Lnext21:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm4_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:4, @aa:32
	mov.b	#0xf:4, @byte_dst:32	; 32-bit address-direct operand
;;;	.word	0x6aff
;;;	.long	@byte_dst

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
	cmp.b	#0xf, @byte_dst
	beq	.Lnext22
	fail
.Lnext22:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, @erd
	mov.l	#byte_dst, er1
	mov.b	#0xa5:8, @er1	; Register indirect operand
;;;	.word	0x017d
;;;	.word	0x01a5

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	beq	.Lnext1
	fail
.Lnext1:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_postinc:		; post-increment from imm8 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, @erd+
	mov.l	#byte_dst, er1
	mov.b	#0xa5:8, @er1+	; Imm8, register post-incr operands.
;;;	.word	0x017d
;;;	.word	0x81a5

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst+1, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	beq	.Lnext2
	fail
.Lnext2:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_postdec:		; post-decrement from imm8 to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, @erd-
	mov.l	#byte_dst, er1
	mov.b	#0xa5:8, @er1-	; Imm8, register post-decr operands.
;;;	.word	0x017d
;;;	.word	0xa1a5

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst-1, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	beq	.Lnext3
	fail
.Lnext3:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, @+erd
	mov.l	#byte_dst-1, er1
	mov.b	#0xa5:8, @+er1	; Imm8, register pre-incr operands
;;;	.word	0x017d
;;;	.word	0x91a5

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	beq	.Lnext4
	fail
.Lnext4:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, @-erd
	mov.l	#byte_dst+1, er1
	mov.b	#0xa5:8, @-er1	; Imm8, register pre-decr operands
;;;	.word	0x017d
;;;	.word	0xb1a5

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	beq	.Lnext5
	fail
.Lnext5:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, @(dd:2, erd)
	mov.l	#byte_dst-3, er1
	mov.b	#0xa5:8, @(3:2, er1)	; Imm8, reg plus 2-bit disp. operand
;;;	.word	0x017d
;;;	.word	0x31a5

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst-3, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	beq	.Lnext6
	fail
.Lnext6:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, @(dd:16, erd)
	mov.l	#byte_dst-4, er1
	mov.b	#0xa5:8, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x017d
;;;	.word	0x6f90
;;;	.word	0x0004

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst-4, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	beq	.Lnext7
	fail
.Lnext7:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, @(dd:32, erd)
	mov.l	#byte_dst-8, er1
	mov.b	#0xa5:8, @(8:32, er1)	; Register plus 32-bit disp. operand
;;;	.word	0x017d
;;;	.word	0xc9a5
;;;	.long	8

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst-8, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	beq	.Lnext8
	fail
.Lnext8:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_indexb16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffffff01, er1
	set_ccr_zero
	;; mov.b #xx:8, @(dd:16, rd.b)
	mov.b	#0xa5:8, @(byte_dst-1:16, r1.b) ; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	0xffffff01, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_indexw16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffff0002, er1
	set_ccr_zero
	;; mov.b #xx:8, @(dd:16, rd.w)
	mov.b	#0xa5:8, @(byte_dst-2:16, r1.w) ; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	0xffff0002, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_indexl16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0x00000003, er1
	set_ccr_zero
	;; mov.b #xx:8, @(dd:16, erd.l)
	mov.b	#0xa5:8, @(byte_dst-3:16, er1.l) ; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	0x00000003, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_indexb32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffffff04, er1
	set_ccr_zero
	;; mov.b #xx:8, @(dd:32, rd.b)
	mov.b	#0xa5:8, @(byte_dst-4:32, r1.b)	; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	0xffffff04 er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_indexw32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffff0005, er1
	set_ccr_zero
	;; mov.b #xx:8, @(dd:32, rd.w)
	mov.b	#0xa5:8, @(byte_dst-5:32, r1.w)	; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	0xffff0005 er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_indexl32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0x00000006, er1
	set_ccr_zero
	;; mov.b #xx:8, @(dd:32, erd.l)
	mov.b	#0xa5:8, @(byte_dst-6:32, er1.l)	; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	0x00000006 er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	#0xa5, @byte_dst
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, @aa:16
	mov.b	#0xa5:8, @byte_dst:16	; 16-bit address-direct operand
;;;	.word	0x017d
;;;	.word	0x40a5
;;;	.word	@byte_dst

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
	cmp.b	#0xa5, @byte_dst
	beq	.Lnext9
	fail
.Lnext9:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_imm8_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b #xx:8, @aa:32
	mov.b	#0xa5:8, @byte_dst:32	; 32-bit address-direct operand
;;;	.word	0x017d
;;;	.word	0x48a5
;;;	.long	@byte_dst

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
	cmp.b	#0xa5, @byte_dst
	beq	.Lnext10
	fail
.Lnext10:
	mov.b	#0, @byte_dst	; zero it again for the next use.

.endif

	;;
	;; Move byte from register source
	;; 

mov_b_reg8_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, erd
	mov.b	#0x12, r1l
	mov.b	r1l, r0l	; Register 8-bit operand
;;;	.word	0x0c98

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear
	test_h_gr16 0xa512 r0
	test_h_gr16 0xa512 r1	; mov src unchanged
.if (sim_cpu)
	test_h_gr32 0xa5a5a512 er0
	test_h_gr32 0xa5a5a512 er1	; mov src unchanged
.endif
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7


mov_b_reg8_to_indirect:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, @erd
	mov.l	#byte_dst, er1
	mov.b	r0l, @er1	; Register indirect operand
;;;	.word	0x6898

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.b	@byte_dst, r0l
	cmp.b	r2l, r0l
	beq	.Lnext44
	fail
.Lnext44:
	mov.b	#0, r0l
	mov.b	r0l, @byte_dst	; zero it again for the next use.

.if (sim_cpu == h8sx)
mov_b_reg8_to_postinc:		; post-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, @erd+
	mov.l	#byte_dst, er1
	mov.b	r0l, @er1+	; Register post-incr operand
;;;	.word	0x0173
;;;	.word	0x6c98

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst+1, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	r2l, @byte_dst
	beq	.Lnext49
	fail
.Lnext49:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_reg8_to_postdec:		; post-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, @erd-
	mov.l	#byte_dst, er1
	mov.b	r0l, @er1-	; Register post-decr operand
;;;	.word	0x0171
;;;	.word	0x6c98

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst-1, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	r2l, @byte_dst
	beq	.Lnext50
	fail
.Lnext50:
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_reg8_to_preinc:		; pre-increment from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, @+erd
	mov.l	#byte_dst-1, er1
	mov.b	r0l, @+er1	; Register pre-incr operand
;;;	.word	0x0172
;;;	.word	0x6c98

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	r2l, @byte_dst
	beq	.Lnext51
	fail
.Lnext51:
	mov.b	#0, @byte_dst	; zero it again for the next use.
.endif

mov_b_reg8_to_predec:		; pre-decrement from register to mem
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, @-erd
	mov.l	#byte_dst+1, er1
	mov.b	r0l, @-er1	; Register pre-decr operand
;;;	.word	0x6c98

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.b	@byte_dst, r0l
	cmp.b	r2l, r0l
	beq	.Lnext48
	fail
.Lnext48:
	mov.b	#0, r0l
	mov.b	r0l, @byte_dst	; zero it again for the next use.

.if (sim_cpu == h8sx)
mov_b_reg8_to_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, @(dd:2, erd)
	mov.l	#byte_dst-3, er1
	mov.b	r0l, @(3:2, er1)	; Register plus 2-bit disp. operand
;;;	.word	0x0173
;;;	.word	0x6898

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32	byte_dst-3, er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	r2l, @byte_dst
	beq	.Lnext52
	fail
.Lnext52:
	mov.b	#0, @byte_dst	; zero it again for the next use.
.endif

mov_b_reg8_to_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, @(dd:16, erd)
	mov.l	#byte_dst-4, er1
	mov.b	r0l, @(4:16, er1)	; Register plus 16-bit disp. operand
;;;	.word	0x6e98
;;;	.word	0x0004

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	byte_dst-4, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.b	@byte_dst, r0l
	cmp.b	r2l, r0l
	beq	.Lnext45
	fail
.Lnext45:
	mov.b	#0, r0l
	mov.b	r0l, @byte_dst	; zero it again for the next use.

mov_b_reg8_to_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, @(dd:32, erd)
	mov.l	#byte_dst-8, er1
	mov.b	r0l, @(8:32, er1)	; Register plus 32-bit disp. operand
;;;	.word	0x7810
;;;	.word	0x6aa8
;;;	.long	8

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	byte_dst-8, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	mov.b	@byte_dst, r0l
	cmp.b	r2l, r0l
	beq	.Lnext46
	fail
.Lnext46:
	mov.b	#0, r0l
	mov.b	r0l, @byte_dst	; zero it again for the next use.

.if (sim_cpu == h8sx)
mov_b_reg8_to_indexb16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffffff01, er1
	set_ccr_zero
	;; mov.b ers, @(dd:16, rd.b)
	mov.b	r0l, @(byte_dst-1:16, r1.b)	; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	0xffffff01 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_dst, r0l
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_reg8_to_indexw16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffff0002, er1
	set_ccr_zero
	;; mov.b ers, @(dd:16, rd.w)
	mov.b	r0l, @(byte_dst-2:16, r1.w)	; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	0xffff0002 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_dst, r0l
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_reg8_to_indexl16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0x00000003, er1
	set_ccr_zero
	;; mov.b ers, @(dd:16, erd.l)
	mov.b	r0l, @(byte_dst-3:16, er1.l)	; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	0x00000003 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_dst, r0l
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_reg8_to_indexb32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffffff04 er1
	set_ccr_zero
	;; mov.b ers, @(dd:32, rd.b)
	mov.b	r0l, @(byte_dst-4:32, r1.b)	; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	0xffffff04, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_dst, r0l
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_reg8_to_indexw32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffff0005 er1
	set_ccr_zero
	;; mov.b ers, @(dd:32, rd.w)
	mov.b	r0l, @(byte_dst-5:32, r1.w)	; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	0xffff0005, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_dst, r0l
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.

mov_b_reg8_to_indexl32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0x00000006 er1
	set_ccr_zero
	;; mov.b ers, @(dd:32, erd.l)
	mov.b	r0l, @(byte_dst-6:32, er1.l)	; byte indexed operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32	0x00000006, er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_dst, r0l
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.
.endif

.if (sim_cpu == h8sx)
mov_b_reg8_to_abs8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	mov.l	#byte_dst-20, er0
	ldc	er0, sbr
	set_ccr_zero
	;; mov.b ers, @aa:8
	mov.b	r1l, @20:8	; 8-bit address-direct (sbr-relative) operand

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32  byte_dst-20, er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_dst, r1l
	bne	fail1
	mov.b	#0, @byte_dst	; zero it again for the next use.
.endif

mov_b_reg8_to_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, @aa:16
	mov.b	r0l, @byte_dst:16	; 16-bit address-direct operand
;;;	.word	0x6a88
;;;	.word	@byte_dst

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
	mov.b	@byte_dst, r0l
	cmp.b	r0l, r1l
	beq	.Lnext41
	fail
.Lnext41:
	mov.b	#0, r0l
	mov.b	r0l, @byte_dst	; zero it again for the next use.

mov_b_reg8_to_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b ers, @aa:32
	mov.b	r0l, @byte_dst:32	; 32-bit address-direct operand
;;;	.word	0x6aa8
;;;	.long	@byte_dst

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
	mov.b	@byte_dst, r0l
	cmp.b	r0l, r1l
	beq	.Lnext42
	fail
.Lnext42:
	mov.b	#0, r0l
	mov.b	r0l, @byte_dst	; zero it again for the next use.

	;;
	;; Move byte to register destination.
	;; 

mov_b_indirect_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @ers, rd
	mov.l	#byte_src, er1
	mov.b	@er1, r0l	; Register indirect operand
;;;	.word	0x6818

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0

	test_h_gr32	byte_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_b_postinc_to_reg8:		; post-increment from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @ers+, rd

	mov.l	#byte_src, er1
	mov.b	@er1+, r0l	; Register post-incr operand
;;;	.word	0x6c18

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0

	test_h_gr32	byte_src+1, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
mov_b_postdec_to_reg8:		; post-decrement from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @ers-, rd

	mov.l	#byte_src, er1
	mov.b	@er1-, r0l	; Register post-decr operand
;;;	.word	0x0172
;;;	.word	0x6c18

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0

	test_h_gr32	byte_src-1, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_b_preinc_to_reg8:		; pre-increment from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @+ers, rd

	mov.l	#byte_src-1, er1
	mov.b	@+er1, r0l	; Register pre-incr operand
;;;	.word	0x0171
;;;	.word	0x6c18

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0

	test_h_gr32	byte_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_b_predec_to_reg8:		; pre-decrement from mem to register
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @-ers, rd

	mov.l	#byte_src+1, er1
	mov.b	@-er1, r0l	; Register pre-decr operand
;;;	.word	0x0173
;;;	.word	0x6c18

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0

	test_h_gr32	byte_src, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	
mov_b_disp2_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @(dd:2, ers), rd
	mov.l	#byte_src-1, er1
	mov.b	@(1:2, er1), r0l	; Register plus 2-bit disp. operand
;;; 	.word	0x0171
;;; 	.word	0x6818

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0	; mov result:	a5a5 | 7777

	test_h_gr32	byte_src-1, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

mov_b_disp16_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @(dd:16, ers), rd
	mov.l	#byte_src+0x1234, er1
	mov.b	@(-0x1234:16, er1), r0l	; Register plus 16-bit disp. operand
;;;	.word	0x6e18
;;;	.word	-0x1234

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0	; mov result:	a5a5 | 7777

	test_h_gr32	byte_src+0x1234, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_b_disp32_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @(dd:32, ers), rd
	mov.l	#byte_src+65536, er1
	mov.b	@(-65536:32, er1), r0l	; Register plus 32-bit disp. operand
;;;	.word	0x7810
;;;	.word	0x6a28
;;;	.long	-65536

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0	; mov result:	a5a5 | 7777

	test_h_gr32	byte_src+65536, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
mov_b_indexb16_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffffff01, er1
	set_ccr_zero
	;; mov.b @(dd:16, rs.b), rd
	mov.b	@(byte_src-1:16, r1.b), r0l	; indexed byte operand

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0	; mov result:	a5a5a5 | 77

	test_h_gr32  0xffffff01, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_b_indexw16_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffff0002, er1
	set_ccr_zero
	;; mov.b @(dd:16, rs.w), rd
	mov.b	@(byte_src-2:16, r1.w), r0l	; indexed byte operand

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0	; mov result:	a5a5a5 | 77

	test_h_gr32  0xffff0002, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_b_indexl16_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0x00000003, er1
	set_ccr_zero
	;; mov.b @(dd:16, ers.l), rd
	mov.b	@(byte_src-3:16, er1.l), r0l	; indexed byte operand

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0	; mov result:	a5a5a5 | 77

	test_h_gr32  0x00000003, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_b_indexb32_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffffff04, er1
	set_ccr_zero
	;; mov.b @(dd:32, rs.b), rd
	mov.b	@(byte_src-4:32, r1.b), r0l	; indexed byte operand

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0	; mov result:	a5a5 | 7777

	test_h_gr32  0xffffff04 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_b_indexw32_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffff0005, er1
	set_ccr_zero
	;; mov.b @(dd:32, rs.w), rd
	mov.b	@(byte_src-5:32, r1.w), r0l	; indexed byte operand

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0	; mov result:	a5a5 | 7777

	test_h_gr32  0xffff0005 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_b_indexl32_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0x00000006, er1
	set_ccr_zero
	;; mov.b @(dd:32, ers.l), rd
	mov.b	@(byte_src-6:32, er1.l), r0l	; indexed byte operand

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0	; mov result:	a5a5 | 7777

	test_h_gr32  0x00000006 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.endif

.if (sim_cpu == h8sx)
mov_b_abs8_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#byte_src-255, er1
	ldc	er1, sbr
	set_ccr_zero
	;; mov.b @aa:8, rd
	mov.b	@0xff:8, r0l	; 8-bit (sbr relative) address-direct operand

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0

	test_h_gr32  byte_src-255, er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

mov_b_abs16_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @aa:16, rd
	mov.b	@byte_src:16, r0l	; 16-bit address-direct operand
;;;	.word	0x6a08
;;;	.word	@byte_src

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mov_b_abs32_to_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @aa:32, rd
	mov.b	@byte_src:32, r0l	; 32-bit address-direct operand
;;;	.word	0x6a28
;;;	.long	@byte_src

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	test_h_gr32 0xa5a5a577 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)

	;;
	;; Move byte from memory to memory
	;; 

mov_b_indirect_to_indirect:	; reg indirect, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @ers, @erd

	mov.l	#byte_src, er1
	mov.l	#byte_dst, er0
	mov.b	@er1, @er0
;;;	.word	0x0178
;;;	.word	0x0100

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  byte_dst er0
	test_h_gr32  byte_src er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	beq	.Lnext55
	fail
.Lnext55:
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	bne	.Lnext56
	fail
.Lnext56:			; OK, pass on.

mov_b_postinc_to_postinc:	; reg post-increment, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @ers+, @erd+

	mov.l	#byte_src, er1
	mov.l	#byte_dst, er0
	mov.b	@er1+, @er0+
;;;	.word	0x0178
;;;	.word	0x8180

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  byte_dst+1 er0
	test_h_gr32  byte_src+1 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	beq	.Lnext65
	fail
.Lnext65:
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	bne	.Lnext66
	fail
.Lnext66:			; OK, pass on.

mov_b_postdec_to_postdec:	; reg post-decrement, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @ers-, @erd-

	mov.l	#byte_src, er1
	mov.l	#byte_dst, er0
	mov.b	@er1-, @er0-
;;;	.word	0x0178
;;;	.word	0xa1a0

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  byte_dst-1 er0
	test_h_gr32  byte_src-1 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	beq	.Lnext75
	fail
.Lnext75:
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	bne	.Lnext76
	fail
.Lnext76:			; OK, pass on.

mov_b_preinc_to_preinc:		; reg pre-increment, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @+ers, @+erd

	mov.l	#byte_src-1, er1
	mov.l	#byte_dst-1, er0
	mov.b	@+er1, @+er0
;;;	.word	0x0178
;;;	.word	0x9190

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  byte_dst er0
	test_h_gr32  byte_src er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	beq	.Lnext85
	fail
.Lnext85:
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	bne	.Lnext86
	fail
.Lnext86:				; OK, pass on.

mov_b_predec_to_predec:		; reg pre-decrement, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @-ers, @-erd

	mov.l	#byte_src+1, er1
	mov.l	#byte_dst+1, er0
	mov.b	@-er1, @-er0
;;;	.word	0x0178
;;;	.word	0xb1b0

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  byte_dst er0
	test_h_gr32  byte_src er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	beq	.Lnext95
	fail
.Lnext95:
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	bne	.Lnext96
	fail
.Lnext96:			; OK, pass on.

mov_b_disp2_to_disp2:		; reg 2-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @(dd:2, ers), @(dd:2, erd)

	mov.l	#byte_src-1, er1
	mov.l	#byte_dst-2, er0
	mov.b	@(1:2, er1), @(2:2, er0)
;;; 	.word	0x0178
;;; 	.word	0x1120

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  byte_dst-2 er0
	test_h_gr32  byte_src-1 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	beq	.Lnext105
	fail
.Lnext105:
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	bne	.Lnext106
	fail
.Lnext106:			; OK, pass on.

mov_b_disp16_to_disp16:		; reg 16-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @(dd:16, ers), @(dd:16, erd)

	mov.l	#byte_src-1, er1
	mov.l	#byte_dst-2, er0
	mov.b	@(1:16, er1), @(2:16, er0)
;;; 	.word	0x0178
;;; 	.word	0xc1c0
;;; 	.word	0x0001
;;; 	.word	0x0002

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  byte_dst-2 er0
	test_h_gr32  byte_src-1 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	beq	.Lnext115
	fail
.Lnext115:
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	bne	.Lnext116
	fail
.Lnext116:			; OK, pass on.

mov_b_disp32_to_disp32:		; reg 32-bit disp, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @(dd:32, ers), @(dd:32, erd)

	mov.l	#byte_src-1, er1
	mov.l	#byte_dst-2, er0
	mov.b	@(1:32, er1), @(2:32, er0)
;;; 	.word	0x0178
;;; 	.word	0xc9c8
;;;	.long	1
;;;	.long	2

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  byte_dst-2 er0
	test_h_gr32  byte_src-1 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	beq	.Lnext125
	fail
.Lnext125:
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	bne	.Lnext126
	fail
.Lnext126:				; OK, pass on.

mov_b_indexb16_to_indexb16:	; reg 16-bit indexed, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffffff01, er1
	mov.l	#0xffffff02, er0
	;; mov.b @(dd:16, rs.b), @(dd:16, rd.b)
	set_ccr_zero
	mov.b	@(byte_src-1:16, r1.b), @(byte_dst-2:16, r0.b)

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  0xffffff02 er0
	test_h_gr32  0xffffff01 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	bne	fail1
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	beq	fail1

mov_b_indexw16_to_indewb16:	; reg 16-bit indexed, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffff0003, er1
	mov.l	#0xffff0004, er0
	;; mov.b @(dd:16, rs.w), @(dd:16, rd.w)
	set_ccr_zero
	mov.b	@(byte_src-3:16, r1.w), @(byte_dst-4:16, r0.w)

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  0xffff0004 er0
	test_h_gr32  0xffff0003 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	bne	fail1
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	beq	fail1

mov_b_indexl16_to_indexl16:	; reg 16-bit indexed, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0x00000005, er1
	mov.l	#0x00000006, er0
	;; mov.b @(dd:16, ers.l), @(dd:16, erd.l)
	set_ccr_zero
	mov.b	@(byte_src-5:16, er1.l), @(byte_dst-6:16, er0.l)

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  0x00000006 er0
	test_h_gr32  0x00000005 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	bne	fail1
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	beq	fail1

mov_b_indexb32_to_indexb32:	; reg 32-bit indexed, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffffff01, er1
	mov.l	#0xffffff02, er0
	set_ccr_zero
	;; mov.b @(dd:32, rs.b), @(dd:32, rd.b)
	mov.b	@(byte_src-1:32, r1.b), @(byte_dst-2:32, r0.b)

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  0xffffff02 er0
	test_h_gr32  0xffffff01 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	bne	fail1
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	beq	fail1

mov_b_indexw32_to_indexw32:	; reg 32-bit indexed, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0xffff0003, er1
	mov.l	#0xffff0004, er0
	set_ccr_zero
	;; mov.b @(dd:32, rs.w), @(dd:32, rd.w)
	mov.b	@(byte_src-3:32, r1.w), @(byte_dst-4:32, r0.w)

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  0xffff0004 er0
	test_h_gr32  0xffff0003 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	bne	fail1
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	beq	fail1

mov_b_indexl32_to_indexl32:	; reg 32-bit indexed, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#0x00000005, er1
	mov.l	#0x00000006, er0
	set_ccr_zero
	;; mov.b @(dd:32, rs.w), @(dd:32, rd.w)
	mov.b	@(byte_src-5:32, er1.l), @(byte_dst-6:32, er0.l)

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_neg_clear
	test_zero_clear
	test_ovf_clear
	test_carry_clear

	;; Verify the affected registers.

	test_h_gr32  0x00000006 er0
	test_h_gr32  0x00000005 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the move to memory.
	cmp.b	@byte_src, @byte_dst
	bne	fail1
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	beq	fail1

mov_b_abs16_to_abs16:		; 16-bit absolute addr, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @aa:16, @aa:16

	mov.b	@byte_src:16, @byte_dst:16
;;; 	.word	0x0178
;;; 	.word	0x4040
;;;	.word	@byte_src
;;;	.word	@byte_dst

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
	cmp.b	@byte_src, @byte_dst
	beq	.Lnext135
	fail
.Lnext135:
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	bne	.Lnext136
	fail
.Lnext136:				; OK, pass on.

mov_b_abs32_to_abs32:		; 32-bit absolute addr, memory to memory
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;; mov.b @aa:32, @aa:32

	mov.b	@byte_src:32, @byte_dst:32
;;; 	.word	0x0178
;;; 	.word	0x4848
;;;	.long	@byte_src
;;;	.long	@byte_dst

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
	cmp.b	@byte_src, @byte_dst
	beq	.Lnext145
	fail
.Lnext145:
	;; Now clear the destination location, and verify that.
	mov.b	#0, @byte_dst
	cmp.b	@byte_src, @byte_dst
	bne	.Lnext146
	fail
.Lnext146:				; OK, pass on.


.endif

	pass

	exit 0

fail1:
	fail
	