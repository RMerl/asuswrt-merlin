	.h8300s
	.section .text
	.align 2
	.global _main
	.global movb_tests
	.global movw_tests
	.global movl_tests
	.global ldm_stm_tests
	.global movfpe_movtpe_tests
	.global add_sub_addx_subx_tests
	.global inc_dec_adds_subs_tests
	.global daa_das_tests
	.global mul_div_tests
	.global cmp_tests
	.global neg_tests
	.global ext_tests
	.global tas_mac_tests
	.global logic_operations_tests
	.global sha_shl_tests
	.global rot_rotx_tests
	.global bset_bclr_tests
	.global bnot_btst_tests
	.global band_bor_bxor_tests
	.global bld_bst_tests
	.global branch_tests
	.global system_control_tests
	.global block_data_transfer_tests
_main:
	nop

movb_tests:
	mov.b	r0l,r0h
	mov.b	#0x12,r1l
	mov.b	@er0,r1h
	mov.b	@(0x1234:16,er0),r2l
	mov.b	@(0x12345678:32,er0),r2h
	mov.b	@er0+,r3l
	mov.b	@0x12:8,r3h
	mov.b	@0x1234:16,r4l
	mov.b	@0x12345678:32,r4h

movw_tests:
	mov.w	e0,r0
	mov.w	#0x1234,r1
	mov.w	@er0,r2
	mov.w	@(0x1234:16,er0),r3
	mov.w	@(0x12345678:32,er0),r4
	mov.w	@er0+,r5
	mov.w	@0x1234:16,r6
	mov.w	@0x12345678:32,r7

movl_tests:
	mov.l	er0,er1
	mov.l	#0x12345678,er1
	mov.l	@er0,er2
	mov.l	@(0x1234:16,er0),er3
	mov.l	@(0x12345678:32,er0),er4
	mov.l	@er0+,er5
	mov.l	@0x1234:16,er6
	mov.l	@0x12345678:32,er7

ldm_stm_tests:
	ldm.l	@sp+,er0-er1
	ldm.l	@sp+,er0-er2
	ldm.l	@sp+,er0-er3
	stm.l	er0-er1,@-sp
	stm.l	er0-er2,@-sp
	stm.l	er0-er3,@-sp

movfpe_movtpe_tests:
	movfpe	@0x1234:16,r2l
	movtpe	r2l,@0x1234:16

add_sub_addx_subx_tests:
	add.b	#0x12,r0l
	add.b	r1l,r1h
	add.w	#0x1234,r2
	add.w	r3,r4
	add.l	#0x12345678,er5
	add.l	er6,er7
	sub.b	r1l,r1h
	sub.w	#0x1234,r2
	sub.w	r3,r4
	sub.l	#0x12345678,er5
	sub.l	er6,er7
	addx	#0x12,r0l
	addx	r1l,r1h
	subx	#0x12,r0l
	subx	r1l,r1h

inc_dec_adds_subs_tests:
	inc.b	r0l
	inc.w	#0x1,r4
	inc.w	#0x2,r3
	inc.l	#0x1,er2
	inc.l	#0x2,er1
	dec.b	r0l
	dec.w	#0x1,r4
	dec.w	#0x2,r3
	dec.l	#0x1,er2
	dec.l	#0x2,er1
	adds	#0x1,er7
	adds	#0x2,er6
	adds	#0x4,er5
	subs	#0x1,er7
	subs	#0x2,er6
	subs	#0x4,er5

daa_das_tests:
	daa	r0l
	das	r0h

mul_div_tests:
	mulxs.b	r0l,r1
	mulxs.w	r2,er3
	mulxu.b	r0l,e1
	mulxu.w	e2,er3
	divxs.b	r0l,r1
	divxs.w	r2,er3
	divxu.b	r0l,e1
	divxu.w	e2,er3

cmp_tests:
	cmp.b	#0x12,r0l
	cmp.b	r1l,r1h
	cmp.w	#0x1234,r2
	cmp.w	r3,e3
	cmp.l	#0x12345678,er4
	cmp.l	er5,er6

neg_tests:
	neg.b	r0l
	neg.w	r2
	neg.l	er3

ext_tests:
	exts.w	r0
	exts.l	er1
	extu.w	r2
	extu.l	er3

tas_mac_tests:
	tas	@er0
	mac	@er1+,@er2+
	clrmac
	ldmac	er4,mach
	ldmac	er5,macl
	stmac	mach,er6
	stmac	macl,er7

logic_operations_tests:
	and.b	#0x12,r0l
	and.b	r1l,r2h
	and.w	#0x1234,r0
	and.w	r1,r2
	and.l	#0x12345678,er0
	and.l	er1,er2
	or.b	#0x12,r0l
	or.b	r1l,r2h
	or.w	#0x1234,r0
	or.w	r1,r2
	or.l	#0x12345678,er0
	or.l	er1,er2
	xor.b	#0x12,r0l
	xor.b	r1l,r2h
	xor.w	#0x1234,r0
	xor.w	r1,r2
	xor.l	#0x12345678,er0
	xor.l	er1,er2
	not.b	r0l
	not.w	r1
	not.l	er2

sha_shl_tests:
	shal	r0l
	shal	r1
	shal	er2
	shar	r3l
	shar	r4
	shar	er5
	shll	r0l
	shll	r1
	shll	er2
	shlr	r3l
	shlr	r4
	shlr	er5

rot_rotx_tests:
	rotl	r0l
	rotl	r1
	rotl	er2
	rotr	r3l
	rotr	r4
	rotr	er5
	rotxl	r0l
	rotxl	r1
	rotxl	er2
	rotxr	r3l
	rotxr	r4
	rotxr	er5

bset_bclr_tests:
	bset	#0x7,r0l
	bset	#0x6,@er1
	bset	#0x5,@0x12:8
	bset	#0x4,@0x1234:16
	bset	#0x3,@0x12345678:32
	bset	r7l,r0h
	bset	r6l,@er1
	bset	r5l,@0x12:8
	bset	r4l,@0x1234:16
	bset	r3l,@0x12345678:32
	bclr	#0x7,r0l
	bclr	#0x6,@er1
	bclr	#0x5,@0x12:8
	bclr	#0x4,@0x1234:16
	bclr	#0x3,@0x12345678:32
	bclr	r7h,r0h
	bclr	r6h,@er1
	bclr	r5h,@0x12:8
	bclr	r4h,@0x1234:16
	bclr	r3h,@0x12345678:32

bnot_btst_tests:
	bnot	#0x7,r0l
	bnot	#0x6,@er1
	bnot	#0x5,@0x12:8
	bnot	#0x4,@0x1234:16
	bnot	#0x3,@0x12345678:32
	bnot	r7l,r0h
	bnot	r6l,@er1
	bnot	r5l,@0x12:8
	bnot	r4l,@0x1234:16
	bnot	r3l,@0x12345678:32
	btst	#0x7,r0l
	btst	#0x6,@er1
	btst	#0x5,@0x12:8
	btst	#0x4,@0x1234:16
	btst	#0x3,@0x12345678:32
	btst	r7h,r0h
	btst	r6h,@er1
	btst	r5h,@0x12:8
	btst	r4h,@0x1234:16
	btst	r3h,@0x12345678:32

band_bor_bxor_tests:
	band	#0x7,r0l
	band	#0x6,@er1
	band	#0x5,@0x12:8
	band	#0x4,@0x1234:16
	band	#0x3,@0x12345678:32
	bor	#0x7,r0l
	bor	#0x6,@er1
	bor	#0x5,@0x12:8
	bor	#0x4,@0x1234:16
	bor	#0x3,@0x12345678:32
	bxor	#0x7,r0l
	bxor	#0x6,@er1
	bxor	#0x5,@0x12:8
	bxor	#0x4,@0x1234:16
	bxor	#0x3,@0x12345678:32

bld_bst_tests:
	bld	#0x7,r0l
	bld	#0x6,@er1
	bld	#0x5,@0x12:8
	bld	#0x4,@0x1234:16
	bld	#0x3,@0x12345678:32
	bild	#0x7,r0l
	bild	#0x6,@er1
	bild	#0x5,@0x12:8
	bild	#0x4,@0x1234:16
	bild	#0x3,@0x12345678:32
	bst	#0x7,r0l
	bst	#0x6,@er1
	bst	#0x5,@0x12:8
	bst	#0x4,@0x1234:16
	bst	#0x3,@0x12345678:32
	bist	#0x7,r0l
	bist	#0x6,@er1
	bist	#0x5,@0x12:8
	bist	#0x4,@0x1234:16
	bist	#0x3,@0x12345678:32

branch_tests:
	bra	branch_tests
	brn	branch_tests
	bhi	branch_tests
	bls	branch_tests
	bcc	branch_tests
	bcs	branch_tests
	bne	branch_tests
	beq	branch_tests
	bvc	branch_tests
	bvs	branch_tests
	bpl	branch_tests
	bmi	branch_tests
	bge	branch_tests
	blt	branch_tests
	bgt	branch_tests
	ble	branch_tests
	jmp	@er0
	jmp	@branch_tests
	jmp	@@0 (0)
	bsr	branch_tests:8
	bsr	branch_tests:16
	jsr	@er0
	jsr	@branch_tests
	jsr	@@0 (0)
	rts

system_control_tests:
	trapa	#0x2
	rte
	sleep
	ldc	#0x12,ccr
	ldc	r3l,ccr
	ldc	@er0,ccr
	ldc	@(0x1234:16,er0),ccr
	ldc	@(0x12345678:32,er0),ccr
	ldc	@er1+,ccr
	ldc	@0x1234:16,ccr
	ldc	@0x12345678:32,ccr
	stc	ccr,r3l
	stc	ccr,@er0
	stc	ccr,@(0x1234:16,er0)
	stc	ccr,@(0x12345678:32,er0)
	stc	ccr,@-er1
	stc	ccr,@0x1234:16
	stc	ccr,@0x12345678:32
	andc	#0x12,ccr
	orc	#0x34,ccr
	xorc	#0x56,ccr
	ldc	#0x12,exr
	ldc	r3l,exr
	ldc	@er0,exr
	ldc	@(0x1234:16,er0),exr
	ldc	@(0x12345678:32,er0),exr
	ldc	@er1+,exr
	ldc	@0x1234:16,exr
	ldc	@0x12345678:32,exr
	stc	exr,r3l
	stc	exr,@er0
	stc	exr,@(0x1234:16,er0)
	stc	exr,@(0x12345678:32,er0)
	stc	exr,@-er1
	stc	exr,@0x1234:16
	stc	exr,@0x12345678:32
	andc	#0x12,exr
	orc	#0x34,exr
	xorc	#0x56,exr
	nop

block_data_transfer_tests:
	eepmov.b
	eepmov.w
