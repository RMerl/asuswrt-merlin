# Hitachi H8 testcase 'sub.b'
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
	# sub.b #xx:8, rd	; <illegal>
	# sub.b #xx:8, @erd	;         7 d rd ???? a ???? xxxxxxxx
	# sub.b #xx:8, @erd+	; 0 1 7 4 6 c rd 1??? a ???? xxxxxxxx
	# sub.b #xx:8, @erd-	; 0 1 7 6 6 c rd 1??? a ???? xxxxxxxx
	# sub.b rs, rd		;                     1 8 rs rd
	# sub.b reg8, @erd	;         7 d rd ???? 1 8 rs ????
	# sub.b reg8, @erd+	;         0 1 7     9 8 rd 3 rs
	# sub.b reg8, @erd-	;         0 1 7     9 a rd 3 rs
	#

	# Coming soon:
	# sub.b #xx:8, @+erd	; 0 1 7 5 6 c rd 1??? a ???? xxxxxxxx
	# sub.b #xx:8, @-erd	; 0 1 7 7 6 c rd 1??? a ???? xxxxxxxx
	# sub.b reg8, @+erd	;         0 1 7     9 9 rd 3 rs
	# sub.b reg8, @-erd	;         0 1 7     9 b rd 3 rs
	# ...

.data
pre_byte:	.byte 0
byte_dest:	.byte 0xa5
post_byte:	.byte 0

	start
	
.if (0)				; Guess what?  Sub.b immediate reg8 is illegal!
sub_b_imm8_reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  sub.b #xx:8,Rd
	sub.b	#5, r0l		; Immediate 8-bit operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
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
.endif

.if (sim_cpu == h8sx)
sub_b_imm8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  sub.b #xx:8,@eRd
	mov	#byte_dest, er0
	sub.b	#5:8, @er0	; Immediate 8-bit src, reg indirect dst
;;; 	.word	0x7d00
;;; 	.word	0xa105

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

	;; Now check the result of the sub to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0xa0, r0l
	beq	.L1
	fail
.L1:

sub_b_imm8_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  sub.b #xx:8,@eRd+
	mov	#byte_dest, er0
	sub.b	#5:8, @er0+	; Immediate 8-bit src, reg post-incr dest
;;; 	.word	0x0174
;;; 	.word	0x6c08
;;; 	.word	0xa105

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set
	
	test_h_gr32 post_byte, er0	; er0 still contains address plus one
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x9b, r0l
	beq	.L2
	fail
.L2:

sub_b_imm8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  sub.b #xx:8,@eRd-
	mov	#byte_dest, er0
	sub.b	#5:8, @er0-	; Immediate 8-bit src, reg post-decr dest
;;; 	.word	0x0176
;;; 	.word	0x6c08
;;; 	.word	0xa105

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set
	
	test_h_gr32 pre_byte, er0	; er0 still contains address minus one
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x96, r0l
	beq	.L3
	fail
.L3:

.endif

sub_b_reg8_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  sub.b Rs,Rd
	mov.b	#5, r0h
	sub.b	r0h, r0l	; Register operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
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

.if (sim_cpu == h8sx)
sub_b_reg8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  sub.b rs8,@eRd	; Subx to register indirect
	mov	#byte_dest, er0
	mov	#5, r1l
	sub.b	r1l, @er0	; reg8 src, reg indirect dest
;;; 	.word	0x7d00
;;; 	.word	0x1890

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 byte_dest er0	; er0 still contains address
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x91, r0l
	beq	.L4
	fail
.L4:

sub_b_reg8_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  sub.b rs8,@eRd+	; Subx to register indirect
	mov	#byte_dest, er0
	mov	#5, r1l
	sub.b	r1l, @er0+	; reg8 src, reg indirect dest
;;; 	.word	0x0179
;;; 	.word	0x8039

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 post_byte er0	; er0 still contains address plus one
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x8c, r0l
	beq	.L5
	fail
.L5:

sub_b_reg8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  sub.b rs8,@eRd-	; Subx to register indirect
	mov	#byte_dest, er0
	mov	#5, r1l
	sub.b	r1l, @er0-	; reg8 src, reg indirect dest
;;; 	.word	0x0179
;;; 	.word	0xa039

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_set

	test_h_gr32 pre_byte er0	; er0 still contains address minus one
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the sub to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#0x87, r0l
	beq	.L6
	fail
.L6:

.endif

	pass

	exit 0
