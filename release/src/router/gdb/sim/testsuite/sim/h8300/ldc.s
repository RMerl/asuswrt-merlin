# Hitachi H8 testcase 'ldc'
# mach(): all
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"
	.data
byte_pre:
	.byte	0
byte_src:
	.byte	0xff
byte_post:
	.byte	0
	
	start

ldc_imm8_ccr:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0xff, ccr	; set all ccr flags high, immediate operand
	bcs	.L1		; carry flag set?
	fail
.L1:	bvs	.L2		; overflow flag set?
	fail
.L2:	beq	.L3		; zero flag set?
	fail
.L3:	bmi	.L4		; neg flag set?
	fail
.L4:
	ldc	#0, ccr		; set all ccr flags low, immediate operand
	bcc	.L5		; carry flag clear?
	fail
.L5:	bvc	.L6		; overflow flag clear?
	fail
.L6:	bne	.L7		; zero flag clear?
	fail
.L7:	bpl	.L8		; neg flag clear?
	fail
.L8:
	test_cc_clear
	test_grs_a5a5
	
ldc_reg8_ccr:
	set_grs_a5a5
	set_ccr_zero

	mov	#0xff, r0h
	ldc	r0h, ccr	; set all ccr flags high, reg operand
	bcs	.L11		; carry flag set?
	fail
.L11:	bvs	.L12		; overflow flag set?
	fail
.L12:	beq	.L13		; zero flag set?
	fail
.L13:	bmi	.L14		; neg flag set?
	fail
.L14:
	mov	#0, r0h
	ldc	r0h, ccr	; set all ccr flags low, reg operand
	bcc	.L15		; carry flag clear?
	fail
.L15:	bvc	.L16		; overflow flag clear?
	fail
.L16:	bne	.L17		; zero flag clear?
	fail
.L17:	bpl	.L18		; neg flag clear?
	fail
.L18:
	test_cc_clear
	test_h_gr16  0x00a5 r0	; Register 0 modified by test procedure.
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8300s || sim_cpu == h8sx)	; Earlier versions, no exr
ldc_imm8_exr:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0, exr
	ldc	#0x87, exr	; set exr to 0x87

	stc	exr, r0l	; retrieve and check exr value
	cmp.b	#0x87, r0l
	beq	.L19
	fail
.L19:
	test_h_gr16  0xa587 r0	; Register 0 modified by test procedure.
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
ldc_reg8_exr:	
	set_grs_a5a5
	set_ccr_zero

	ldc	#0, exr
	mov	#0x87, r0h
	ldc	r0h, exr	; set exr to 0x87

	stc	exr, r0l	; retrieve and check exr value
	cmp.b	#0x87, r0l
	beq	.L21
	fail
.L21:
	test_h_gr16  0x8787 r0	; Register 0 modified by test procedure.
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

ldc_abs16_ccr:
	set_grs_a5a5
	set_ccr_zero

	ldc	@byte_src:16, ccr	; abs16 src
	stc	ccr, r0l	; copy into general reg

	test_h_gr32 0xa5a5a5ff er0	; ff in r0l, a5 elsewhere.
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

ldc_abs16_exr:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0, exr
	ldc	@byte_src:16, exr	; abs16 src
	stc	exr, r0l	; copy into general reg

	test_h_gr32 0xa5a5a587 er0	; 87 in r0l, a5 elsewhere.
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

ldc_abs32_ccr:
	set_grs_a5a5
	set_ccr_zero

	ldc	@byte_src:32, ccr	; abs32 src
	stc	ccr, r0l	; copy into general reg

	test_h_gr32 0xa5a5a5ff er0	; ff in r0l, a5 elsewhere.
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

ldc_abs32_exr:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0, exr
	ldc	@byte_src:32, exr	; abs32 src
	stc	exr, r0l	; copy into general reg

	test_h_gr32 0xa5a5a587 er0	; 87 in r0l, a5 elsewhere.
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

ldc_disp16_ccr:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_pre, er1
	ldc	@(1:16, er1), ccr	; disp16 src
	stc	ccr, r0l	; copy into general reg

	test_h_gr32 0xa5a5a5ff er0	; ff in r0l, a5 elsewhere.
	test_h_gr32 byte_pre, er1	; er1 still contains address
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

ldc_disp16_exr:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0, exr
	mov	#byte_post, er1
	ldc	@(-1:16, er1), exr	; disp16 src
	stc	exr, r0l	; copy into general reg

	test_h_gr32 0xa5a5a587 er0	; 87 in r0l, a5 elsewhere.
	test_h_gr32 byte_post, er1	; er1 still contains address
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

ldc_disp32_ccr:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_pre, er1
	ldc	@(1:32, er1), ccr	; disp32 src
	stc	ccr, r0l	; copy into general reg

	test_h_gr32 0xa5a5a5ff er0	; ff in r0l, a5 elsewhere.
	test_h_gr32 byte_pre, er1	; er1 still contains address
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

ldc_disp32_exr:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0, exr
	mov	#byte_post, er1
	ldc	@(-1:32, er1), exr	; disp16 src
	stc	exr, r0l	; copy into general reg

	test_h_gr32 0xa5a5a587 er0	; 87 in r0l, a5 elsewhere.
	test_h_gr32 byte_post, er1	; er1 still contains address
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

ldc_postinc_ccr:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_src, er1
	ldc	@er1+, ccr	; postinc src
	stc	ccr, r0l	; copy into general reg

	test_h_gr32  0xa5a5a5ff er0	; ff in r0l, a5 elsewhere.
	test_h_gr32  byte_src+2, er1	; er1 still contains address
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
ldc_postinc_exr:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0, exr
	mov	#byte_src, er1
	ldc	@er1+, exr	; postinc src
	stc	exr, r0l	; copy into general reg

	test_h_gr32  0xa5a5a587 er0	; 87 in r0l, a5 elsewhere.
	test_h_gr32  byte_src+2, er1	; er1 still contains address
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
ldc_ind_ccr:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_src, er1
	ldc	@er1, ccr	; postinc src
	stc	ccr, r0l	; copy into general reg

	test_h_gr32 0xa5a5a5ff er0	; ff in r0l, a5 elsewhere.
	test_h_gr32 byte_src, er1	; er1 still contains address
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
ldc_ind_exr:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0, exr
	mov	#byte_src, er1
	ldc	@er1, exr	; postinc src
	stc	exr, r0l	; copy into general reg

	test_h_gr32 0xa5a5a587 er0	; 87 in r0l, a5 elsewhere.
	test_h_gr32 byte_src, er1	; er1 still contains address
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.endif
	
.if (sim_cpu == h8sx)		; New vbr and sbr registers for h8sx
ldc_reg_sbr:	
	set_grs_a5a5
	set_ccr_zero

	mov	#0xaaaaaaaa, er0
	ldc	er0, sbr	; set sbr to 0xaaaaaaaa
 	stc	sbr, er1	; retreive and check sbr value

	test_h_gr32 0xaaaaaaaa er1
	test_h_gr32 0xaaaaaaaa er0 ; Register 0 modified by test procedure.
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

ldc_reg_vbr:	
	set_grs_a5a5
	set_ccr_zero

	mov	#0xaaaaaaaa, er0
	ldc	er0, vbr	; set sbr to 0xaaaaaaaa
	stc	vbr, er1	; retreive and check sbr value

	test_h_gr32 0xaaaaaaaa er1
	test_h_gr32 0xaaaaaaaa er0 ; Register 0 modified by test procedure.
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.endif
	pass

	exit 0
