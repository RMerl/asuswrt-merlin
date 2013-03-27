# Hitachi H8 testcase 'xor.b'
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
	# xor.b #xx:8, rd	;                     d rd   xxxxxxxx
	# xor.b #xx:8, @erd	;         7 d rd ???? d ???? xxxxxxxx
	# xor.b #xx:8, @erd+	; 0 1 7 4 6 c rd 1??? d ???? xxxxxxxx
	# xor.b #xx:8, @erd-	; 0 1 7 6 6 c rd 1??? d ???? xxxxxxxx
	# xor.b #xx:8, @+erd	; 0 1 7 5 6 c rd 1??? d ???? xxxxxxxx
	# xor.b #xx:8, @-erd	; 0 1 7 7 6 c rd 1??? d ???? xxxxxxxx
	# xor.b rs, rd		;                     1 5 rs rd
	# xor.b reg8, @erd	;         7 d rd ???? 1 5 rs ????
	# xor.b reg8, @erd+	;         0 1 7     9 8 rd 5 rs
	# xor.b reg8, @erd-	;         0 1 7     9 a rd 5 rs
	# xor.b reg8, @+erd	;         0 1 7     9 9 rd 5 rs
	# xor.b reg8, @-erd	;         0 1 7     9 b rd 5 rs
	#
	# xorc #xx:8, ccr	; 
	# xorc #xx:8, exr	; 

	# Coming soon:
	# ...

.data
pre_byte:	.byte 0
byte_dest:	.byte 0xa5
post_byte:	.byte 0

	start
	
xor_b_imm8_reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  xor.b #xx:8,Rd
	xor.b	#0xff, r0l	; Immediate 8-bit operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa55a r0	; xor result:	a5 ^ ff
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a55a er0	; xor result:	 a5 ^ ff
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8sx)
xor_b_imm8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  xor.b #xx:8,@eRd
	mov	#byte_dest, er0
	xor.b	#0xff:8, @er0	; Immediate 8-bit src, reg indirect dst
;;; 	.word	0x7d00
;;; 	.word	0xd0ff

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_h_gr32 byte_dest, er0	; er0 still contains address
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the xor to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x5a, r0l
	beq	.L1
	fail
.L1:

xor_b_imm8_postinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  xor.b #xx:8,@eRd+
	mov	#byte_dest, er0
	xor.b	#0xff:8, @er0+	; Immediate 8-bit src, reg indirect dst
;;; 	.word	0x0174
;;; 	.word	0x6c08
;;; 	.word	0xd0ff

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set
	
	test_h_gr32 post_byte, er0	; er0 contains address plus one
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the xor to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0xa5, r0l
	beq	.L2
	fail
.L2:

xor_b_imm8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  xor.b #xx:8,@eRd-
	mov	#byte_dest, er0
	xor.b	#0xff:8, @er0-	; Immediate 8-bit src, reg indirect dst
;;;  	.word	0x0176
;;;  	.word	0x6c08
;;;  	.word	0xd0ff

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_h_gr32 pre_byte, er0	; er0 contains address minus one
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the xor to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x5a, r0l
	beq	.L3
	fail
.L3:


.endif

xor_b_reg8_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  xor.b Rs,Rd
	mov.b	#0xff, r0h
	xor.b	r0h, r0l	; Register operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xff5a r0	; xor result:	a5 ^ ff
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5ff5a er0	; xor result:	a5 ^ ff
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8sx)
xor_b_reg8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  xor.b rs8,@eRd	; xor reg8 to register indirect
	mov	#byte_dest, er0
	mov	#0xff, r1l
	xor.b	r1l, @er0	; reg8 src, reg indirect dest
;;; 	.word	0x7d00
;;; 	.word	0x1590

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_dest er0	; er0 still contains address
	test_h_gr32 0xa5a5a5ff er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the or to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0xa5, r0l
	beq	.L4
	fail
.L4:

xor_b_reg8_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  xor.b rs8,@eRd+	; xor reg8 to register post-increment
	mov	#byte_dest, er0
	mov	#0xff, r1l
	xor.b	r1l, @er0+	; reg8 src, reg post-increment dest
;;; 	.word	0x0179
;;; 	.word	0x8059

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 post_byte er0	; er0 contains address plus one
	test_h_gr32 0xa5a5a5ff er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the or to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x5a, r0l
	beq	.L5
	fail
.L5:

xor_b_reg8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  xor.b rs8,@eRd-	; xor reg8 to register post-decrement
	mov	#byte_dest, er0
	mov	#0xff, r1l
	xor.b	r1l, @er0-	; reg8 src, reg indirect dest
;;; 	.word	0x0179
;;; 	.word	0xa059

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 pre_byte er0	; er0 contains address minus one
	test_h_gr32 0xa5a5a5ff er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the or to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0xa5, r0l
	beq	.L6
	fail
.L6:

.endif				; h8sx

xorc_imm8_ccr:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  xorc #xx:8,ccr

	test_neg_clear
	xorc	#0x8, ccr	; Immediate 8-bit operand (neg flag)
	test_neg_set
	xorc	#0x8, ccr
	test_neg_clear

	test_zero_clear
	xorc	#0x4, ccr	; Immediate 8-bit operand (zero flag)
	test_zero_set
	xorc	#0x4, ccr
	test_zero_clear

	test_ovf_clear
	xorc	#0x2, ccr	; Immediate 8-bit operand (overflow flag)
	test_ovf_set
	xorc	#0x2, ccr
	test_ovf_clear

	test_carry_clear
	xorc	#0x1, ccr	; Immediate 8-bit operand (carry flag)
	test_carry_set
	xorc	#0x1, ccr
	test_carry_clear

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8300s || sim_cpu == h8sx)	; Earlier versions, no exr
xorc_imm8_exr:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	ldc	#0, exr
	stc	exr, r0l
	test_h_gr8 0, r0l

	set_ccr_zero
	;;  xorc #xx:8,exr

	xorc	#0x80, exr
	test_cc_clear
	stc	exr, r0l
	test_h_gr8 0x80, r0l
	xorc	#0x80, exr
	stc	exr, r0l
	test_h_gr8 0, r0l

	xorc	#0x4, exr
	stc	exr, r0l
	test_h_gr8 4, r0l
	xorc	#0x4, exr
	stc	exr, r0l
	test_h_gr8 0, r0l

	xorc	#0x2, exr	; Immediate 8-bit operand (overflow flag)
	stc	exr, r0l
	test_h_gr8 2, r0l
	xorc	#0x2, exr
	stc	exr, r0l
	test_h_gr8 0, r0l

	xorc	#0x1, exr	; Immediate 8-bit operand (carry flag)
	stc	exr, r0l
	test_h_gr8 1, r0l
	xorc	#0x1, exr
	stc	exr, r0l
	test_h_gr8 0, r0l

	test_h_gr32  0xa5a5a500 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif				; not h8300 or h8300h
	
	pass

	exit 0
