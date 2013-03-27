# Hitachi H8 testcase 'add.b'
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
	# add.b #xx:8, rd	;                     8   rd xxxxxxxx
	# add.b #xx:8, @erd	;         7 d rd ???? 8 ???? xxxxxxxx
	# add.b #xx:8, @erd+	; 0 1 7 4 6 c rd 1??? 8 ???? xxxxxxxx
	# add.b #xx:8, @erd-	; 0 1 7 6 6 c rd 1??? 8 ???? xxxxxxxx
	# add.b #xx:8, @+erd	; 0 1 7 5 6 c rd 1??? 8 ???? xxxxxxxx
	# add.b #xx:8, @-erd	; 0 1 7 7 6 c rd 1??? 8 ???? xxxxxxxx
	# add.b #xx:8, @(d:16, erd)	; 0 1 7 4 6 e b30 | rd, b31, dd:16 8 ???? xxxxxxxx
	# add.b #xx:8, @(d:32, erd)	; 7 8 b30 | rd, 4 6 a 2 8 dd:32 8 ???? xxxxxxxx
	# add.b #xx:8, @aa:8		; 7 f aaaaaaaa 8 ???? xxxxxxxx
	# add.b #xx:8, @aa:16		; 6 a 1 1??? aa:16 8 ???? xxxxxxxx
	# add.b #xx:8, @aa:32		; 6 a 3 1??? aa:32 8 ???? xxxxxxxx
	# add.b rs, rd		;                     0 8 rs rd
	# add.b reg8, @erd	;         7 d rd ???? 0 8 rs ????
	# add.b reg8, @erd+	;         0 1 7     9 8 rd 1 rs
	# add.b reg8, @erd-	;         0 1 7     9 a rd 1 rs
	# add.b reg8, @+erd	;         0 1 7     9 9 rd 1 rs
	# add.b reg8, @-erd	;         0 1 7     9 b rd 1 rs
	# add.b reg8, @(d:16, erd)	; 0 1 7 9 c b30 | rd32, 1 rs8 imm16
	# add.b reg8, @(d:32, erd)	; 0 1 7 9 d b31 | rd32, 1 rs8 imm32
	# add.b reg8, @aa:8		; 7 f aaaaaaaa 0 8 rs ????
	# add.b reg8, @aa:16		; 6 a 1 1??? aa:16 0 8 rs ????
	# add.b reg8, @aa:32		; 6 a 3 1??? aa:32 0 8 rs ????
	#

	# Coming soon:
	# add.b #xx:8, @(d:2, erd)	; 0 1 7 b30 | b21 | dd:2,  8 ???? xxxxxxxx
	# add.b reg8, @(d:2, erd)	; 0 1 7 9 dd:2 rd32 1 rs8
	# ...

.data
pre_byte:	.byte 0
byte_dest:	.byte 0
post_byte:	.byte 0

	start
	
add_b_imm8_reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  add.b #xx:8,Rd
	add.b	#5:8, r0l	; Immediate 8-bit src, reg8 dst

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
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
	
.if (sim_cpu == h8sx)
add_b_imm8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b #xx:8,@eRd
	mov	#byte_dest, er0
	add.b	#5:8, @er0	; Immediate 8-bit src, reg indirect dst
;;; 	.word	0x7d00
;;; 	.word	0x8005

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

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#5, r0l
	beq	.L1
	fail
.L1:

add_b_imm8_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b #xx:8,@eRd+
	mov	#byte_dest, er0
	add.b	#5:8, @er0+	; Immediate 8-bit src, reg post-inc dst
;;; 	.word	0x0174
;;; 	.word	0x6c08
;;; 	.word	0x8005

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

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#10, r0l
	beq	.L2
	fail
.L2:

add_b_imm8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b #xx:8,@eRd-
	mov	#byte_dest, er0
	add.b	#5:8, @er0-	; Immediate 8-bit src, reg post-dec dst
;;; 	.word	0x0176
;;; 	.word	0x6c08
;;; 	.word	0x8005

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

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#15, r0l
	beq	.L3
	fail
.L3:

add_b_imm8_rdpreinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b #xx:8,@+eRd
	mov	#pre_byte, er0
	add.b	#5:8, @+er0	; Immediate 8-bit src, reg pre-inc dst
;;; 	.word	0x0175
;;; 	.word	0x6c08
;;; 	.word	0x8005

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

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#20, r0l
	beq	.L4
	fail
.L4:

add_b_imm8_rdpredec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b #xx:8,@-eRd
	mov	#post_byte, er0
	add.b	#5:8, @-er0	; Immediate 8-bit src, reg pre-dec dst
;;; 	.word	0x0177
;;; 	.word	0x6c08
;;; 	.word	0x8005

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

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#25, r0l
	beq	.L5
	fail
.L5:

add_b_imm8_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b #xx:8,@(dd:16, eRd)
	mov	#post_byte, er0
	add.b	#5:8, @(-1:16, er0)	; Immediate 8-bit src, 16-bit reg disp dest.
;;; 	.word	0x0174
;;; 	.word	0x6e08
;;; 	.word	0xffff
;;; 	.word	0x8005

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_h_gr32   post_byte, er0	; er0 contains address plus one
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#30, r0l
	beq	.L6
	fail
.L6:

add_b_imm8_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b #xx:8,@(dd:32, eRd)
	mov	#pre_byte, er0
	add.b	#5:8, @(1:32, er0)	; Immediate 8-bit src, 32-bit reg disp. dest.
;;; 	.word	0x7804
;;; 	.word	0x6a28
;;; 	.word	0x0000
;;; 	.word	0x0001
;;; 	.word	0x8005

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

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#35, r0l
	beq	.L7
	fail
.L7:

add_b_imm8_abs8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b reg8,@aa:8
	;; NOTE: for abs8, we will use the SBR register as a base,
	;; since otherwise we would have to make sure that the destination
	;; was in the zero page.
	;;
	mov	#byte_dest-100, er0
	ldc	er0, sbr
	add.b	#5, @100:8	; 8-bit reg src, 8-bit absolute dest
;;; 	.word	0x7f64
;;; 	.word	0x8005

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_h_gr32  byte_dest-100, er0	; reg 0 has base address
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#40, r0l
	beq	.L8
	fail
.L8:

add_b_imm8_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b #xx:8,@aa:16
	add.b	#5:8, @byte_dest:16	; Immediate 8-bit src, 16-bit absolute dest
;;;  	.word	0x6a18
;;; 	.word	byte_dest
;;;  	.word	0x8005

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#45, r0l
	beq	.L9
	fail
.L9:

add_b_imm8_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b #xx:8,@aa:32
	add.b	#5:8, @byte_dest:32	; Immediate 8-bit src, 32-bit absolute dest
;;;  	.word	0x6a38
;;; 	.long	byte_dest
;;;  	.word	0x8005

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#50, r0l
	beq	.L10
	fail
.L10:

.endif

add_b_reg8_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  add.b Rs,Rd
	mov.b	#5, r0h
	add.b	r0h, r0l	; Register operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
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

.if (sim_cpu == h8sx)
add_b_reg8_rdind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b rs8,@eRd	; Add to register indirect
	mov	#byte_dest, er0
	mov	#5, r1l
	add.b	r1l, @er0	; reg8 src, reg indirect dest
;;; 	.word	0x7d00
;;; 	.word	0x0890

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
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#55, r0l
	beq	.L11
	fail
.L11:

add_b_reg8_rdpostinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b rs8,@eRd+	; Add to register post-increment
	mov	#byte_dest, er0
	mov	#5, r1l
	add.b	r1l, @er0+	; reg8 src, reg post-incr dest
;;; 	.word	0x0179
;;; 	.word	0x8019

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 post_byte er0	; er0 contains address plus one
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#60, r0l
	beq	.L12
	fail
.L12:

add_b_reg8_rdpostdec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b rs8,@eRd-	; Add to register post-decrement
	mov	#byte_dest, er0
	mov	#5, r1l
	add.b	r1l, @er0-	; reg8 src, reg post-decr dest
;;; 	.word	0x0179
;;; 	.word	0xa019

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 pre_byte er0	; er0 contains address minus one
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#65, r0l
	beq	.L13
	fail
.L13:

add_b_reg8_rdpreinc:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b rs8,@+eRd	; Add to register pre-increment
	mov	#pre_byte, er0
	mov	#5, r1l
	add.b	r1l, @+er0	; reg8 src, reg pre-incr dest
;;; 	.word	0x0179
;;; 	.word	0x9019

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dest er0	; er0 contains destination address 
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#70, r0l
	beq	.L14
	fail
.L14:

add_b_reg8_rdpredec:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b rs8,@-eRd	; Add to register pre-decrement
	mov	#post_byte, er0
	mov	#5, r1l
	add.b	r1l, @-er0	; reg8 src, reg pre-decr dest
;;; 	.word	0x0179
;;; 	.word	0xb019

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 byte_dest er0	; er0 contains destination address 
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#75, r0l
	beq	.L15
	fail
.L15:

add_b_reg8_disp16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b rs8,@(dd:16, eRd)	; Add to register + 16-bit displacement
	mov	#pre_byte, er0
	mov	#5, r1l
	add.b	r1l, @(1:16, er0)	; reg8 src, 16-bit reg disp dest
;;;  	.word	0x0179
;;;  	.word	0xc019
;;;  	.word	0x0001

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 pre_byte er0	; er0 contains address minus one
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#80, r0l
	beq	.L16
	fail
.L16:

add_b_reg8_disp32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b rs8,@-eRd	; Add to register plus 32-bit displacement
	mov	#post_byte, er0
	mov	#5, r1l
	add.b	r1l, @(-1:32, er0)	; reg8 src, 32-bit reg disp dest
;;; 	.word	0x0179
;;; 	.word	0xd819
;;; 	.word	0xffff
;;; 	.word	0xffff

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32 post_byte er0	; er0 contains address plus one
	test_h_gr32 0xa5a5a505 er1	; er1 has the test load

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#85, r0l
	beq	.L17
	fail
.L17:

add_b_reg8_abs8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b reg8,@aa:8
	;; NOTE: for abs8, we will use the SBR register as a base,
	;; since otherwise we would have to make sure that the destination
	;; was in the zero page.
	;;
	mov	#byte_dest-100, er0
	ldc	er0, sbr
	mov	#5, r1l
	add.b	r1l, @100:8	; 8-bit reg src, 8-bit absolute dest
;;; 	.word	0x7f64
;;; 	.word	0x0890

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_h_gr32  byte_dest-100, er0	; reg 0 has base address
	test_h_gr32  0xa5a5a505 er1	; reg 1 has test load
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#90, r0l
	beq	.L18
	fail
.L18:

add_b_reg8_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b reg8,@aa:16
	mov	#5, r0l
	add.b	r0l, @byte_dest:16	; 8-bit reg src, 16-bit absolute dest
;;; 	.word	0x6a18
;;; 	.word	byte_dest
;;; 	.word	0x0880

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear
	
	test_h_gr32  0xa5a5a505 er0	; reg 0 has test load
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#95, r0l
	beq	.L19
	fail
.L19:

add_b_reg8_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  add.b reg8,@aa:32
	mov	#5, r0l
	add.b	r0l, @byte_dest:32	; 8-bit reg src, 32-bit absolute dest
;;; 	.word	0x6a38
;;; 	.long	byte_dest
;;; 	.word	0x0880

	test_carry_clear	; H=0 N=0 Z=0 V=0 C=0
	test_ovf_clear
	test_zero_clear
	test_neg_clear

	test_h_gr32  0xa5a5a505 er0	; reg 0 has test load
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	;; Now check the result of the add to memory.
	sub.b	r0l, r0l
	mov.b	@byte_dest, r0l
	cmp.b	#100, r0l
	beq	.L20
	fail
.L20:

.endif

	pass

	exit 0
