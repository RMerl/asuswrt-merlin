# Hitachi H8 testcase 'rotl'
# mach(): all
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"

	start

	.data
byte_dest:	.byte	0xa5
	.align 2
word_dest:	.word	0xa5a5
	.align 4
long_dest:	.long	0xa5a5a5a5

	.text

rotl_b_reg8_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	rotl.b	r0l		; shift left arithmetic by one

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear		
	test_neg_clear
	test_h_gr16 0xa54b r0	; 1010 0101 -> 0100 1011
.if (sim_cpu)
	test_h_gr32 0xa5a5a54b er0
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
rotl_b_ind_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	rotl.b	@er0	; shift right arithmetic by one, indirect

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0100 1011
	cmp.b	#0x4b, @byte_dest
	beq	.Lbind1
	fail
.Lbind1:
	mov.b	#0xa5, @byte_dest

rotl_b_indexb16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r0l
	rotl.b	@(byte_dest-5:16, r0.b)	; indexed byte/byte

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a5a505 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0100 1011
	cmp.b	#0x4b, @byte_dest
	beq	.Lbindexb161
	fail
.Lbindexb161:
	mov.b	#0xa5, @byte_dest

rotl_b_indexw16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r0
	rotl.b	@(byte_dest-256:16, r0.w)	; indexed byte/word

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a50100 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0100 1011
	cmp.b	#0x4b, @byte_dest
	beq	.Lbindexw161
	fail
.Lbindexw161:
	mov.b	#0xa5, @byte_dest

rotl_b_indexl16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er0
	rotl.b	@(byte_dest+1:16, er0.l)	; indexed byte/long

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xffffffff er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0100 1011
	cmp.b	#0x4b, @byte_dest
	beq	.Lbindexl161
	fail
.Lbindexl161:
	mov.b	#0xa5, @byte_dest

rotl_b_indexb32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r1l
	rotl.b	@(byte_dest-5:32, r1.b)	; indexed byte/byte

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a5a505 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0100 1011
	cmp.b	#0x4b, @byte_dest
	beq	.Lbindexb321
	fail
.Lbindexb321:
	mov.b	#0xa5, @byte_dest

rotl_b_indexw32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r1
	rotl.b	@(byte_dest-256:32, r1.w)	; indexed byte/word

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a50100 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0100 1011
	cmp.b	#0x4b, @byte_dest
	beq	.Lbindexw321
	fail
.Lbindexw321:
	mov.b	#0xa5, @byte_dest

rotl_b_indexl32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er1
	rotl.b	@(byte_dest+1:32, er1.l)	; indexed byte/long

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xffffffff er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 0100 1011
	cmp.b	#0x4b, @byte_dest
	beq	.Lbindexl321
	fail
.Lbindexl321:
	mov.b	#0xa5, @byte_dest

.endif

rotl_b_reg8_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	rotl.b	#2, r0l		; shift left arithmetic by two

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear		
	test_neg_set

	test_h_gr16 0xa596 r0	; 1010 0101 -> 1001 0110
.if (sim_cpu)
	test_h_gr32 0xa5a5a596 er0
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
rotl_b_ind_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov	#byte_dest, er0
	rotl.b	#2, @er0	; shift right arithmetic by one, indirect

	test_carry_clear	; H=0 N=1 Z=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  byte_dest er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1001 0110
	cmp.b	#0x96, @byte_dest
	beq	.Lbind2
	fail
.Lbind2:
	mov.b	#0xa5, @byte_dest

rotl_b_indexb16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r0l
	rotl.b	#2, @(byte_dest-5:16, r0.b)	; indexed byte/byte

	test_carry_clear	; H=0 N=1 Z=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a5a505 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1001 0110
	cmp.b	#0x96, @byte_dest
	beq	.Lbindexb162
	fail
.Lbindexb162:
	mov.b	#0xa5, @byte_dest

rotl_b_indexw16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r0
	rotl.b	#2, @(byte_dest-256:16, r0.w)	; indexed byte/word

	test_carry_clear	; H=0 N=1 Z=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a50100 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1001 0110
	cmp.b	#0x96, @byte_dest
	beq	.Lbindexw162
	fail
.Lbindexw162:
	mov.b	#0xa5, @byte_dest

rotl_b_indexl16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er0
	rotl.b	#2, @(byte_dest+1:16, er0.l)	; indexed byte/long

	test_carry_clear	; H=0 N=1 Z=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xffffffff er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1001 0110
	cmp.b	#0x96, @byte_dest
	beq	.Lbindexl162
	fail
.Lbindexl162:
	mov.b	#0xa5, @byte_dest

rotl_b_indexb32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r1l
	rotl.b	#2, @(byte_dest-5:32, r1.b)	; indexed byte/byte

	test_carry_clear	; H=0 N=1 Z=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a5a505 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1001 0110
	cmp.b	#0x96, @byte_dest
	beq	.Lbindexb322
	fail
.Lbindexb322:
	mov.b	#0xa5, @byte_dest

rotl_b_indexw32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r1
	rotl.b	#2, @(byte_dest-256:32, r1.w)	; indexed byte/word

	test_carry_clear	; H=0 N=1 Z=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a50100 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1001 0110
	cmp.b	#0x96, @byte_dest
	beq	.Lbindexw322
	fail
.Lbindexw322:
	mov.b	#0xa5, @byte_dest

rotl_b_indexl32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er1
	rotl.b	#2, @(byte_dest+1:32, er1.l)	; indexed byte/long

	test_carry_clear	; H=0 N=1 Z=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xffffffff er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 -> 1001 0110
	cmp.b	#0x96, @byte_dest
	beq	.Lbindexl322
	fail
.Lbindexl322:
	mov.b	#0xa5, @byte_dest

.endif

.if (sim_cpu)			; Not available in h8300 mode
rotl_w_reg16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	rotl.w	r0		; shift left arithmetic by one

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear		
	test_neg_clear
	test_h_gr16 0x4b4b r0	; 1010 0101 1010 0101 -> 0100 1011 0100 1011
	test_h_gr32 0xa5a54b4b er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
rotl_w_indexb16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r0l
	rotl.w	@(word_dest-10:16, r0.b)	; indexed word/byte

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a5a505 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0100 1011 0100 1011
	cmp.w	#0x4b4b, @word_dest
	beq	.Lwindexb161
	fail
.Lwindexb161:
	mov.w	#0xa5a5, @word_dest

rotl_w_indexw16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r0
	rotl.w	@(word_dest-512:16, r0.w)	; indexed word/word

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a50100 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0100 1011 0100 1011
	cmp.w	#0x4b4b, @word_dest
	beq	.Lwindexw161
	fail
.Lwindexw161:
	mov.w	#0xa5a5, @word_dest

rotl_w_indexl16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er0
	rotl.w	@(word_dest+2:16, er0.l)	; indexed word/long

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xffffffff er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0100 1011 0100 1011
	cmp.w	#0x4b4b, @word_dest
	beq	.Lwindexl161
	fail
.Lwindexl161:
	mov.w	#0xa5a5, @word_dest

rotl_w_indexb32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r1l
	rotl.w	@(word_dest-10:32, r1.b)	; indexed word/byte

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a5a505 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0100 1011 0100 1011
	cmp.w	#0x4b4b, @word_dest
	beq	.Lwindexb321
	fail
.Lwindexb321:
	mov.w	#0xa5a5, @word_dest

rotl_w_indexw32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r1
	rotl.w	@(word_dest-512:32, r1.w)	; indexed word/byte

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a50100 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0100 1011 0100 1011
	cmp.w	#0x4b4b, @word_dest
	beq	.Lwindexw321
	fail
.Lwindexw321:
	mov.w	#0xa5a5, @word_dest

rotl_w_indexl32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er1
	rotl.w	@(word_dest+2:32, er1.l)	; indexed word/byte

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xffffffff er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 0100 1011 0100 1011
	cmp.w	#0x4b4b, @word_dest
	beq	.Lwindexl321
	fail
.Lwindexl321:
	mov.w	#0xa5a5, @word_dest
.endif

rotl_w_reg16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	rotl.w	#2, r0		; shift left arithmetic by two

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear		
	test_neg_set
	test_h_gr16 0x9696 r0	; 1010 0101 1010 0101 -> 1001 0110 1001 0110
	test_h_gr32 0xa5a59696 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
rotl_w_indexb16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r0l
	rotl.w	#2, @(word_dest-10:16, r0.b)	; indexed word/byte

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a5a505 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1001 0110 1001 0110
	cmp.w	#0x9696, @word_dest
	beq	.Lwindexb162
	fail
.Lwindexb162:
	mov.w	#0xa5a5, @word_dest

rotl_w_indexw16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r0
	rotl.w	#2, @(word_dest-512:16, r0.w)	; indexed word/word

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a50100 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1001 0110 1001 0110
	cmp.w	#0x9696, @word_dest
	beq	.Lwindexw162
	fail
.Lwindexw162:
	mov.w	#0xa5a5, @word_dest

rotl_w_indexl16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er0
	rotl.w	#2, @(word_dest+2:16, er0.l)	; indexed word/long

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xffffffff er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1001 0110 1001 0110
	cmp.w	#0x9696, @word_dest
	beq	.Lwindexl162
	fail
.Lwindexl162:
	mov.w	#0xa5a5, @word_dest

rotl_w_indexb32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r1l
	rotl.w	#2, @(word_dest-10:32, r1.b)	; indexed word/byte

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a5a505 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1001 0110 1001 0110
	cmp.w	#0x9696, @word_dest
	beq	.Lwindexb322
	fail
.Lwindexb322:
	mov.w	#0xa5a5, @word_dest

rotl_w_indexw32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r1
	rotl.w	#2, @(word_dest-512:32, r1.w)	; indexed word/byte

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a50100 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1001 0110 1001 0110
	cmp.w	#0x9696, @word_dest
	beq	.Lwindexw322
	fail
.Lwindexw322:
	mov.w	#0xa5a5, @word_dest

rotl_w_indexl32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er1
	rotl.w	#2, @(word_dest+2:32, er1.l)	; indexed word/byte

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xffffffff er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 -> 1001 0110 1001 0110
	cmp.w	#0x9696, @word_dest
	beq	.Lwindexl322
	fail
.Lwindexl322:
	mov.w	#0xa5a5, @word_dest
.endif

rotl_l_reg32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	rotl.l	er0		; shift left arithmetic by one

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear		
	test_neg_clear
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 0100 1011 0100 1011 0100 1011 0100 1011
	test_h_gr32 0x4b4b4b4b er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
rotl_l_indexb16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r0l
	rotl.l	@(long_dest-20:16, er0.b)	; indexed long/byte

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a5a505 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 0100 1011 0100 1011 0100 1011 0100 1011
	cmp.l	#0x4b4b4b4b, @long_dest
	beq	.Llindexb161
	fail
.Llindexb161:
	mov.l	#0xa5a5a5a5, @long_dest

rotl_l_indexw16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r0
	rotl.l	@(long_dest-1024:16, er0.w)	; indexed long/word

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a50100 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 0100 1011 0100 1011 0100 1011 0100 1011
	cmp.l	#0x4b4b4b4b, @long_dest
	beq	.Llindexw161
	fail
.Llindexw161:
	mov.l	#0xa5a5a5a5, @long_dest

rotl_l_indexl16_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er0
	rotl.l	@(long_dest+4:16, er0.l)	; indexed long/long

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xffffffff er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 0100 1011 0100 1011 0100 1011 0100 1011
	cmp.l	#0x4b4b4b4b, @long_dest
	beq	.Llindexl161
	fail
.Llindexl161:
	mov.l	#0xa5a5a5a5, @long_dest

rotl_l_indexb32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r1l
	rotl.l	@(long_dest-20:32, er1.b)	; indexed long/byte

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a5a505 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 0100 1011 0100 1011 0100 1011 0100 1011
	cmp.l	#0x4b4b4b4b, @long_dest
	beq	.Llindexb321
	fail
.Llindexb321:
	mov.l	#0xa5a5a5a5, @long_dest

rotl_l_indexw32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r1
	rotl.l	@(long_dest-1024:32, er1.w)	; indexed long/byte

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xa5a50100 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 0100 1011 0100 1011 0100 1011 0100 1011
	cmp.l	#0x4b4b4b4b, @long_dest
	beq	.Llindexw321
	fail
.Llindexw321:
	mov.l	#0xa5a5a5a5, @long_dest

rotl_l_indexl32_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er1
	rotl.l	@(long_dest+4:32, er1.l)	; indexed long/byte

	test_carry_set		; H=0 N=0 Z=0 V=0 C=1
	test_zero_clear
	test_ovf_clear
	test_neg_clear

	test_h_gr32  0xffffffff er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 0100 1011 0100 1011 0100 1011 0100 1011
	cmp.l	#0x4b4b4b4b, @long_dest
	beq	.Llindexl321
	fail
.Llindexl321:
	mov.l	#0xa5a5a5a5, @long_dest
.endif

rotl_l_reg32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	rotl.l	#2, er0		; shift left arithmetic by two

	test_carry_clear	; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear		
	test_neg_set
	; 1010 0101 1010 0101 1010 0101 1010 0101
	; -> 1001 0110 1001 0110 1001 0110 1001 0110
	test_h_gr32 0x96969696 er0

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8sx)
rotl_l_indexb16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r0l
	rotl.l	#2, @(long_dest-20:16, er0.b)	; indexed long/byte

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a5a505 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 1001 0110 1001 0110 1001 0110 1001 0110
	cmp.l	#0x96969696, @long_dest
	beq	.Llindexb162
	fail
.Llindexb162:
	mov.l	#0xa5a5a5a5, @long_dest

rotl_l_indexw16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r0
	rotl.l	#2, @(long_dest-1024:16, er0.w)	; indexed long/word

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a50100 er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 1001 0110 1001 0110 1001 0110 1001 0110
	cmp.l	#0x96969696, @long_dest
	beq	.Llindexw162
	fail
.Llindexw162:
	mov.l	#0xa5a5a5a5, @long_dest

rotl_l_indexl16_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er0
	rotl.l	#2, @(long_dest+4:16, er0.l)	; indexed long/long

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xffffffff er0
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 1001 0110 1001 0110 1001 0110 1001 0110
	cmp.l	#0x96969696, @long_dest
	beq	.Llindexl162
	fail
.Llindexl162:
	mov.l	#0xa5a5a5a5, @long_dest

rotl_l_indexb32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.b	#5, r1l
	rotl.l	#2, @(long_dest-20:32, er1.b)	; indexed long/byte

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a5a505 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 1001 0110 1001 0110 1001 0110 1001 0110
	cmp.l	#0x96969696, @long_dest
	beq	.Llindexb322
	fail
.Llindexb322:
	mov.l	#0xa5a5a5a5, @long_dest

rotl_l_indexw32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.w	#256, r1
	rotl.l	#2, @(long_dest-1024:32, er1.w)	; indexed long/byte

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xa5a50100 er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 1001 0110 1001 0110 1001 0110 1001 0110
	cmp.l	#0x96969696, @long_dest
	beq	.Llindexw322
	fail
.Llindexw322:
	mov.l	#0xa5a5a5a5, @long_dest

rotl_l_indexl32_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	mov.l	#0xffffffff, er1
	rotl.l	#2, @(long_dest+4:32, er1.l)	; indexed long/byte

	test_carry_clear		; H=0 N=1 Z=0 V=0 C=0
	test_zero_clear
	test_ovf_clear
	test_neg_set

	test_h_gr32  0xffffffff er1
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	; 1010 0101 1010 0101 1010 0101 1010 0101 
	; -> 1001 0110 1001 0110 1001 0110 1001 0110
	cmp.l	#0x96969696, @long_dest
	beq	.Llindexl322
	fail
.Llindexl322:
	mov.l	#0xa5a5a5a5, @long_dest
.endif
.endif

	pass

	exit 0

