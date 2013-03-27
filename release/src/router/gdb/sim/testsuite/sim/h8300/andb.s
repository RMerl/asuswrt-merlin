# Hitachi H8 testcase 'and.b'
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
	# and.b #xx:8, rd	;                     e rd   xxxxxxxx
	# and.b #xx:8, @erd	;         7 d rd ???? e ???? xxxxxxxx
	# and.b #xx:8, @erd+	; 0 1 7 4 6 c rd 1??? e ???? xxxxxxxx
	# and.b #xx:8, @erd-	; 0 1 7 6 6 c rd 1??? e ???? xxxxxxxx
	# and.b #xx:8, @+erd	; 0 1 7 5 6 c rd 1??? e ???? xxxxxxxx
	# and.b #xx:8, @-erd	; 0 1 7 7 6 c rd 1??? e ???? xxxxxxxx
	# and.b rs, rd		;                     1 6 rs rd
	# and.b reg8, @erd	;         7 d rd ???? 1 6 rs ????
	# and.b reg8, @erd+	;         0 1 7     9 8 rd 6 rs
	# and.b reg8, @erd-	;         0 1 7     9 a rd 6 rs
	# and.b reg8, @+erd	;         0 1 7     9 9 rd 6 rs
	# and.b reg8, @-erd	;         0 1 7     9 b rd 6 rs
	#
	# andc #xx:8, ccr	;         0 6 xxxxxxxx
	# andc #xx:8, exr	; 0 1 4 1 0 6 xxxxxxxx

	# Coming soon:
	# ...

.data
pre_byte:	.byte 0
byte_dest:	.byte 0xa5
post_byte:	.byte 0

	start
	
and_b_imm8_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  and.b #xx:8,Rd
	and.b	#0xaa, r0l	; Immediate 8-bit operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa5a0 r0	; and result:	a5 & aa
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a5a0 er0	; and result:	 a5 & aa
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8sx)
and_b_imm8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  and.b #xx:8,@eRd
	mov	#byte_dest, er0
	and.b	#0xaa:8, @er0	; Immediate 8-bit src, reg indirect dst
;;; 	.word	0x7d00
;;; 	.word	0xe0aa

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set
	
	test_h_gr32 byte_dest, er0	; er0 still contains address
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the and to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0xa0, r0l
	beq	.L1
	fail
.L1:

and_b_imm8_rdpostinc:
	mov	#byte_dest, er0
	mov.b	#0xa5, r1l
	mov.b	r1l, @er0

	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  and.b #xx:8,@eRd+
	mov	#byte_dest, er0
	and.b	#0x55:8, @er0+	; Immediate 8-bit src, reg post-incr dest
;;; 	.word	0x0174
;;; 	.word	0x6c08
;;; 	.word	0xe055

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_h_gr32 post_byte, er0	; er0 contains address plus one
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the and to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x05, r0l
	beq	.L2
	fail
.L2:

and_b_imm8_rdpostdec:
	mov	#byte_dest, er0
	mov.b	#0xa5, r1l
	mov.b	r1l, @er0

	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  and.b #xx:8,@eRd-
	mov	#byte_dest, er0
	and.b	#0xaa:8, @er0-	; Immediate 8-bit src, reg post-decr dest
;;; 	.word	0x0176
;;; 	.word	0x6c08
;;; 	.word	0xe0aa

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set
	
	test_h_gr32 pre_byte, er0	; er0 contains address minus one
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the and to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0xa0, r0l
	beq	.L3
	fail
.L3:

and_b_imm8_rdpreinc:
	mov	#byte_dest, er0
	mov.b	#0xa5, r1l
	mov.b	r1l, @er0

	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  and.b #xx:8,@+eRd
	mov	#pre_byte, er0
	and.b	#0x55:8, @+er0	; Immediate 8-bit src, reg pre-incr dest
;;; 	.word	0x0175
;;; 	.word	0x6c08
;;; 	.word	0xe055

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_h_gr32 byte_dest, er0	; er0 contains destination address 
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the and to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x05, r0l
	beq	.L4
	fail
.L4:

and_b_imm8_rdpredec:
	mov	#byte_dest, er0
	mov.b	#0xa5, r1l
	mov.b	r1l, @er0

	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  and.b #xx:8,@-eRd
	mov	#post_byte, er0
	and.b	#0xaa:8, @-er0	; Immediate 8-bit src, reg pre-decr dest
;;; 	.word	0x0177
;;; 	.word	0x6c08
;;; 	.word	0xe0aa

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set
	
	test_h_gr32 byte_dest, er0	; er0 contains destination address 
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the and to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0xa0, r0l
	beq	.L5
	fail
.L5:

.endif				; h8sx

and_b_reg8_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  and.b Rs,Rd
	mov.b	#0xaa, r0h
	and.b	r0h, r0l	; Register operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xaaa0 r0	; and result:	a5 & aa
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5aaa0 er0	; and result:	a5 & aa
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8sx)
and_b_reg8_rdind:
	mov	#byte_dest, er0
	mov.b	#0xa5, r1l
	mov.b	r1l, @er0

	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  and.b rs8,@eRd	; And to register indirect
	mov	#byte_dest, er0
	mov	#0x55, r1l
	and.b	r1l, @er0	; reg8 src, reg indirect dest
;;; 	.word	0x7d00
;;; 	.word	0x1690

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dest er0	; er0 still contains address
	test_h_gr32 0xa5a5a555 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the and to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x05, r0l
	beq	.L6
	fail
.L6:

and_b_reg8_rdpostinc:
	mov	#byte_dest, er0
	mov.b	#0xa5, r1l
	mov.b	r1l, @er0

	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  and.b rs8,@eRd+	; And to register post-incr
	mov	#byte_dest, er0
	mov	#0xaa, r1l
	and.b	r1l, @er0+	; reg8 src, reg post-incr dest
;;; 	.word	0x0179
;;; 	.word	0x8069

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 post_byte er0	; er0 contains address plus one
	test_h_gr32 0xa5a5a5aa er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the and to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0xa0, r0l
	beq	.L7
	fail
.L7:

and_b_reg8_rdpostdec:
	mov	#byte_dest, er0
	mov.b	#0xa5, r1l
	mov.b	r1l, @er0

	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  and.b rs8,@eRd-	; And to register post-decr
	mov	#byte_dest, er0
	mov	#0x55, r1l
	and.b	r1l, @er0-	; reg8 src, reg post-decr dest
;;; 	.word	0x0179
;;; 	.word	0xa069

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 pre_byte er0	; er0 contains address minus one
	test_h_gr32 0xa5a5a555 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the and to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x05, r0l
	beq	.L8
	fail
.L8:

and_b_reg8_rdpreinc:
	mov	#byte_dest, er0
	mov.b	#0xa5, r1l
	mov.b	r1l, @er0

	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  and.b rs8,@+eRd	; And to register post-incr
	mov	#pre_byte, er0
	mov	#0xaa, r1l
	and.b	r1l, @+er0	; reg8 src, reg post-incr dest
;;; 	.word	0x0179
;;; 	.word	0x9069

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_dest er0	; er0 contains destination address 
	test_h_gr32 0xa5a5a5aa er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the and to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0xa0, r0l
	beq	.L9
	fail
.L9:

and_b_reg8_rdpredec:
	mov	#byte_dest, er0
	mov.b	#0xa5, r1l
	mov.b	r1l, @er0

	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  and.b rs8,@-eRd	; And to register post-decr
	mov	#post_byte, er0
	mov	#0x55, r1l
	and.b	r1l, @-er0	; reg8 src, reg post-decr dest
;;; 	.word	0x0179
;;; 	.word	0xb069

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dest er0	; er0 contains destination address 
	test_h_gr32 0xa5a5a555 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the and to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x05, r0l
	beq	.L10
	fail
.L10:
.endif				; h8sx

andc_imm8_ccr:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  andc #xx:8,ccr
	set_ccr 0xff

	test_neg_set
	andc	#0xf7, ccr	; Immediate 8-bit operand (neg flag)
	test_neg_clear

	test_zero_set
	andc	#0xfb, ccr	; Immediate 8-bit operand (zero flag)
	test_zero_clear

	test_ovf_set
	andc	#0xfd, ccr	; Immediate 8-bit operand (overflow flag)
	test_ovf_clear

	test_carry_set
	andc	#0xfe, ccr	; Immediate 8-bit operand (carry flag)
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
andc_imm8_exr:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	ldc	#0xff, exr
	stc	exr, r0l
	test_h_gr8 0x87, r0l

	;;  andc #xx:8,exr
	set_ccr_zero
	andc	#0x7f, exr
	test_cc_clear
	stc	exr, r0l
	test_h_gr8 0x7, r0l

	andc	#0x3, exr
	stc	exr, r0l
	test_h_gr8 0x3, r0l

	andc	#0x1, exr
	stc	exr, r0l
	test_h_gr8 0x1, r0l

	andc	#0x0, exr
	stc	exr, r0l
	test_h_gr8 0x0, r0l

	test_h_gr32  0xa5a5a500 er0
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif				; not h8300 or h8300h
	
	pass

	exit 0
