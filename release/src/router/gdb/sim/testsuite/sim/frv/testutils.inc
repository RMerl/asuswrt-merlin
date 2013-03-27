# gr28-gr31, fr31, icc3, fcc3 are used as tmps.
# consider them call clobbered by these macros.

	.macro start
	.data
failmsg:
	.ascii "fail\n"
passmsg:
	.ascii "pass\n"
	.text
	.global _start
_start:
	; enable data and insn caches in copy-back mode
	; Also enable all registers
	or_spr_immed	0xc80003c0,hsr0
	and_spr_immed	0xfffff3ff,hsr0

	; turn on psr.nem, psr.cm, psr.ef, psr.em, psr.esr,
	; disable external interrupts
	or_spr_immed	0x69f8,psr

	; If fsr exists, enable all fp_exceptions except inexact
	movsg		psr,gr28
	srli		gr28,28,gr28
	subicc 		gr28,0x2,gr0,icc3 ; is fr400?
	beq 		icc3,0,nofsr0
	or_spr_immed	0x3d000000,fsr0
nofsr0:

	; Set the stack pointer
	sethi.p		0x7,sp
	setlo		0xfffc,sp	; TODO -- what's a good value for this?

	; Set the TBR address
	sethi.p		0xf,gr28
	setlo		0xf000,gr28
	movgs		gr28,tbr	; TODO -- what's a good value for this?

	; Go to user mode -- causes too many problems
	;and_spr_immed	0xfffffffb,psr
	.endm

; Set GR with another GR
	.macro set_gr_gr src targ
	addi	\src,0,\targ
	.endm

; Set GR with immediate value
	.macro set_gr_immed val reg
	.if (\val >= -32768) && (\val <= 23767)
	setlos	\val,\reg
	.else
	setlo.p	%lo(\val),\reg
	sethi	%hi(\val),\reg
	.endif
	.endm

	.macro set_gr_limmed valh vall reg
	sethi.p	\valh,\reg
	setlo	\vall,\reg
	.endm

; Set GR with address value
	.macro set_gr_addr addr reg
	sethi.p	%hi(\addr),\reg
	setlo	%lo(\addr),\reg
	.endm

; Set GR with SPR
	.macro set_gr_spr src targ
	movsg	\src,\targ
	.endm

; Set GR with a value from memory
	.macro set_gr_mem addr reg
	set_gr_addr	\addr,gr28
	ldi		@(gr28,0),\reg
	.endm

; Increment GR with immediate value
	.macro inc_gr_immed val reg
	.if (\val >= -2048) && (\val <= 2047)
	addi \reg,\val,\reg
	.else
	set_gr_immed \val,gr28
	add \reg,gr28,\reg
	.endif
	.endm

; AND GR with immediate value
	.macro and_gr_immed val reg
	.if (\val >= -2048) && (\val <= 2047)
	andi \reg,\val,\reg
	.else
	set_gr_immed \val,gr28
	and \reg,gr28,\reg
	.endif
	.endm

; OR GR with immediate value
	.macro or_gr_immed val reg
	.if (\val >= -2048) && (\val <= 2047)
	ori \reg,\val,\reg
	.else
	set_gr_immed \val,gr28
	or \reg,gr28,\reg
	.endif
	.endm

; Set FR with another FR
	.macro set_fr_fr src targ
	fmovs	\src,\targ
	.endm

; Set FR with integer immediate value
	.macro set_fr_iimmed valh vall reg
	set_gr_limmed	\valh,\vall,gr28
	movgf		gr28,\reg
	.endm

; Set FR with integer immediate value
	.macro set_fr_immed val reg
	set_gr_immed	\val,gr28
	movgf		gr28,\reg
	.endm

; Set FR with a value from memory
	.macro set_fr_mem addr reg
	set_gr_addr	\addr,gr28
	ldfi		@(gr28,0),\reg
	.endm

; Set double FR with another double FR
	.macro set_dfr_dfr src targ
	fmovd	\src,\targ
	.endm

; Set double FR with a value from memory
	.macro set_dfr_mem addr reg
	set_gr_addr	\addr,gr28
	lddfi		@(gr28,0),\reg
	.endm

; Set CPR with immediate value
	.macro set_cpr_immed val reg
	addi		sp,-4,gr28
	set_gr_immed	\val,gr29
	st		gr29,@(gr28,gr0)
	ldc		@(gr28,gr0),\reg
	.endm

	.macro set_cpr_limmed valh vall reg
	addi		sp,-4,gr28
	set_gr_limmed	\valh,\vall,gr29
	st		gr29,@(gr28,gr0)
	ldc		@(gr28,gr0),\reg
	.endm

; Set SPR with immediate value
	.macro set_spr_immed val reg
	set_gr_immed	\val,gr28
	movgs		gr28,\reg
	.endm

	.macro set_spr_limmed valh vall reg
	set_gr_limmed	\valh,\vall,gr28
	movgs		gr28,\reg
	.endm

	.macro set_spr_addr addr reg
	set_gr_addr	\addr,gr28
	movgs		gr28,\reg
	.endm

; increment SPR with immediate value
	.macro inc_spr_immed val reg
	movsg		\reg,gr28
	inc_gr_immed 	\val,gr28
	movgs		gr28,\reg
	.endm

; OR spr with immediate value
	.macro or_spr_immed val reg
	movsg		\reg,gr28
	set_gr_immed 	\val,gr29
	or		gr28,gr29,gr28
	movgs		gr28,\reg
	.endm

; AND spr with immediate value
	.macro and_spr_immed val reg
	movsg		\reg,gr28
	set_gr_immed 	\val,gr29
	and		gr28,gr29,gr28
	movgs		gr28,\reg
	.endm

; Set accumulator with immediate value
	.macro set_acc_immed val reg
	set_fr_immed	\val,fr31
	mwtacc		fr31,\reg
	.endm

; Set accumulator guard with immediate value
	.macro set_accg_immed val reg
	set_fr_immed	\val,fr31
	mwtaccg		fr31,\reg
	.endm

; Set memory with immediate value
	.macro set_mem_immed val base
	set_gr_immed	\val,gr28
	sti		gr28,@(\base,0)
	.endm

	.macro set_mem_limmed valh vall base
	set_gr_limmed	\valh,\vall,gr28
	sti		gr28,@(\base,0)
	.endm

; Set memory with GR value
	.macro set_mem_gr reg addr
	set_gr_addr	\addr,gr28
	sti		\reg,@(gr28,0)
	.endm

; Test the value of a general register against another general register
	.macro test_gr_gr reg1 reg2
	subcc \reg1,\reg2,gr0,icc3
	beq icc3,0,test_gr\@
	fail
test_gr\@:
	.endm

; Test the value of an immediate against a general register
	.macro test_gr_immed val reg
	.if (\val >= -512) && (\val <= 511)
	subicc \reg,\val,gr0,icc3
	.else
	set_gr_immed \val,gr28
	subcc \reg,gr28,gr0,icc3
	.endif
	beq icc3,0,test_gr\@
	fail
test_gr\@:
	.endm

	.macro test_gr_limmed valh vall reg
	set_gr_limmed \valh,\vall,gr28
	subcc \reg,gr28,gr0,icc3
	beq icc3,0,test_gr\@
	fail
test_gr\@:
	.endm

; Test the value of an floating register against an integer immediate
	.macro test_fr_limmed valh vall reg
	movfg		\reg,gr29
	set_gr_limmed	\valh,\vall,gr28
	subcc		gr29,gr28,gr0,icc3
	beq icc3,0,test_gr\@
	fail
test_gr\@:
	.endm

	.macro test_fr_iimmed val reg
	movfg		\reg,gr29
	set_gr_immed	\val,gr28
	subcc		gr29,gr28,gr0,icc3
	beq icc3,0,test_gr\@
	fail
test_gr\@:
	.endm

; Test the value of a floating register against another floating point register
	.macro test_fr_fr reg1 reg2
	fcmps	\reg1,\reg2,fcc3
	fbeq 	fcc3,0,test_gr\@
	fail
test_gr\@:
	.endm

; Test the value of a double floating register against another
; double floating point register
	.macro test_dfr_dfr reg1 reg2
	fcmpd	\reg1,\reg2,fcc3
	fbeq 	fcc3,0,test_gr\@
	fail
test_gr\@:
	.endm

; Test the value of a special purpose register against an integer immediate
	.macro test_spr_immed val reg
	movsg		\reg,gr29
	set_gr_immed	\val,gr28
	subcc		gr29,gr28,gr0,icc3
	beq icc3,0,test_gr\@
	fail
test_gr\@:
	.endm

	.macro test_spr_limmed valh vall reg
	movsg		\reg,gr29
	set_gr_limmed	\valh,\vall,gr28
	subcc		gr29,gr28,gr0,icc3
	beq icc3,0,test_gr\@
	fail
test_gr\@:
	.endm

	.macro test_spr_gr spr gr
	movsg		\spr,gr28
	test_gr_gr	\gr,gr28
	.endm

	.macro test_spr_addr addr reg
	movsg		\reg,gr29
	set_gr_addr	\addr,gr28
	test_gr_gr	gr28,gr29
	.endm

; Test spr bits masked and shifted against the given value
	.macro test_spr_bits	mask,shift,val,reg
	movsg		\reg,gr28
	set_gr_immed 	\mask,gr29
	and		gr28,gr29,gr28
	srli		gr28,\shift,gr29
	test_gr_immed	\val,gr29
	.endm
	

; Test the value of an accumulator against an integer immediate
	.macro test_acc_immed val reg
	mrdacc		\reg,fr31
	test_fr_iimmed	\val,fr31
	.endm

; Test the value of an accumulator against an integer immediate
	.macro test_acc_limmed valh vall reg
	mrdacc		\reg,fr31
	test_fr_limmed	\valh,\vall,fr31
	.endm

; Test the value of an accumulator guard against an integer immediate
	.macro test_accg_immed val reg
	mrdaccg		\reg,fr31
	test_fr_iimmed	\val,fr31
	.endm

; Test CPR agains an immediate value
	.macro test_cpr_limmed valh vall reg
	addi		sp,-4,gr31
	stc		\reg,@(gr31,gr0)
	test_mem_limmed	\valh,\vall,gr31
	.endm

; Test the value of an immediate against memory
	.macro test_mem_immed val base
	ldi		@(\base,0),gr29
	.if (\val >= -512) && (\val <= 511)
	subicc gr29,\val,gr0,icc3
	.else
	set_gr_immed \val,gr28
	subcc gr29,gr28,gr0,icc3
	.endif
	beq icc3,0,test_gr\@
	fail
test_gr\@:
	.endm

	.macro test_mem_limmed valh vall base
	ldi		@(\base,0),gr29
	set_gr_limmed \valh,\vall,gr28
	subcc gr29,gr28,gr0,icc3
	beq icc3,0,test_gr\@
	fail
test_gr\@:
	.endm

; Set an integer condition code
	.macro set_icc mask iccno
	set_gr_immed	4,gr29
	smuli		gr29,\iccno,gr30
	addi		gr31,16,gr31
	set_gr_immed	0xf,gr28
	sll		gr28,gr31,gr28
	not		gr28,gr28
	movsg		ccr,gr29
	and		gr28,gr29,gr29
	set_gr_immed	\mask,gr28
	sll		gr28,gr31,gr28
	or		gr28,gr29,gr29
	movgs		gr29,ccr
	.endm
; started here
; Test the condition codes
	.macro test_icc N Z V C iccno
	.if (\N == 1)
	bp   \iccno,0,fail\@
	.else
	bn   \iccno,0,fail\@
	.endif
	.if (\Z == 1)
	bne  \iccno,0,fail\@
	.else
	beq   \iccno,0,fail\@
	.endif
	.if (\V == 1)
	bnv \iccno,0,fail\@
	.else
	bv   \iccno,0,fail\@
	.endif
	.if (\C == 1)
	bnc   \iccno,0,fail\@
	.else
	bc   \iccno,0,fail\@
	.endif
	bra test_cc\@
fail\@:
	fail
test_cc\@:
	.endm

; Set an floating point condition code
	.macro set_fcc mask fccno
	set_gr_immed	4,gr29
	smuli		gr29,\fccno,gr30
	set_gr_immed	0xf,gr28
	sll		gr28,gr31,gr28
	not		gr28,gr28
	movsg		ccr,gr29
	and		gr28,gr29,gr29
	set_gr_immed	\mask,gr28
	sll		gr28,gr31,gr28
	or		gr28,gr29,gr29
	movgs		gr29,ccr
	.endm

; Test the condition codes
	.macro test_fcc val fccno
	set_gr_immed	4,gr29
	smuli		gr29,\fccno,gr30
	movsg		ccr,gr29
	srl		gr29,gr31,gr29
	andi		gr29,0xf,gr29
	test_gr_immed	\val,gr29
	.endm

; Set PSR.ET
	.macro set_psr_et val
	movsg		psr,gr28
	.if (\val == 1)
	ori		gr28,1,gr28		; Turn on SPR.ET
	.else
	andi		gr28,0xfffffffe,gr28	; Turn off SPR.ET
	.endif
	movgs		gr28,psr
	.endm

; Floating point constants
	.macro float_constants
f0:		.float	0.0
f1:		.float	1.0
f2:		.float	2.0
f3:		.float	3.0
f6:		.float	6.0
f9:		.float	9.0
fn0:		.float	-0.0
fn1:		.float	-1.0
finf:		.long	0x7f800000
fninf:		.long	0xff800000
fmax:		.long	0x7f7fffff
fmin:		.long	0xff7fffff
feps:		.long	0x00400000
fneps:		.long	0x80400000
fnan1:		.long	0x7fc00000
fnan2:		.long	0x7f800001
	.endm

	.macro double_constants
d0:		.double	0.0
d1:		.double	1.0
d2:		.double	2.0
d3:		.double	3.0
d6:		.double	6.0
d9:		.double	9.0
dn0:		.double	-0.0
dn1:		.double	-1.0
dinf:		.long	0x7ff00000
		.long	0x00000000
dninf:		.long	0xfff00000
		.long	0x00000000
dmax:		.long	0x7fefffff
		.long	0xffffffff
dmin:		.long	0xffefffff
		.long	0xffffffff
deps:		.long	0x00080000
		.long	0x00000000
dneps:		.long	0x80080000
		.long	0x00000000
dnan1:		.long	0x7ff80000
		.long	0x00000000
dnan2:		.long	0x7ff00000
		.long	0x00000001
	.endm

; Load floating point constants
	.macro load_float_constants
	set_fr_mem	fninf,fr0
	set_fr_mem	fmin,fr4
	set_fr_mem	fn1,fr8
	set_fr_mem	fneps,fr12
	set_fr_mem	fn0,fr16
	set_fr_mem	f0,fr20
	set_fr_mem	feps,fr24
	set_fr_mem	f1,fr28
	set_fr_mem	f2,fr32
	set_fr_mem	f3,fr36
	set_fr_mem	f6,fr40
	set_fr_mem	f9,fr44
	set_fr_mem	fmax,fr48
	set_fr_mem	finf,fr52
	set_fr_mem	fnan1,fr56
	set_fr_mem	fnan2,fr60
	.endm

	.macro load_float_constants1
	set_fr_mem	fninf,fr1
	set_fr_mem	fmin,fr5
	set_fr_mem	fn1,fr9
	set_fr_mem	fneps,fr13
	set_fr_mem	fn0,fr17
	set_fr_mem	f0,fr21
	set_fr_mem	feps,fr25
	set_fr_mem	f1,fr29
	set_fr_mem	f2,fr33
	set_fr_mem	f3,fr37
	set_fr_mem	f6,fr41
	set_fr_mem	f9,fr45
	set_fr_mem	fmax,fr49
	set_fr_mem	finf,fr53
	set_fr_mem	fnan1,fr57
	set_fr_mem	fnan2,fr61
	.endm

	.macro load_float_constants2
	set_fr_mem	fninf,fr2
	set_fr_mem	fmin,fr6
	set_fr_mem	fn1,fr10
	set_fr_mem	fneps,fr14
	set_fr_mem	fn0,fr18
	set_fr_mem	f0,fr22
	set_fr_mem	feps,fr26
	set_fr_mem	f1,fr30
	set_fr_mem	f2,fr34
	set_fr_mem	f3,fr38
	set_fr_mem	f6,fr42
	set_fr_mem	f9,fr46
	set_fr_mem	fmax,fr50
	set_fr_mem	finf,fr54
	set_fr_mem	fnan1,fr58
	set_fr_mem	fnan2,fr62
	.endm

	.macro load_float_constants3
	set_fr_mem	fninf,fr3
	set_fr_mem	fmin,fr7
	set_fr_mem	fn1,fr11
	set_fr_mem	fneps,fr15
	set_fr_mem	fn0,fr19
	set_fr_mem	f0,fr23
	set_fr_mem	feps,fr27
	set_fr_mem	f1,fr31
	set_fr_mem	f2,fr35
	set_fr_mem	f3,fr39
	set_fr_mem	f6,fr43
	set_fr_mem	f9,fr47
	set_fr_mem	fmax,fr51
	set_fr_mem	finf,fr55
	set_fr_mem	fnan1,fr59
	set_fr_mem	fnan2,fr63
	.endm

	.macro load_double_constants
	set_dfr_mem	dninf,fr0
	set_dfr_mem	dmin,fr4
	set_dfr_mem	dn1,fr8
	set_dfr_mem	dneps,fr12
	set_dfr_mem	dn0,fr16
	set_dfr_mem	d0,fr20
	set_dfr_mem	deps,fr24
	set_dfr_mem	d1,fr28
	set_dfr_mem	d2,fr32
	set_dfr_mem	d3,fr36
	set_dfr_mem	d6,fr40
	set_dfr_mem	d9,fr44
	set_dfr_mem	dmax,fr48
	set_dfr_mem	dinf,fr52
	set_dfr_mem	dnan1,fr56
	set_dfr_mem	dnan2,fr60
	.endm

; Lock the insn cache at the given address
	.macro lock_insn_cache address
	icpl		\address,gr0,1
	.endm

; Lock the data cache at the given address
	.macro lock_data_cache address
	dcpl		\address,gr0,1
	.endm

; Invalidate the data cache at the given address
	.macro invalidate_data_cache address
	dci		@(\address,gr0)
	.endm

; Flush the data cache at the given address
	.macro flush_data_cache address
	dcf		@(\address,gr0)
	.endm

; Write a bctrlr 0,0 insn at the address contained in the given register
	.macro set_bctrlr_0_0 address
	set_mem_immed	0x80382000,\address	; bctrlr 0,0
	flush_data_cache \address
	.endm

; Exit with return code
	.macro exit rc
	setlos		#1,gr7
	set_gr_immed	\rc,gr8
	tira		gr0,#0
	.endm

; Pass the test case
	.macro pass
pass\@:
	setlos.p	#5,gr10
	setlos		#1,gr8
	setlos		#5,gr7
	set_gr_addr  	passmsg,gr9
	tira		gr0,#0
	exit		#0
	.endm

; Fail the testcase
	.macro fail
fail\@:
	setlos.p	#5,gr10
	setlos		#1,gr8
	setlos		#5,gr7
	set_gr_addr	failmsg,gr9
	tira		gr0,#0
	exit		#1
	.endm
