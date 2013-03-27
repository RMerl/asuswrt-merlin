# Hitachi H8 testcase 'addx'
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
	# addx.b #xx:8, rd8	; 9 rd8 xxxxxxxx
	# addx.b #xx:8, @erd	; 7 d erd ???? 9 ???? xxxxxxxx 
	# addx.b #xx:8, @erd-	; 0 1 7 6 6 c erd 1??? 9 ???? xxxxxxxx
	# addx.b rs8, rd8	; 0 e rs8 rd8
	# addx.b rs8, @erd	; 7 d erd ???? 0 e rs8 ????
	# addx.b rs8, @erd-	; 0 1 7 6 6 c erd 1??? 0 e rs8 ????
	# addx.b @ers, rd8	; 7 c ers ???? 0 e ???? rd8
	# addx.b @ers-, rd8	; 0 1 7 6 6 c ers 00?? 0 e ???? rd8
	# addx.b @ers, @erd	; 0 1 7 4 6 8 ers d 0 erd 1 ???? 
	# addx.b @ers-, @erd-	; 0 1 7 6 6 c ers d a erd 1 ????
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

addx_b_imm8_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b #xx:8,Rd	; Addx with carry initially zero.
	addx.b	#5, r0l		; Immediate 8-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0xa5aa r0	; add result:	a5 + 5
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a5aa er0	; add result:	 a5 + 5
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
addx_b_imm8_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b #xx:8,Rd	; Addx with carry initially one.
	set_carry_flag
	addx.b	#5, r0l		; Immediate 8-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0xa5ab r0	; add result:	a5 + 5 + 1
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a5ab er0	; add result:	 a5 + 5 + 1
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8sx)
addx_b_imm8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b #xx:8,@eRd	; Addx to register indirect
	mov	#byte_dest, er0
	addx.b	#5, @er0

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dest er0	; er0 still contains address

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.b	#5, @byte_dest
	beq	.Lb1
	fail
.Lb1:

addx_b_imm8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b #xx:8,@eRd-	; Addx to register post-decrement
	mov	#byte_dest, er0
	addx.b	#5, @er0-

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dest-1 er0	; er0 contains address minus one

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.b	#10, @byte_dest
	beq	.Lb2
	fail
.Lb2:
.endif

addx_b_reg8_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b Rs,Rd	; addx with carry initially zero
	mov.b	#5, r0h
	addx.b	r0h, r0l	; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0x05aa r0	; add result:	a5 + 5
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a505aa er0	; add result:	a5 + 5
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

addx_b_reg8_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b Rs,Rd	; addx with carry initially one
	mov.b	#5, r0h
	set_carry_flag
	addx.b	r0h, r0l	; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0x05ab r0	; add result:	a5 + 5 + 1
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a505ab er0	; add result:	a5 + 5 + 1
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8sx)
addx_b_reg8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b rs8,@eRd	; Addx to register indirect
	mov	#byte_dest, er0
	mov.b	#5, r1l
	addx.b	r1l, @er0

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dest er0	; er0 still contains address
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.b	#15, @byte_dest
	beq	.Lb3
	fail
.Lb3:

addx_b_reg8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b rs8,@eRd-	; Addx to register post-decrement
	mov	#byte_dest, er0
	mov.b	#5, r1l
	addx.b	r1l, @er0-

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dest-1 er0	; er0 contains address minus one
	test_h_gr32 0xa5a5a505 er1	; er1 contains the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.b	#20, @byte_dest
	beq	.Lb4
	fail
.Lb4:

addx_b_rsind_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b @eRs,rd8	; Addx from reg indirect to reg
	mov	#byte_src, er0
	addx.b	@er0, r1l

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_src er0	; er0 still contains address
	test_h_gr32 0xa5a5a5aa er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

addx_b_rspostdec_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b @eRs-,rd8	; Addx to register post-decrement
	mov	#byte_src, er0
	addx.b	@er0-, r1l

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_src-1 er0	; er0 contains address minus one
	test_h_gr32 0xa5a5a5aa er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

addx_b_rsind_rsind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b @eRs,rd8	; Addx from reg indirect to reg
	mov	#byte_src, er0
	mov	#byte_dest, er1
	addx.b	@er0, @er1

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_src er0	; er0 still contains src address
	test_h_gr32 byte_dest er1	; er1 still contains dst address

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the add to memory.
	cmp.b	#25, @byte_dest
	beq	.Lb5
	fail
.Lb5:

addx_b_rspostdec_rspostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.b @eRs-,rd8	; Addx to register post-decrement
	mov	#byte_src, er0
	mov	#byte_dest, er1
	addx.b	@er0-, @er1-

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_src-1 er0	; er0 contains src address minus one
	test_h_gr32 byte_dest-1 er1	; er1 contains dst address minus one

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the add to memory.
	cmp.b	#30, @byte_dest
	beq	.Lb6
	fail
.Lb6:

addx_w_imm16_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w #xx:16,Rd	; Addx with carry initially zero.
	addx.w	#0x505, r0	; Immediate 16-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0xaaaa r0	; add result:	0xa5a5 + 0x505
	test_h_gr32 0xa5a5aaaa er0	; add result:	 0xa5a5 + 0x505
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
addx_w_imm16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w #xx:16,Rd	; Addx with carry initially one.
	set_carry_flag
	addx.w	#0x505, r0	; Immediate 16-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr16 0xaaab r0	; add result:	0xa5a5 + 0x505 + 1
	test_h_gr32 0xa5a5aaab er0	; add result:	 0xa5a5 + 0x505 + 1
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
addx_w_imm16_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w #xx:16,@eRd	; Addx to register indirect
	mov	#word_dest, er0
	addx.w	#0x505, @er0

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 word_dest er0	; er0 still contains address

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.w	#0x505, @word_dest
	beq	.Lw1
	fail
.Lw1:

addx_w_imm16_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w #xx:16,@eRd-	; Addx to register post-decrement
	mov	#word_dest, er0
	addx.w	#0x505, @er0-

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 word_dest-2 er0	; er0 contains address minus one

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.w	#0xa0a, @word_dest
	beq	.Lw2
	fail
.Lw2:

addx_w_reg16_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w Rs,Rd	; addx with carry initially zero
	mov.w	#0x505, e0
	addx.w	e0, r0		; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0x0505aaaa er0	; add result:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

addx_w_reg16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w Rs,Rd	; addx with carry initially one
	mov.w	#0x505, e0
	set_carry_flag
	addx.w	e0, r0		; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0x0505aaab er0	; add result:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
addx_w_reg16_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w rs8,@eRd	; Addx to register indirect
	mov	#word_dest, er0
	mov.w	#0x505, r1
	addx.w	r1, @er0

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 word_dest er0	; er0 still contains address
	test_h_gr32 0xa5a50505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.w	#0xf0f, @word_dest
	beq	.Lw3
	fail
.Lw3:

addx_w_reg16_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w rs8,@eRd-	; Addx to register post-decrement
	mov	#word_dest, er0
	mov.w	#0x505, r1
	addx.w	r1, @er0-

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 word_dest-2 er0	; er0 contains address minus one
	test_h_gr32 0xa5a50505  er1	; er1 contains the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.w	#0x1414, @word_dest
	beq	.Lw4
	fail
.Lw4:

addx_w_rsind_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w @eRs,rd8	; Addx from reg indirect to reg
	mov	#word_src, er0
	addx.w	@er0, r1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 word_src er0	; er0 still contains address
	test_h_gr32 0xa5a5aaaa er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

addx_w_rspostdec_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w @eRs-,rd8	; Addx to register post-decrement
	mov	#word_src, er0
	addx.w	@er0-, r1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 word_src-2 er0	; er0 contains address minus one
	test_h_gr32 0xa5a5aaaa er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

addx_w_rsind_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w @eRs,rd8	; Addx from reg indirect to reg
	mov	#word_src, er0
	mov	#word_dest, er1
	addx.w	@er0, @er1

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 word_src er0	; er0 still contains src address
	test_h_gr32 word_dest er1	; er1 still contains dst address

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the add to memory.
	cmp.w	#0x1919, @word_dest
	beq	.Lw5
	fail
.Lw5:

addx_w_rspostdec_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.w @eRs-,rd8	; Addx to register post-decrement
	mov	#word_src, er0
	mov	#word_dest, er1
	addx.w	@er0-, @er1-

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 word_src-2 er0	; er0 contains src address minus one
	test_h_gr32 word_dest-2 er1	; er1 contains dst address minus one

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the add to memory.
	cmp.w	#0x1e1e, @word_dest
	beq	.Lw6
	fail
.Lw6:

addx_l_imm32_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l #xx:32,Rd	; Addx with carry initially zero.
	addx.l	#0x50505, er0	; Immediate 32-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0xa5aaaaaa er0	; add result:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
addx_l_imm32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l #xx:32,Rd	; Addx with carry initially one.
	set_carry_flag
	addx.l	#0x50505, er0	; Immediate 32-bit operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0xa5aaaaab er0	; add result:
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
addx_l_imm32_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l #xx:32,@eRd	; Addx to register indirect
	mov	#long_dest, er0
	addx.l	#0x50505, @er0

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 long_dest er0	; er0 still contains address

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.l	#0x50505, @long_dest
	beq	.Ll1
	fail
.Ll1:

addx_l_imm32_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l #xx:32,@eRd-	; Addx to register post-decrement
	mov	#long_dest, er0
	addx.l	#0x50505, @er0-

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 long_dest-4 er0	; er0 contains address minus one

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.l	#0xa0a0a, @long_dest
	beq	.Ll2
	fail
.Ll2:

addx_l_reg32_0:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l Rs,Rd	; addx with carry initially zero
	mov.l	#0x50505, er0
	addx.l	er0, er1	; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0x50505    er0	; add load
	test_h_gr32 0xa5aaaaaa er1	; add result:
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

addx_l_reg32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l Rs,Rd	; addx with carry initially one
	mov.l	#0x50505, er0
	set_carry_flag
	addx.l	er0, er1	; Register operand

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 0x50505    er0	; add result:
	test_h_gr32 0xa5aaaaab er1	; add result:
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
addx_l_reg32_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l rs8,@eRd	; Addx to register indirect
	mov	#long_dest, er0
	mov.l	#0x50505, er1
	addx.l	er1, @er0

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 long_dest er0	; er0 still contains address
	test_h_gr32 0x50505   er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.l	#0xf0f0f, @long_dest
	beq	.Ll3
	fail
.Ll3:

addx_l_reg32_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l rs8,@eRd-	; Addx to register post-decrement
	mov	#long_dest, er0
	mov.l	#0x50505, er1
	addx.l	er1, @er0-

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 long_dest-4 er0	; er0 contains address minus one
	test_h_gr32 0x50505     er1	; er1 contains the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	cmp.l	#0x141414, @long_dest
	beq	.Ll4
	fail
.Ll4:

addx_l_rsind_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l @eRs,rd8	; Addx from reg indirect to reg
	mov	#long_src, er0
	addx.l	@er0, er1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 long_src er0	; er0 still contains address
	test_h_gr32 0xa5aaaaaa er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

addx_l_rspostdec_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l @eRs-,rd8	; Addx to register post-decrement
	mov	#long_src, er0
	addx.l	@er0-, er1

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 long_src-4 er0	; er0 contains address minus one
	test_h_gr32 0xa5aaaaaa er1	; er1 contains the sum

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

addx_l_rsind_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l @eRs,rd8	; Addx from reg indirect to reg
	mov	#long_src, er0
	mov	#long_dest, er1
	addx.l	@er0, @er1

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 long_src er0	; er0 still contains src address
	test_h_gr32 long_dest er1	; er1 still contains dst address

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the add to memory.
	cmp.l	#0x191919, @long_dest
	beq	.Ll5
	fail
.Ll5:

addx_l_rspostdec_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  addx.l @eRs-,rd8	; Addx to register post-decrement
	mov	#long_src, er0
	mov	#long_dest, er1
	addx.l	@er0-, @er1-

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 long_src-4 er0	; er0 contains src address minus one
	test_h_gr32 long_dest-4 er1	; er1 contains dst address minus one

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	;; Now check the result of the add to memory.
	cmp.l	#0x1e1e1e, @long_dest
	beq	.Ll6
	fail
.Ll6:
.endif
	pass

	exit 0
