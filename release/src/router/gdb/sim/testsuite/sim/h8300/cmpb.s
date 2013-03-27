# Hitachi H8 testcase 'cmp.b'
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
	# cmp.b #xx:8, rd	;                     a rd   xxxxxxxx
	# cmp.b #xx:8, @erd	;         7 d rd ???? a ???? xxxxxxxx
	# cmp.b #xx:8, @erd+	; 0 1 7 4 6 c rd 1??? a ???? xxxxxxxx
	# cmp.b #xx:8, @erd-	; 0 1 7 6 6 c rd 1??? a ???? xxxxxxxx
	# cmp.b #xx:8, @+erd	; 0 1 7 5 6 c rd 1??? a ???? xxxxxxxx
	# cmp.b #xx:8, @-erd	; 0 1 7 7 6 c rd 1??? a ???? xxxxxxxx
	# cmp.b rs, rd		;                     1 c rs rd
	# cmp.b reg8, @erd	;         7 d rd ???? 1 c rs ????
	# cmp.b reg8, @erd+	;         0 1 7     9 8 rd 2 rs
	# cmp.b reg8, @erd-	;         0 1 7     9 a rd 2 rs
	# cmp.b reg8, @+erd	;         0 1 7     9 9 rd 2 rs
	# cmp.b reg8, @-erd	;         0 1 7     9 b rd 2 rs
	# cmp.b rsind, rdind	     ; 7 c 0rs 5 0 ?rd 2 ????
	# cmp.b rspostinc, rdpostinc ; 0 1 7 4 6 c 0rs c 8 ?rd 2 ????
	# cmp.b rspostdec, rdpostdec ; 0 1 7 6 6 c 0rs c a ?rd 2 ????
	# cmp.b rspreinc, rdpreinc   ; 0 1 7 5 6 c 0rs c 9 ?rd 2 ????
	# cmp.b rspredec, rdpredec   ; 0 1 7 7 6 c 0rs c b ?rd 2 ????
	# cmp.b disp2, disp2	     ; 0 1 7 01dd:2 6 8 0rs c 00dd:2 ?rd 2 ????
	# cmp.b disp16, disp16	     ; 0 1 7 4 6 e 0rs c dd:16 c 0rd 2 ???? dd:16
	# cmp.b disp32, disp32	     ; 7 8 0rs 4 6 a 2 c dd:32 c 1rd 2 ???? dd:32
	# cmp.b indexb16, indexb16   ; 0 1 7 5 6 e 0rs c dd:16 d 0rd 2 ???? dd:16
	# cmp.b indexw16, indexw16   ; 0 1 7 6 6 e 0rs c dd:16 e 0rd 2 ???? dd:16
	# cmp.b indexl16, indexl16   ; 0 1 7 7 6 e 0rs c dd:16 f 0rd 2 ???? dd:16
	# cmp.b indexb32, indexb32   ; 7 8 0rs 5 6 a 2 c dd:32 d 1rd 2 ???? dd:32
	# cmp.b indexw32, indexw32   ; 7 8 0rs 6 6 a 2 c dd:32 e 1rd 2 ???? dd:32
	# cmp.b indexl32, indexl32   ; 7 8 0rs 7 6 a 2 c dd:32 f 1rd 2 ???? dd:32
	# cmp.b abs16, abs16	     ; 6 a 1 5 aa:16 4 0??? 2 ???? aa:16
	# cmp.b abs32, abs32	     ; 6 a 3 5 aa:32 4 1??? 2 ???? aa:32
	#

	# Coming soon:
	
	# ...

.data
byte_src:	.byte 0x5a
pre_byte:	.byte 0
byte_dst:	.byte 0xa5
post_byte:	.byte 0

	start
	
cmp_b_imm8_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  cmp.b #xx:8,Rd
	cmp.b	#0xa5, r0l	; Immediate 8-bit src, reg8 dest
	beq	.Leq1
	fail
.Leq1:	cmp.b	#0xa6, r0l
	blt	.Llt1
	fail
.Llt1:	cmp.b	#0xa4, r0l
	bgt	.Lgt1
	fail
.Lgt1:	
	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa5a5 r0	; r0 unchanged
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a5a5 er0	; er0 unchanged
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8sx)
cmp_b_imm8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  cmp.b #xx:8,@eRd
	mov	#byte_dst, er0
	cmp.b	#0xa5:8, @er0	; Immediate 8-bit src, reg indirect dst
;;; 	.word	0x7d00
;;; 	.word	0xa0a5
	beq	.Leq2
	fail
.Leq2:	set_ccr_zero
	cmp.b	#0xa6, @er0
;;; 	.word	0x7d00
;;; 	.word	0xa0a6
	blt	.Llt2
	fail
.Llt2:	set_ccr_zero
	cmp.b	#0xa4, @er0
;;; 	.word	0x7d00
;;; 	.word	0xa0a4
	bgt	.Lgt2
	fail
.Lgt2:		
	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_h_gr32 byte_dst er0	; er0 still contains address
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the cmp to memory (memory unchanged).
	sub.b	r0l, r0l
	mov.b	@byte_dst, r0l
	cmp.b	#0xa5, r0l
	beq	.L2
	fail
.L2:

cmp_b_imm8_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  cmp.b #xx:8,@eRd+
	mov	#byte_dst, er0
	cmp.b	#0xa5:8, @er0+	; Immediate 8-bit src, reg postinc dst
;;; 	.word	0x0174
;;; 	.word	0x6c08
;;; 	.word	0xa0a5
	beq	.Leq3
	fail
.Leq3:	test_h_gr32 post_byte er0	; er0 contains address plus one
	mov	#byte_dst, er0
	set_ccr_zero
	cmp.b	#0xa6, @er0+
;;; 	.word	0x0174
;;; 	.word	0x6c08
;;; 	.word	0xa0a6
	blt	.Llt3
	fail
.Llt3:	test_h_gr32 post_byte er0	; er0 contains address plus one
	mov	#byte_dst, er0
	set_ccr_zero
	cmp.b	#0xa4, @er0+
;;; 	.word	0x0174
;;; 	.word	0x6c08
;;; 	.word	0xa0a4
	bgt	.Lgt3
	fail
.Lgt3:
	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 post_byte er0	; er0 contains address plus one
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the cmp to memory (memory unchanged).
	sub.b	r0l, r0l
	mov.b	@byte_dst, r0l
	cmp.b	#0xa5, r0l
	beq	.L3
	fail
.L3:

cmp_b_imm8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  cmp.b #xx:8,@eRd-
	mov	#byte_dst, er0
	cmp.b	#0xa5:8, @er0-	; Immediate 8-bit src, reg postdec dst
;;; 	.word	0x0176
;;; 	.word	0x6c08
;;; 	.word	0xa0a5
	beq	.Leq4
	fail
.Leq4:	test_h_gr32 pre_byte er0	; er0 contains address minus one
	mov	#byte_dst, er0
	set_ccr_zero
	cmp.b	#0xa6, @er0-
;;; 	.word	0x0176
;;; 	.word	0x6c08
;;; 	.word	0xa0a6
	blt	.Llt4
	fail
.Llt4:	test_h_gr32 pre_byte er0	; er0 contains address minus one
	mov	#byte_dst, er0
	set_ccr_zero
	cmp.b	#0xa4, @er0-
;;; 	.word	0x0176
;;; 	.word	0x6c08
;;; 	.word	0xa0a4
	bgt	.Lgt4
	fail
.Lgt4:
	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 pre_byte er0	; er0 contains address minus one
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the cmp to memory (memory unchanged).
	sub.b	r0l, r0l
	mov.b	@byte_dst, r0l
	cmp.b	#0xa5, r0l
	beq	.L4
	fail
.L4:

cmp_b_imm8_rdpreinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  cmp.b #xx:8,@+eRd
	mov	#pre_byte, er0
	cmp.b	#0xa5:8, @+er0	; Immediate 8-bit src, reg pre-inc dst
;;; 	.word	0x0175
;;; 	.word	0x6c08
;;; 	.word	0xa0a5
	beq	.Leq5
	fail
.Leq5:	test_h_gr32 byte_dst er0	; er0 contains destination address 
	mov	#pre_byte, er0
	set_ccr_zero
	cmp.b	#0xa6, @+er0
;;; 	.word	0x0175
;;; 	.word	0x6c08
;;; 	.word	0xa0a6
	blt	.Llt5
	fail
.Llt5:	test_h_gr32 byte_dst er0	; er0 contains destination address 
	mov	#pre_byte, er0
	set_ccr_zero
	cmp.b	#0xa4, @+er0
;;; 	.word	0x0175
;;; 	.word	0x6c08
;;; 	.word	0xa0a4
	bgt	.Lgt5
	fail
.Lgt5:
	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dst er0	; er0 contains destination address 
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the cmp to memory (memory unchanged).
	sub.b	r0l, r0l
	mov.b	@byte_dst, r0l
	cmp.b	#0xa5, r0l
	beq	.L5
	fail
.L5:

cmp_b_imm8_rdpredec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  cmp.b #xx:8,@-eRd
	mov	#post_byte, er0
	cmp.b	#0xa5:8, @-er0	; Immediate 8-bit src, reg pre-dec dst
;;; 	.word	0x0177
;;; 	.word	0x6c08
;;; 	.word	0xa0a5
	beq	.Leq6
	fail
.Leq6:	test_h_gr32 byte_dst er0	; er0 contains destination address 
	mov	#post_byte, er0
	set_ccr_zero
	cmp.b	#0xa6, @-er0
;;; 	.word	0x0177
;;; 	.word	0x6c08
;;; 	.word	0xa0a6
	blt	.Llt6
	fail
.Llt6:	test_h_gr32 byte_dst er0	; er0 contains destination address 
	mov	#post_byte, er0
	set_ccr_zero
	cmp.b	#0xa4, @-er0
;;; 	.word	0x0177
;;; 	.word	0x6c08
;;; 	.word	0xa0a4
	bgt	.Lgt6
	fail
.Lgt6:
	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dst er0	; er0 contains destination address 
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the cmp to memory (memory unchanged).
	sub.b	r0l, r0l
	mov.b	@byte_dst, r0l
	cmp.b	#0xa5, r0l
	beq	.L6
	fail
.L6:


.endif

cmp_b_reg8_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  cmp.b Rs,Rd
	mov.b	#0xa5, r0h
	cmp.b	r0h, r0l	; Reg8 src, reg8 dst
	beq	.Leq7
	fail
.Leq7:	mov.b	#0xa6, r0h
	cmp.b	r0h, r0l
	blt	.Llt7
	fail
.Llt7:	mov.b	#0xa4, r0h
	cmp.b	r0h, r0l
	bgt	.Lgt7
	fail
.Lgt7:
	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa4a5 r0	; r0l unchanged.
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a4a5 er0	; r0l unchanged
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.if (sim_cpu == h8sx)
cmp_b_reg8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  cmp.b rs8,@eRd	; cmp reg8 to register indirect
	mov	#byte_dst, er0
	mov	#0xa5, r1l
	cmp.b	r1l, @er0	; reg8 src, reg indirect dest
;;; 	.word	0x7d00
;;; 	.word	0x1c90
	beq	.Leq8
	fail
.Leq8:	set_ccr_zero
	mov	#0xa6, r1l
	cmp.b	r1l, @er0
;;; 	.word	0x7d00
;;; 	.word	0x1c90
	blt	.Llt8
	fail
.Llt8:	set_ccr_zero
	mov	#0xa4, r1l
	cmp.b	r1l, @er0
;;; 	.word	0x7d00
;;; 	.word	0x1c90
	bgt	.Lgt8
	fail
.Lgt8:
	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dst er0	; er0 still contains address
	test_h_gr32 0xa5a5a5a4 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the cmp to memory (no change).
	sub.b	r0l, r0l
	mov.b	@byte_dst, r0l
	cmp.b	#0xa5, r0l
	beq	.L8
	fail
.L8:

cmp_b_reg8_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  cmp.b reg8,@eRd+
	mov	#byte_dst, er0
	mov	#0xa5, r1l
	cmp.b	r1l, @er0+	; Immediate 8-bit src, reg post-incr dst
;;; 	.word	0x0179
;;; 	.word	0x8029
	beq	.Leq9
	fail
.Leq9:	test_h_gr32 post_byte er0	; er0 contains address plus one
	mov	#byte_dst er0
	mov	#0xa6, r1l
	set_ccr_zero
	cmp.b	r1l, @er0+
;;; 	.word	0x0179
;;; 	.word	0x8029
	blt	.Llt9
	fail
.Llt9:	test_h_gr32 post_byte er0	; er0 contains address plus one
	mov	#byte_dst er0
	mov	#0xa4, r1l
	set_ccr_zero
	cmp.b	r1l, @er0+
;;; 	.word	0x0179
;;; 	.word	0x8029
	bgt	.Lgt9
	fail
.Lgt9:
	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 post_byte er0	; er0 contains address plus one
	test_h_gr32 0xa5a5a5a4 er1	; er1 contains test load
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the cmp to memory (memory unchanged).
	sub.b	r0l, r0l
	mov.b	@byte_dst, r0l
	cmp.b	#0xa5, r0l
	beq	.L9
	fail
.L9:

cmp_b_reg8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  cmp.b reg8,@eRd-
	mov	#byte_dst, er0
	mov	#0xa5, r1l
	cmp.b	r1l, @er0-	; Immediate 8-bit src, reg postdec dst
;;; 	.word	0x0179
;;; 	.word	0xa029
	beq	.Leq10
	fail
.Leq10:	test_h_gr32 pre_byte er0	; er0 contains address minus one
	mov	#byte_dst er0
	mov	#0xa6, r1l
	set_ccr_zero
	cmp.b	r1l, @er0-
;;; 	.word	0x0179
;;; 	.word	0xa029
	blt	.Llt10
	fail
.Llt10:	test_h_gr32 pre_byte er0	; er0 contains address minus one
	mov	#byte_dst er0
	mov	#0xa4, r1l
	set_ccr_zero
	cmp.b	r1l, @er0-
;;; 	.word	0x0179
;;; 	.word	0xa029
	bgt	.Lgt10
	fail
.Lgt10:
	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 pre_byte er0	; er0 contains address minus one
	test_h_gr32 0xa5a5a5a4 er1	; er1 contains test load
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the cmp to memory (memory unchanged).
	sub.b	r0l, r0l
	mov.b	@byte_dst, r0l
	cmp.b	#0xa5, r0l
	beq	.L10
	fail
.L10:

cmp_b_reg8_rdpreinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  cmp.b reg8,@+eRd
	mov	#pre_byte, er0
	mov	#0xa5, r1l
	cmp.b	r1l, @+er0	; Immediate 8-bit src, reg post-incr dst
;;; 	.word	0x0179
;;; 	.word	0x9029
	beq	.Leq11
	fail
.Leq11:	test_h_gr32 byte_dst er0	; er0 contains destination address 
	mov	#pre_byte er0
	mov	#0xa6, r1l
	set_ccr_zero
	cmp.b	r1l, @+er0
;;; 	.word	0x0179
;;; 	.word	0x9029
	blt	.Llt11
	fail
.Llt11:	test_h_gr32 byte_dst er0	; er0 contains destination address 
	mov	#pre_byte er0
	mov	#0xa4, r1l
	set_ccr_zero
	cmp.b	r1l, @+er0
;;; 	.word	0x0179
;;; 	.word	0x9029
	bgt	.Lgt11
	fail
.Lgt11:
	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dst er0	; er0 contains destination address 
	test_h_gr32 0xa5a5a5a4 er1	; er1 contains test load
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the cmp to memory (memory unchanged).
	sub.b	r0l, r0l
	mov.b	@byte_dst, r0l
	cmp.b	#0xa5, r0l
	beq	.L11
	fail
.L11:

cmp_b_reg8_rdpredec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  cmp.b reg8,@-eRd
	mov	#post_byte, er0
	mov	#0xa5, r1l
	cmp.b	r1l, @-er0	; Immediate 8-bit src, reg postdec dst
;;; 	.word	0x0179
;;; 	.word	0xb029
	beq	.Leq12
	fail
.Leq12:	test_h_gr32 byte_dst er0	; er0 contains destination address 
	mov	#post_byte er0
	mov	#0xa6, r1l
	set_ccr_zero
	cmp.b	r1l, @-er0
;;; 	.word	0x0179
;;; 	.word	0xb029
	blt	.Llt12
	fail
.Llt12:	test_h_gr32 byte_dst er0	; er0 contains destination address 
	mov	#post_byte er0
	mov	#0xa4, r1l
	set_ccr_zero
	cmp.b	r1l, @-er0
;;; 	.word	0x0179
;;; 	.word	0xb029
	bgt	.Lgt12
	fail
.Lgt12:
	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dst er0	; er0 contains destination address 
	test_h_gr32 0xa5a5a5a4 er1	; er1 contains test load
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the cmp to memory (memory unchanged).
	sub.b	r0l, r0l
	mov.b	@byte_dst, r0l
	cmp.b	#0xa5, r0l
	beq	.L12
	fail
.L12:

cmp_b_rsind_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#byte_src, er1
	mov	#byte_dst, er2	
	set_ccr_zero
	cmp.b	@er1, @er2
	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	byte_src er1	
	test_h_gr32	byte_dst er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1
.if 1				; ambiguous
cmp_b_rspostinc_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#byte_src, er1
	mov	#byte_dst, er2	
	set_ccr_zero
	cmp.b	@er1+, @er2+
;;;	.word	0x0174
;;;	.word	0x6c1c
;;;	.word	0x8220

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	byte_src+1 er1	
	test_h_gr32	byte_dst+1 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1
.endif
.if 1				; ambiguous
cmp_b_rspostdec_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#byte_src, er1
	mov	#byte_dst, er2	
	set_ccr_zero
	cmp.b	@er1-, @er2-
;;;	.word	0x0176
;;;	.word	0x6c1c
;;;	.word	0xa220

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	byte_src-1 er1	
	test_h_gr32	byte_dst-1 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1
.endif

cmp_b_rspreinc_rdpreinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#byte_src-1, er1
	mov	#byte_dst-1, er2	
	set_ccr_zero
	cmp.b	@+er1, @+er2
;;;	.word	0x0175
;;;	.word	0x6c1c
;;;	.word	0x9220

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	byte_src er1	
	test_h_gr32	byte_dst er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1

cmp_b_rspredec_predec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#byte_src+1, er1
	mov	#byte_dst+1, er2	
	set_ccr_zero
	cmp.b	@-er1, @-er2
;;;	.word	0x0177
;;;	.word	0x6c1c
;;;	.word	0xb220

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	byte_src er1	
	test_h_gr32	byte_dst er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1

cmp_b_disp2_disp2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#byte_src-1, er1
	mov	#byte_dst-2, er2
	set_ccr_zero
	cmp.b	@(1:2, er1), @(2:2, er2)
;;;	.word	0x0175
;;;	.word	0x681c
;;;	.word	0x2220

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	byte_src-1 er1	
	test_h_gr32	byte_dst-2 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1

cmp_b_disp16_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#byte_src-3, er1
	mov	#byte_dst-4, er2
	set_ccr_zero
	cmp.b	@(3:16, er1), @(4:16, er2)
;;;	.word	0x0174
;;;	.word	0x6e1c
;;;	.word	3
;;;	.word	0xc220
;;;	.word	4

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	byte_src-3 er1	
	test_h_gr32	byte_dst-4 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1

cmp_b_disp32_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#byte_src+5, er1
	mov	#byte_dst+6, er2
	set_ccr_zero
	cmp.b	@(-5:32, er1), @(-6:32, er2)
;;;	.word	0x7814
;;;	.word	0x6a2c
;;;	.long	-5
;;;	.word	0xca20
;;;	.long	-6

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	byte_src+5 er1	
	test_h_gr32	byte_dst+6 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1

cmp_b_indexb16_indexb16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#0xffffff01, er1
	mov	#0xffffff02, er2
	set_ccr_zero
	cmp.b	@(byte_src-1:16, r1.b), @(byte_dst-2:16, r2.b)
;;; 	.word	0x0175
;;; 	.word	0x6e1c
;;; 	.word	byte_src-1
;;; 	.word	0xd220
;;; 	.word	byte_dst-2

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	0xffffff01 er1	
	test_h_gr32	0xffffff02 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1
.if 1				; ambiguous
cmp_b_indexw16_indexw16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#0xffff0003, er1
	mov	#0xffff0004, er2
	set_ccr_zero
	cmp.b	@(byte_src-3:16, r1.w), @(byte_dst-4:16, r2.w)
;;; 	.word	0x0176
;;; 	.word	0x6e1c
;;; 	.word	byte_src-3
;;; 	.word	0xe220
;;; 	.word	byte_dst-4

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	0xffff0003 er1	
	test_h_gr32	0xffff0004 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1
.endif

cmp_b_indexl16_indexl16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#0x00000005, er1
	mov	#0x00000006, er2
	set_ccr_zero
	cmp.b	@(byte_src-5:16, er1.l), @(byte_dst-6:16, er2.l)
;;; 	.word	0x0177
;;; 	.word	0x6e1c
;;; 	.word	byte_src-5
;;; 	.word	0xf220
;;; 	.word	byte_dst-6

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	0x00000005 er1	
	test_h_gr32	0x00000006 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1

cmp_b_indexb32_indexb32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#0xffffff01, er1
	mov	#0xffffff02, er2
	set_ccr_zero
	cmp.b	@(byte_src-1:32, r1.b), @(byte_dst-2:32, r2.b)
;;;	.word	0x7815
;;;	.word	0x6a2c
;;;	.long	byte_src-1
;;;	.word	0xda20
;;;	.long	byte_dst-2

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	0xffffff01 er1	
	test_h_gr32	0xffffff02 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1

.if 1				; ambiguous
cmp_b_indexw32_indexw32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#0xffff0003, er1
	mov	#0xffff0004, er2
	set_ccr_zero
	cmp.b	@(byte_src-3:32, r1.w), @(byte_dst-4:32, r2.w)
;;;	.word	0x7816
;;;	.word	0x6a2c
;;;	.long	byte_src-3
;;;	.word	0xea20
;;;	.long	byte_dst-4

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	0xffff0003 er1	
	test_h_gr32	0xffff0004 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1
.endif

cmp_b_indexl32_indexl32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov	#0x00000005, er1
	mov	#0x00000006, er2
	set_ccr_zero
	cmp.b	@(byte_src-5:32, er1.l), @(byte_dst-6:32, er2.l)
;;;	.word	0x7817
;;;	.word	0x6a2c
;;;	.long	byte_src-5
;;;	.word	0xfa20
;;;	.long	byte_dst-6

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_gr_a5a5	0
	test_h_gr32	0x00000005 er1	
	test_h_gr32	0x00000006 er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1

cmp_b_abs16_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero
	cmp.b	@byte_src:16, @byte_dst:16

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_grs_a5a5
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1

cmp_b_abs32_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero
	cmp.b	@byte_src:32, @byte_dst:32

	test_neg_clear		; N=0, Z=0, V=1, C=0
	test_zero_clear
	test_ovf_set
	test_carry_clear

	test_grs_a5a5
	cmp.b	#0x5a, @byte_src:16
	bne	fail1
	cmp.b	#0xa5, @byte_dst:16
	bne	fail1

.endif
	pass

	exit 0

fail1:	fail
