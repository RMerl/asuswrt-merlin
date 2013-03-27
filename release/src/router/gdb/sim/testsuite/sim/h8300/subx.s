# Hitachi H8 testcase 'subx'
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
	# subx.b #xx:8, rd8	; b rd8 xxxxxxxx
	# subx.b #xx:8, @erd	; 7 d erd ???? b ???? xxxxxxxx 
	# subx.b #xx:8, @erd-	; 0 1 7 6 6 c erd 1??? b ???? xxxxxxxx
	# subx.b rs8, rd8	; 1 e rs8 rd8
	# subx.b rs8, @erd	; 7 d erd ???? 1 e rs8 ????
	# subx.b rs8, @erd-	; 0 1 7 6 6 c erd 1??? 1 e rs8 ????
	# subx.b @ers, rd8	; 7 c ers ???? 1 e ???? rd8
	# subx.b @ers-, rd8	; 0 1 7 6 6 c ers 00?? 1 e ???? rd8
	# subx.b @ers, @erd	; 0 1 7 4 6 8 ers d 0 erd 3 ???? 
	# subx.b @ers-, @erd-	; 0 1 7 6 6 c ers d a erd 3 ????
	#
	# word ops
	# long ops	

.data
byte_src:	.byte 0x5
byte_dest:	.byte 0

	.align 2
word_src:	.word 0x505
word_dest:	.word 0

	.align 4
long_src:	.long 0x50505
long_dest:	.long 0


	start

subx_b_imm8_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  subx.b #xx:8,Rd	; Subx with carry initially zero.
	subx.b	#5, r0l		; Immediate 8-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0xa5a0 r0	; sub result:	a5 - 5
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a5a0 er0	; sub result:	 a5 - 5
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
subx_b_imm8_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  subx.b #xx:8,Rd	; Subx with carry initially one.
	set_carry_flag
	subx.b	#4, r0l		; Immediate 8-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0xa5a0 r0	; sub result:	a5 - (4 + 1)
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a5a0 er0	; sub result:	 a5 - (4 + 1)
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8sx)
subx_b_imm8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.b #xx:8,@eRd	; Subx to register indirect
	mov	#byte_dest, er0
	mov.b	#0xa5, @er0
	set_ccr_zero
	subx.b	#5, @er0

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_dest er0	; er0 still contains subress

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.b	#0xa0, @byte_dest
	beq	.Lb1
	fail
.Lb1:

subx_b_imm8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.b #xx:8,@eRd-	; Subx to register post-decrement
	mov	#byte_dest, er0
	mov.b	#0xa5, @er0
	set_ccr_zero
	subx.b	#5, @er0-

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_dest-1 er0	; er0 contains subress minus one

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.b	#0xa0, @byte_dest
	beq	.Lb2
	fail
.Lb2:
.endif

subx_b_reg8_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.b Rs,Rd	; subx with carry initially zero
	mov.b	#5, r0h
	set_ccr_zero
	subx.b	r0h, r0l	; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0x05a0 r0	; sub result:	a5 - 5
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a505a0 er0	; sub result:	a5 - 5
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subx_b_reg8_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.b Rs,Rd	; subx with carry initially one
	mov.b	#4, r0h
	set_ccr_zero
	set_carry_flag
	subx.b	r0h, r0l	; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0x04a0 r0	; sub result:	a5 - (4 + 1)
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a504a0 er0	; sub result:	a5 - (4 + 1)
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8sx)
subx_b_reg8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.b rs8,@eRd	; Subx to register indirect
	mov	#byte_dest, er0
	mov.b	#0xa5, @er0
	mov.b	#5, r1l
	set_ccr_zero
	subx.b	r1l, @er0

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_dest er0	; er0 still contains subress
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.b	#0xa0, @byte_dest
	beq	.Lb3
	fail
.Lb3:

subx_b_reg8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.b rs8,@eRd-	; Subx to register post-decrement
	mov	#byte_dest, er0
	mov.b	#0xa5, @er0
	mov.b	#5, r1l
	set_ccr_zero
	subx.b	r1l, @er0-

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_dest-1 er0	; er0 contains subress minus one
	test_h_gr32 0xa5a5a505 er1	; er1 contains the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.b	#0xa0, @byte_dest
	beq	.Lb4
	fail
.Lb4:

subx_b_rsind_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.b @eRs,rd8	; Subx from reg indirect to reg
	mov	#byte_src, er0
	set_ccr_zero
	subx.b	@er0, r1l

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_src er0	; er0 still contains subress
	test_h_gr32 0xa5a5a5a0 er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subx_b_rspostdec_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.b @eRs-,rd8	; Subx to register post-decrement
	mov	#byte_src, er0
	set_ccr_zero
	subx.b	@er0-, r1l

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_src-1 er0	; er0 contains subress minus one
	test_h_gr32 0xa5a5a5a0 er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subx_b_rsind_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.b @eRs,rd8	; Subx from reg indirect to reg
	mov	#byte_src, er0
	mov	#byte_dest, er1
	mov.b	#0xa5, @er1
	set_ccr_zero
	subx.b	@er0, @er1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_src er0	; er0 still contains src subress
	test_h_gr32 byte_dest er1	; er1 still contains dst subress

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the sub to memory.
	cmp.b	#0xa0, @byte_dest
	beq	.Lb5
	fail
.Lb5:

subx_b_rspostdec_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	mov	#byte_src, er0
	mov	#byte_dest, er1
	mov.b	#0xa5, @er1
	set_ccr_zero
	;;  subx.b @eRs-,@erd-	; Subx post-decrement to post-decrement
	subx.b	@er0-, @er1-

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_src-1 er0	; er0 contains src subress minus one
	test_h_gr32 byte_dest-1 er1	; er1 contains dst subress minus one

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the sub to memory.
	cmp.b	#0xa0, @byte_dest
	beq	.Lb6
	fail
.Lb6:

subx_w_imm16_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  subx.w #xx:16,Rd	; Subx with carry initially zero.
	subx.w	#0x505, r0	; Immediate 16-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0xa0a0 r0	; sub result:	0xa5a5 + 0x505
	test_h_gr32 0xa5a5a0a0 er0	; sub result:	 0xa5a5 + 0x505
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
subx_w_imm16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  subx.w #xx:16,Rd	; Subx with carry initially one.
	set_carry_flag
	subx.w	#0x504, r0	; Immediate 16-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0xa0a0 r0	; sub result:	0xa5a5 + 0x505 + 1
	test_h_gr32 0xa5a5a0a0 er0	; sub result:	 0xa5a5 + 0x505 + 1
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
subx_w_imm16_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.w #xx:16,@eRd	; Subx to register indirect
	mov	#word_dest, er0
	mov.w	#0xa5a5, @er0
	set_ccr_zero
	subx.w	#0x505, @er0

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 word_dest er0	; er0 still contains subress

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.w	#0xa0a0, @word_dest
	beq	.Lw1
	fail
.Lw1:

subx_w_imm16_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.w #xx:16,@eRd-	; Subx to register post-decrement
	mov	#word_dest, er0
	mov.w	#0xa5a5, @er0
	set_ccr_zero
	subx.w	#0x505, @er0-

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 word_dest-2 er0	; er0 contains subress minus one

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.w	#0xa0a0, @word_dest
	beq	.Lw2
	fail
.Lw2:

subx_w_reg16_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.w Rs,Rd	; subx with carry initially zero
	mov.w	#0x505, e0
	set_ccr_zero
	subx.w	e0, r0		; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0x0505a0a0 er0	; sub result:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subx_w_reg16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.w Rs,Rd	; subx with carry initially one
	mov.w	#0x504, e0
	set_ccr_zero
	set_carry_flag
	subx.w	e0, r0		; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0x0504a0a0 er0	; sub result:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
subx_w_reg16_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.w rs8,@eRd	; Subx to register indirect
	mov	#word_dest, er0
	mov.w	#0xa5a5, @er0
	mov.w	#0x505, r1
	set_ccr_zero
	subx.w	r1, @er0

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 word_dest er0	; er0 still contains subress
	test_h_gr32 0xa5a50505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.w	#0xa0a0, @word_dest
	beq	.Lw3
	fail
.Lw3:

subx_w_reg16_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.w rs8,@eRd-	; Subx to register post-decrement
	mov	#word_dest, er0
	mov.w	#0xa5a5, @er0
	mov.w	#0x505, r1
	set_ccr_zero
	subx.w	r1, @er0-

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 word_dest-2 er0	; er0 contains subress minus one
	test_h_gr32 0xa5a50505  er1	; er1 contains the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.w	#0xa0a0, @word_dest
	beq	.Lw4
	fail
.Lw4:

subx_w_rsind_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.w @eRs,rd8	; Subx from reg indirect to reg
	mov	#word_src, er0
	set_ccr_zero
	subx.w	@er0, r1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 word_src er0	; er0 still contains subress
	test_h_gr32 0xa5a5a0a0 er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subx_w_rspostdec_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.w @eRs-,rd8	; Subx to register post-decrement
	mov	#word_src, er0
	set_ccr_zero
	subx.w	@er0-, r1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 word_src-2 er0	; er0 contains subress minus one
	test_h_gr32 0xa5a5a0a0 er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subx_w_rsind_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.w @eRs,rd8	; Subx from reg indirect to reg
	mov	#word_src, er0
	mov	#word_dest, er1
	mov.w	#0xa5a5, @er1
	set_ccr_zero
	subx.w	@er0, @er1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 word_src er0	; er0 still contains src subress
	test_h_gr32 word_dest er1	; er1 still contains dst subress

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the sub to memory.
	cmp.w	#0xa0a0, @word_dest
	beq	.Lw5
	fail
.Lw5:

subx_w_rspostdec_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.w @eRs-,rd8	; Subx to register post-decrement
	mov	#word_src, er0
	mov	#word_dest, er1
	mov.w	#0xa5a5, @er1
	set_ccr_zero
	subx.w	@er0-, @er1-

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 word_src-2 er0	; er0 contains src subress minus one
	test_h_gr32 word_dest-2 er1	; er1 contains dst subress minus one

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the sub to memory.
	cmp.w	#0xa0a0, @word_dest
	beq	.Lw6
	fail
.Lw6:

subx_l_imm32_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  subx.l #xx:32,Rd	; Subx with carry initially zero.
	subx.l	#0x50505, er0	; Immediate 32-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0xa5a0a0a0 er0	; sub result:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
subx_l_imm32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  subx.l #xx:32,Rd	; Subx with carry initially one.
	set_carry_flag
	subx.l	#0x50504, er0	; Immediate 32-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0xa5a0a0a0 er0	; sub result:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
subx_l_imm32_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.l #xx:32,@eRd	; Subx to register indirect
	mov	#long_dest, er0
	mov.l	#0xa5a5a5a5, @er0
	set_ccr_zero
	subx.l	#0x50505, @er0

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 long_dest er0	; er0 still contains subress

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.l	#0xa5a0a0a0, @long_dest
	beq	.Ll1
	fail
.Ll1:

subx_l_imm32_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.l #xx:32,@eRd-	; Subx to register post-decrement
	mov	#long_dest, er0
	mov.l	#0xa5a5a5a5, @er0
	set_ccr_zero
	subx.l	#0x50505, @er0-

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 long_dest-4 er0	; er0 contains subress minus one

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.l	#0xa5a0a0a0, @long_dest
	beq	.Ll2
	fail
.Ll2:

subx_l_reg32_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.l Rs,Rd	; subx with carry initially zero
	mov.l	#0x50505, er0
	set_ccr_zero
	subx.l	er0, er1	; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0x50505    er0	; sub load
	test_h_gr32 0xa5a0a0a0 er1	; sub result:
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subx_l_reg32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.l Rs,Rd	; subx with carry initially one
	mov.l	#0x50504, er0
	set_ccr_zero
	set_carry_flag
	subx.l	er0, er1	; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0x50504    er0	; sub result:
	test_h_gr32 0xa5a0a0a0 er1	; sub result:
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
subx_l_reg32_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.l rs8,@eRd	; Subx to register indirect
	mov	#long_dest, er0
	mov.l	er1, @er0
	mov.l	#0x50505, er1
	set_ccr_zero
	subx.l	er1, @er0

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 long_dest er0	; er0 still contains subress
	test_h_gr32 0x50505   er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.l	#0xa5a0a0a0, @long_dest
	beq	.Ll3
	fail
.Ll3:

subx_l_reg32_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.l rs8,@eRd-	; Subx to register post-decrement
	mov	#long_dest, er0
	mov.l	er1, @er0
	mov.l	#0x50505, er1
	set_ccr_zero
	subx.l	er1, @er0-

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 long_dest-4 er0	; er0 contains subress minus one
	test_h_gr32 0x50505     er1	; er1 contains the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	cmp.l	#0xa5a0a0a0, @long_dest
	beq	.Ll4
	fail
.Ll4:

subx_l_rsind_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.l @eRs,rd8	; Subx from reg indirect to reg
	mov	#long_src, er0
	set_ccr_zero
	subx.l	@er0, er1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 long_src er0	; er0 still contains subress
	test_h_gr32 0xa5a0a0a0 er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subx_l_rspostdec_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.l @eRs-,rd8	; Subx to register post-decrement
	mov	#long_src, er0
	set_ccr_zero
	subx.l	@er0-, er1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 long_src-4 er0	; er0 contains subress minus one
	test_h_gr32 0xa5a0a0a0 er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subx_l_rsind_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.l @eRs,rd8	; Subx from reg indirect to reg
	mov	#long_src, er0
	mov	#long_dest, er1
	mov.l	er2, @er1
	set_ccr_zero
	subx.l	@er0, @er1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 long_src er0	; er0 still contains src subress
	test_h_gr32 long_dest er1	; er1 still contains dst subress

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the sub to memory.
	cmp.l	#0xa5a0a0a0, @long_dest
	beq	.Ll5
	fail
.Ll5:

subx_l_rspostdec_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  subx.l @eRs-,rd8	; Subx to register post-decrement
	mov	#long_src, er0
	mov	#long_dest, er1
	mov.l	er2, @er1
	set_ccr_zero
	subx.l	@er0-, @er1-

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 long_src-4 er0	; er0 contains src subress minus one
	test_h_gr32 long_dest-4 er1	; er1 contains dst subress minus one

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the sub to memory.
	cmp.l	#0xa5a0a0a0, @long_dest
	beq	.Ll6
	fail
.Ll6:
.endif
	pass

	exit 0
