# Support macros for the sh assembly test cases.

	.equ	no_dsp,  0
	.equ	yes_dsp, 1

	.section	.rodata
	.align 2
_pass:	.string	"pass\n"
_fail:	.string "fail\n"
_stack:	.fill	128, 4, 0
stackt:	

	.macro	push reg
	mov.l	\reg, @-r15
	.endm

	.macro	pop reg
	mov.l	@r15+, \reg
	.endm

	.macro start
	.text
	.align 1
	.global start
start:	mov.l	stackp, r15
	bra	main
	nop
	.align 2
stackp:	.long	stackt
mpass:
	mov	#4, r4
	mov	#1, r5
	mov.l	ppass, r6
	mov	#5, r7
	trapa	#34
	rts
	nop
mfail:
	mov	#4, r4
	mov	#1, r5
	mov.l	pfail, r6
	mov	#5, r7
	trapa	#34
	mov	#1, r5
mexit:
	mov	#1, r4
	mov	#0, r6
	mov	#0, r7
	trapa	#34
	.align 2
ppass:	.long	_pass
pfail:	.long	_fail

mtesta5:
	push	r0
	mov.l	a5a5, r0
	cmp/eq	r1, r0
	bf	mfail
	cmp/eq	r2, r0
	bf	mfail
	cmp/eq	r3, r0
	bf	mfail
	cmp/eq	r4, r0
	bf	mfail
	cmp/eq	r5, r0
	bf	mfail
	cmp/eq	r6, r0
	bf	mfail
	cmp/eq	r7, r0
	bf	mfail
	cmp/eq	r8, r0
	bf	mfail
	cmp/eq	r9, r0
	bf	mfail
	cmp/eq	r10, r0
	bf	mfail
	cmp/eq	r11, r0
	bf	mfail
	cmp/eq	r12, r0
	bf	mfail
	cmp/eq	r13, r0
	bf	mfail
	cmp/eq	r14, r0
	bf	mfail
	# restore and check r0
	pop	r0
	cmp/eq	r0, r1
	bf	mfail
	# pass
	rts
	nop
.if (sim_cpu == no_dsp)
mtesta5_fp:
	push	r0
	flds	fr0, fpul
	sts	fpul, r0
	push	r0
	mov.l	a5a5, r0
	lds	r0, fpul
	fsts	fpul, fr0
	fcmp/eq	fr1, fr0
	bf	mfail
	fcmp/eq	fr2, fr0
	bf	mfail
	fcmp/eq	fr3, fr0
	bf	mfail
	fcmp/eq	fr4, fr0
	bf	mfail
	fcmp/eq	fr5, fr0
	bf	mfail
	fcmp/eq	fr6, fr0
	bf	mfail
	fcmp/eq	fr7, fr0
	bf	mfail
	fcmp/eq	fr8, fr0
	bf	mfail
	fcmp/eq	fr9, fr0
	bf	mfail
	fcmp/eq	fr10, fr0
	bf	mfail
	fcmp/eq	fr11, fr0
	bf	mfail
	fcmp/eq	fr12, fr0
	bf	mfail
	fcmp/eq	fr13, fr0
	bf	mfail
	fcmp/eq	fr14, fr0
	bf	mfail
	fcmp/eq	fr15, fr0
	bf	mfail
	# restore and check fr0
	pop	r0
	lds	r0, fpul
	fsts	fpul, fr0
	fcmp/eq	fr0, fr1
	bf	mfail
	# restore r0 and pass
	pop	r0
	rts
	nop
.endif

mseta5:
	mov.l	a5a5, r0
	mov.l	a5a5, r1
	mov.l	a5a5, r2
	mov.l	a5a5, r3
	mov.l	a5a5, r4
	mov.l	a5a5, r5
	mov.l	a5a5, r6
	mov.l	a5a5, r7
	mov.l	a5a5, r8
	mov.l	a5a5, r9
	mov.l	a5a5, r10
	mov.l	a5a5, r11
	mov.l	a5a5, r12
	mov.l	a5a5, r13
	mov.l	a5a5, r14
	rts
	nop

.if (sim_cpu == no_dsp)
mseta5_fp:
	push	r0
	mov.l	a5a5, r0
	lds	r0, fpul
	fsts	fpul, fr0
	fsts	fpul, fr1
	fsts	fpul, fr2
	fsts	fpul, fr3
	fsts	fpul, fr4
	fsts	fpul, fr5
	fsts	fpul, fr6
	fsts	fpul, fr7
	fsts	fpul, fr8
	fsts	fpul, fr9
	fsts	fpul, fr10
	fsts	fpul, fr11
	fsts	fpul, fr12
	fsts	fpul, fr13
	fsts	fpul, fr14
	fsts	fpul, fr15
	pop	r0
	rts
	nop
.endif

	.align 2
a5a5:	.long	0xa5a5a5a5
main:
	.endm

	.macro exit val
	mov	#\val, r5
	bra	mexit
	nop
	.endm

	.macro pass
	bsr	mpass
	nop
	.endm

	.macro fail
	bra	mfail
	nop
	.endm
	# Branch if false -- 8k range
	.macro bf8k label
	bt	.Lbf8k\@
	bra	\label
.Lbf8k\@:
	.endm

	# Branch if true -- 8k range
	.macro bt8k label
	bf	.Lbt8k\@
	bra	\label
.Lbt8k\@:
	.endm

	# Assert value of register (any general register but r0)
	# Preserves r0 on stack, restores it on success.
	.macro assertreg val reg
	push	r0
	mov.l	.Larval\@, r0
	cmp/eq	r0, \reg
	bt	.Lar\@
	fail
	.align 2
.Larval\@:
	.long	\val
.Lar\@:	pop	r0
	.endm

	# Assert value of register zero
	# Preserves r1 on stack, restores it on success.
	.macro assertreg0 val
	push	r1
	mov.l	.Lazval\@, r1
	cmp/eq	r1, r0
	bt	.Laz\@
	fail
	.align 2
.Lazval\@:
	.long	\val
.Laz\@:	pop	r1
	.endm

	# Assert value of system register 
	# [mach, macl, pr, dsr, a0, x0, x1, y0, y1, ...]
	.macro assert_sreg val reg
	push	r0
	sts	\reg, r0
	assertreg0 \val
	pop	r0
	.endm

	# Assert value of system register that isn't directly stc-able
	# [a1, m0, m1, ...]
	.macro assert_sreg2 val reg
	push	r0
	sts	a0, r0
	push	r0
	pcopy	\reg, a0
	sts	a0, r0
	assertreg0 \val
	pop	r0
	lds	r0, a0
	pop	r0
	.endm

	# Assert value of control register 
	# [gbr, vbr, ssr, spc, sgr, dbr, r[0-7]_bank, sr, mod, re, rs, ...]
	.macro assert_creg val reg
	push	r0
	stc	\reg, r0
	assertreg0 \val
	pop	r0
	.endm

	# Assert integer value of fp register
	# Preserves r0 on stack, restores it on success
	# Assumes single-precision fp mode
	.macro assert_fpreg_i val freg
	push	r0
	ftrc	\freg, fpul
	sts	fpul, r0
	assertreg0	\val
	pop	r0
	.endm

	# Assert integer value of dp register
	# Preserves r0 on stack, restores it on success
	# Assumes double-precision fp mode
	.macro assert_dpreg_i val dreg
	push	r0
	ftrc	\dreg, fpul
	sts	fpul, r0
	assertreg0	\val
	pop	r0
	.endm

	# Assert hex value of fp register
	# Preserves r0 on stack, restores it on success
	# Assumes single-precision fp mode
	.macro assert_fpreg_x val freg
	push	r0
	flds	\freg, fpul
	sts	fpul, r0
	assertreg0	\val
	pop	r0
	.endm

	# Set FP bank 0
	# Saves and restores r0 and r1
	.macro	bank0
	push	r0
	push	r1
	mov	#32, r1
	shll16	r1
	not	r1, r1
	sts	fpscr, r0
	and	r1, r0
	lds	r0, fpscr
	pop	r1
	pop	r0
	.endm

	# Set FP bank 1
	.macro	bank1
	push	r0
	push	r1
	mov	#32,  r1
	shll16	r1
	sts	fpscr, r0
	or	r1, r0
	lds	r0, fpscr
	pop	r1
	pop	r0
	.endm

	# Set FP 32-bit xfer
	.macro	sz_32
	push	r0
	push	r1
	mov	#16,  r1
	shll16	r1
	not	r1, r1
	sts	fpscr, r0
	and	r1, r0
	lds	r0, fpscr
	pop	r1
	pop	r0
	.endm

	# Set FP 64-bit xfer
	.macro	sz_64
	push	r0
	push	r1
	mov	#16,  r1
	shll16	r1
	sts	fpscr, r0
	or	r1, r0
	lds	r0, fpscr
	pop	r1
	pop	r0
	.endm

	# Set FP single precision
	.macro	single_prec
	push	r0
	push	r1
	mov	#8, r1
	shll16	r1
	not	r1, r1
	sts	fpscr, r0
	and	r1, r0
	lds	r0, fpscr
	pop	r1
	pop	r0
	.endm

	# Set FP double precision
	.macro	double_prec
	push	r0
	push	r1
	mov	#8, r1
	shll16	r1
	sts	fpscr, r0
	or	r1, r0
	lds	r0, fpscr
	pop	r1
	pop	r0
	.endm

	.macro	set_carry
	sett
	.endm

	.macro	set_ovf
	sett
	.endm

	.macro	clear_carry
	clrt
	.endm

	.macro	clear_ovf
	clrt
	.endm

	# sets, clrs


	.macro set_grs_a5a5
	bsr	mseta5
	nop
	.endm

	.macro set_greg val greg
	mov.l	gregval\@, \greg
	bra	set_greg\@
	nop
	.align	2
gregval\@:	.long	\val
set_greg\@:
	.endm

	.macro set_fprs_a5a5
	bsr	mseta5_fp
	nop
	.endm

	.macro test_grs_a5a5
	bsr	mtesta5
	nop
	.endm

	.macro test_fprs_a5a5	
	bsr	mtesta5_fp
	nop
	.endm

	.macro test_gr_a5a5 reg
	assertreg 0xa5a5a5a5 \reg
	.endm

	.macro test_fpr_a5a5 reg
	assert_fpreg_x 0xa5a5a5a5 \reg
	.endm

	.macro test_gr0_a5a5
	assertreg0 0xa5a5a5a5
	.endm

	# Perform a single to double precision floating point conversion.
	# Assumes correct settings of fpscr.
	.macro _s2d fpr dpr
	flds \fpr, fpul
	fcnvsd fpul, \dpr
	.endm

	# Manipulate the status register
	.macro set_sr	val
	push	r0
	mov.l	.Lsrval\@, r0
	ldc	r0, sr
	pop	r0
	bra	.Lsetsr\@
	nop
	.align 2
.Lsrval\@:
	.long	\val
.Lsetsr\@:
	.endm

	.macro	get_sr	reg
	stc	sr, \reg
	.endm

	.macro	test_sr val
	push	r0
	get_sr	r0
	assertreg0 \val
	pop	r0
	.endm

	.macro	set_sr_bit val
	push	r0
	push	r1
	get_sr	r0
	mov.l	.Lsrbitval\@, r1
	or	r1, r0
	ldc	r0, sr
	pop	r1
	pop	r0
	bra	.Lsrbit\@
	nop
	.align 2
.Lsrbitval\@:
	.long	\val
.Lsrbit\@:
	.endm

	.macro	test_sr_bit_set val
	push	r0
	push	r1
	get_sr	r0
	mov.l	.Ltsbsval\@, r1
	tst	r1, r0
	bf	.Ltsbs\@
	fail
	.align	2
.Ltsbsval\@:
	.long	\val
.Ltsbs\@:
	pop	r1
	pop	r0
	.endm

	.macro	test_sr_bit_clear val
	push	r0
	push	r1
	get_sr	r0
	mov.l	.Ltsbcval\@, r1
	not	r0, r0
	tst	r1, r0
	bf	.Ltsbc\@
	fail
	.align	2
.Ltsbcval\@:
	.long	\val
.Ltsbc\@:
	pop	r1
	pop	r0
	.endm

	# Set system registers
	.macro set_sreg val reg
	# [mach, macl, pr, dsr, a0, x0, x1, y0, y1, ...]
	push	r0
	mov.l	.Lssrval\@, r0
	lds	r0, \reg
	pop	r0
	bra	.Lssr\@
	nop
	.align 2
.Lssrval\@:
	.long	\val
.Lssr\@:
	.endm

	.macro set_sreg2 val reg
	# [a1, m0, m1, ...]
	push	r0
	sts	a0, r0
	push	r0
	mov.l	.Lssr2val\@, r0
	lds	r0, a0
	pcopy	a0, \reg
	pop	r0
	lds	r0, a0
	pop	r0
	bra	.Lssr2_\@
	nop
	.align 2
.Lssr2val\@:
	.long	\val
.Lssr2_\@:
	.endm


	.macro set_creg val reg
	# [gbr, vbr, ssr, spc, sgr, dbr... ]
	push	r0
	mov.l	.Lscrval\@, r0
	ldc	r0, \reg
	pop	r0
	bra	.Lscr\@
	nop
	.align 2
.Lscrval\@:
	.long	\val
.Lscr\@:
	.endm

	.macro	set_dctrue
	push	r0
	sts	dsr, r0
	or	#1, r0
	lds	r0, dsr
	pop	r0
	.endm

	.macro	set_dcfalse
	push	r0
	sts	dsr, r0
	not	r0, r0
	or	#1, r0
	not	r0, r0
	lds	r0, dsr
	pop	r0
	.endm

	.macro	assertmem addr val
	push	r0
	mov.l	.Laddr\@, r0
	mov.l	@r0, r0
	assertreg0 \val
	bra	.Lam\@
	nop
	.align	2
.Laddr\@:
	.long	\addr
.Lam\@:	pop	r0
	.endm
