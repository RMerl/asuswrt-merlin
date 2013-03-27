# Hitachi H8 testcase 'not.b, not.w, not.l'
# mach(): all
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf	
# ld(h8300s):	-m h8300self	
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"

	# Instructions tested:
	# not.b rd	;                     1 7 0  rd
	# not.b @erd	;         7 d rd ???? 1 7 0  ignore
	# not.b @erd+	; 0 1 7 4 6 c rd 1??? 1 7 0  ignore
	# not.b @erd-	; 0 1 7 6 6 c rd 1??? 1 7 0  ignore
	# not.b @+erd	; 0 1 7 5 6 c rd 1??? 1 7 0  ignore
	# not.b @-erd	; 0 1 7 7 6 c rd 1??? 1 7 0  ignore
	# not.b @(d:2,  erd)	; 0 1 7 01dd  6 8 rd 8 1 7 0  ignore
	# not.b @(d:16, erd)	; 0 1 7  4 6 e rd 1??? dd:16 1 7 0  ignore
	# not.b @(d:32, erd)	; 7 8 rd 4 6 a  2 1??? dd:32 1 7 0  ignore
	# not.b @aa:16		; 6 a 1 1??? aa:16 1 7 0  ignore
	# not.b @aa:32		; 6 a 3 1??? aa:32 1 7 0  ignore
	# word operations
	# long operations
	#
	# Coming soon:
	# not.b @aa:8		; 7 f aaaaaaaa 1 7 0  ignore
	#

.data
byte_dest:	.byte 0xa5
	.align 2
word_dest:	.word 0xa5a5
	.align 4
long_dest:	.long 0xa5a5a5a5
	start

	#
	# 8-bit byte operations 
	#

not_b_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  not.b Rd
	not	r0l		; 8-bit register
;;;	.word	0x1708
	
	cmp.b	#0x5a, r0l	; result of "not 0xa5"
	beq	.Lbrd
	fail
.Lbrd:	
	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa55a r0	; r0 changed by 'not'
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a55a er0	; er0 changed by 'not' 
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
not_b_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.b @eRd
	mov	#byte_dest, er0
	not.b	@er0		; register indirect operand
;;;	.word	0x7d00
;;;	.word	0x1700

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_h_gr32 byte_dest er0	; er0 still contains address
	cmp.b	#0x5a:8, @er0	; memory contents changed
	beq	.Lbind
	fail
.Lbind:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_b_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.b @eRd+
	mov	#byte_dest, er0	; register post-increment operand
	not.b	@er0+
;;;	.word	0x0174
;;;	.word	0x6c08
;;;	.word	0x1700

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_dest+1 er0	; er0 contains address plus one
	cmp.b	#0xa5:8, @-er0
	beq	.Lbpostinc
	fail
.Lbpostinc:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_b_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.b @eRd-
	mov	#byte_dest, er0	; register post-decrement operand
	not.b	@er0-
;;;	.word	0x0176
;;;	.word	0x6c08
;;;	.word	0x1700

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dest-1 er0	; er0 contains address minus one
	cmp.b	#0x5a:8, @+er0
;;; 	.word	0x0175
;;; 	.word	0x6c08
;;; 	.word	0xa05a
	beq	.Lbpostdec
	fail
.Lbpostdec:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_b_rdpreinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.b @+eRd
	mov	#byte_dest-1, er0
	not.b	@+er0			; reg pre-increment operand
;;;	.word	0x0175
;;;	.word	0x6c08
;;;	.word	0x1700

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.b	#0xa5:8, @er0
	beq	.Lbpreinc
	fail
.Lbpreinc:
	test_h_gr32 byte_dest er0	; er0 contains destination address 
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_b_rdpredec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.b @-eRd
	mov	#byte_dest+1, er0
	not.b	@-er0		; reg pre-decr operand
;;;	.word	0x0177
;;;	.word	0x6c08
;;;	.word	0x1700

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.b	#0x5a:8, @er0
	beq	.Lbpredec
	fail
.Lbpredec:
	test_h_gr32 byte_dest er0	; er0 contains destination address 
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_b_disp2dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.b @(dd:2, erd)
	mov	#byte_dest-1, er0
	not.b	@(1:2, er0)	; reg plus 2-bit displacement
;;; 	.word	0x0175
;;; 	.word	0x6808
;;; 	.word	0x1700

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.b	#0xa5:8, @+er0
	beq	.Lbdisp2
	fail
.Lbdisp2:
	test_h_gr32 byte_dest er0	; er0 contains destination address 
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_b_disp16dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.b @(dd:16, erd)
	mov	#byte_dest+100, er0
	not.b	@(-100:16, er0)	; reg plus 16-bit displacement
;;;	.word	0x0174
;;;	.word	0x6e08
;;;	.word	-100
;;;	.word	0x1700

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.b	#0x5a:8, @byte_dest
	beq	.Lbdisp16
	fail
.Lbdisp16:
	test_h_gr32 byte_dest+100 er0	; er0 contains destination address 
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_b_disp32dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.b @(dd:32, erd)
	mov	#byte_dest-0xfffff, er0
	not.b	@(0xfffff:32, er0)	; reg plus 32-bit displacement
;;;	.word	0x7804
;;;	.word	0x6a28
;;;	.long	0xfffff
;;;	.word	0x1700

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.b	#0xa5:8, @byte_dest
	beq	.Lbdisp32
	fail
.Lbdisp32:
	test_h_gr32 byte_dest-0xfffff er0 ; er0 contains destination address
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_b_abs16dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.b @aa:16
	not.b	@byte_dest:16	; 16-bit absolute address
;;;	.word	0x6a18
;;;	.word	byte_dest
;;;	.word	0x1700

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.b	#0x5a:8, @byte_dest
	beq	.Lbabs16
	fail
.Lbabs16:
	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_b_abs32dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.b @aa:32
	not.b	@byte_dest:32	; 32-bit absolute address
;;;	.word	0x6a38
;;;	.long	byte_dest
;;;	.word	0x1700

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.b	#0xa5:8, @byte_dest
	beq	.Lbabs32
	fail
.Lbabs32:
	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

	#
	# 16-bit word operations
	#

.if (sim_cpu)			; any except plain-vanilla h8/300
not_w_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  not.w Rd
	not	r1		; 16-bit register operand
;;;	.word	0x1711
	
	cmp.w	#0x5a5a, r1	; result of "not 0xa5a5"
	beq	.Lwrd
	fail
.Lwrd:	
	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr32 0xa5a55a5a er1	; er1 changed by 'not' 
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
not_w_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.w @eRd
	mov	#word_dest, er1
	not.w	@er1		; register indirect operand
;;;	.word	0x0154
;;;	.word	0x6d18
;;;	.word	0x1710

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.w	#0x5a5a, @word_dest	; memory contents changed
	beq	.Lwind
	fail
.Lwind:
	test_h_gr32 word_dest er1	; er1 still contains address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_w_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.w @eRd+
	mov	#word_dest, er1	; register post-increment operand
	not.w	@er1+
;;;	.word	0x0154
;;;	.word	0x6d18
;;;	.word	0x1710

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.w	#0xa5a5, @word_dest
	beq	.Lwpostinc
	fail
.Lwpostinc:
	test_h_gr32 word_dest+2 er1	; er1 contains address plus two
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_w_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.w @eRd-
	mov	#word_dest, er1
	not.w	@er1-
;;;	.word	0x0156
;;;	.word	0x6d18
;;;	.word	0x1710

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.w	#0x5a5a, @word_dest
	beq	.Lwpostdec
	fail
.Lwpostdec:
	test_h_gr32 word_dest-2 er1	; er1 contains address minus two
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_w_rdpreinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.w @+eRd
	mov	#word_dest-2, er1
	not.w	@+er1		; reg pre-increment operand
;;;	.word	0x0155
;;;	.word	0x6d18
;;;	.word	0x1710

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.w	#0xa5a5, @word_dest
	beq	.Lwpreinc
	fail
.Lwpreinc:
	test_h_gr32 word_dest er1	; er1 contains destination address 
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_w_rdpredec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.w @-eRd
	mov	#word_dest+2, er1
	not.w	@-er1		; reg pre-decr operand
;;;	.word	0x0157
;;;	.word	0x6d18
;;;	.word	0x1710

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.w	#0x5a5a, @word_dest
	beq	.Lwpredec
	fail
.Lwpredec:
	test_h_gr32 word_dest er1	; er1 contains destination address 
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_w_disp2dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.w @(dd:2, erd)
	mov	#word_dest-2, er1
	not.w	@(2:2, er1)	; reg plus 2-bit displacement
;;; 	.word	0x0155
;;; 	.word	0x6918
;;; 	.word	0x1710

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.w	#0xa5a5, @word_dest
	beq	.Lwdisp2
	fail
.Lwdisp2:
	test_h_gr32 word_dest-2 er1	; er1 contains address minus one
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_w_disp16dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.w @(dd:16, erd)
	mov	#word_dest+100, er1
	not.w	@(-100:16, er1)	; reg plus 16-bit displacement
;;;	.word	0x0154
;;;	.word	0x6f18
;;;	.word	-100
;;;	.word	0x1710

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.w	#0x5a5a, @word_dest
	beq	.Lwdisp16
	fail
.Lwdisp16:
	test_h_gr32 word_dest+100 er1	; er1 contains destination address 
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_w_disp32dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.w @(dd:32, erd)
	mov	#word_dest-0xfffff, er1
	not.w	@(0xfffff:32, er1)	; reg plus 32-bit displacement
;;;	.word	0x7814
;;;	.word	0x6b28
;;;	.long	0xfffff
;;;	.word	0x1710

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.w	#0xa5a5, @word_dest
	beq	.Lwdisp32
	fail
.Lwdisp32:
	test_h_gr32 word_dest-0xfffff er1 ; er1 contains destination address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_w_abs16dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.w @aa:16
	not.w	@word_dest:16	; 16-bit absolute address
;;;	.word	0x6b18
;;;	.word	word_dest
;;;	.word	0x1710

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.w	#0x5a5a, @word_dest
	beq	.Lwabs16
	fail
.Lwabs16:
	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_w_abs32dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.w @aa:32
	not.w	@word_dest:32	; 32-bit absolute address
;;;	.word	0x6b38
;;;	.long	word_dest
;;;	.word	0x1710

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.w	#0xa5a5, @word_dest
	beq	.Lwabs32
	fail
.Lwabs32:
	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.endif				; h8sx
.endif				; h8/300

	#
	# 32-bit word operations
	#

.if (sim_cpu)			; any except plain-vanilla h8/300
not_l_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  not.l eRd
	not	er1		; 32-bit register operand
;;;	.word	0x1731

	cmp.l	#0x5a5a5a5a, er1	; result of "not 0xa5a5a5a5"
	beq	.Llrd
	fail
.Llrd:	
	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr32 0x5a5a5a5a er1	; er1 changed by 'not' 
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
not_l_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.l @eRd
	mov	#long_dest, er1
	not.l	@er1		; register indirect operand
;;;	.word	0x0104
;;;	.word	0x6d18
;;;	.word	0x1730

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.l	#0x5a5a5a5a, @long_dest	; memory contents changed
	beq	.Llind
	fail
.Llind:
	test_h_gr32 long_dest er1	; er1 still contains address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_l_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.l @eRd+
	mov	#long_dest, er1	; register post-increment operand
	not.l	@er1+
;;;	.word	0x0104
;;;	.word	0x6d18
;;;	.word	0x1730

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.l	#0xa5a5a5a5, @long_dest
	beq	.Llpostinc
	fail
.Llpostinc:
	test_h_gr32 long_dest+4 er1	; er1 contains address plus two
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_l_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.l @eRd-
	mov	#long_dest, er1
	not.l	@er1-
;;;	.word	0x0106
;;;	.word	0x6d18
;;;	.word	0x1730

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.l	#0x5a5a5a5a, @long_dest
	beq	.Llpostdec
	fail
.Llpostdec:
	test_h_gr32 long_dest-4 er1	; er1 contains address minus two
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_l_rdpreinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.l @+eRd
	mov	#long_dest-4, er1
	not.l	@+er1		; reg pre-increment operand
;;;	.word	0x0105
;;;	.word	0x6d18
;;;	.word	0x1730

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.l	#0xa5a5a5a5, @long_dest
	beq	.Llpreinc
	fail
.Llpreinc:
	test_h_gr32 long_dest er1	; er1 contains destination address 
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_l_rdpredec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.l @-eRd
	mov	#long_dest+4, er1
	not.l	@-er1		; reg pre-decr operand
;;;	.word	0x0107
;;;	.word	0x6d18
;;;	.word	0x1730

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.l	#0x5a5a5a5a, @long_dest
	beq	.Llpredec
	fail
.Llpredec:
	test_h_gr32 long_dest er1	; er1 contains destination address 
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_l_disp2dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.l @(dd:2, erd)
	mov	#long_dest-4, er1
	not.l	@(4:2, er1)	; reg plus 2-bit displacement
;;; 	.word	0x0105
;;; 	.word	0x6918
;;; 	.word	0x1730

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.l	#0xa5a5a5a5, @long_dest
	beq	.Lldisp2
	fail
.Lldisp2:
	test_h_gr32 long_dest-4 er1	; er1 contains address minus one
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_l_disp16dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.l @(dd:16, erd)
	mov	#long_dest+100, er1
	not.l	@(-100:16, er1)	; reg plus 16-bit displacement
;;;	.word	0x0104
;;;	.word	0x6f18
;;;	.word	-100
;;;	.word	0x1730

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.l	#0x5a5a5a5a, @long_dest
	beq	.Lldisp16
	fail
.Lldisp16:
	test_h_gr32 long_dest+100 er1	; er1 contains destination address 
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_l_disp32dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.l @(dd:32, erd)
	mov	#long_dest-0xfffff, er1
	not.l	@(0xfffff:32, er1)	; reg plus 32-bit displacement
;;;	.word	0x7894
;;;	.word	0x6b28
;;;	.long	0xfffff
;;;	.word	0x1730

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.l	#0xa5a5a5a5, @long_dest
	beq	.Lldisp32
	fail
.Lldisp32:
	test_h_gr32 long_dest-0xfffff er1 ; er1 contains destination address
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_l_abs16dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.l @aa:16
	not.l	@long_dest:16	; 16-bit absolute address
;;;	.word	0x0104
;;;	.word	0x6b08
;;;	.word	long_dest
;;;	.word	0x1730

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	cmp.l	#0x5a5a5a5a, @long_dest
	beq	.Llabs16
	fail
.Llabs16:
	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

not_l_abs32dst:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  not.l @aa:32
	not.l	@long_dest:32	; 32-bit absolute address
;;;	.word	0x0104
;;;	.word	0x6b28
;;;	.long	long_dest
;;;	.word	0x1730

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	cmp.l	#0xa5a5a5a5, @long_dest
	beq	.Llabs32
	fail
.Llabs32:
	test_gr_a5a5 0		; Make sure ALL general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.endif				; h8sx
.endif				; h8/300

	pass

	exit 0
