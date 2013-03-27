# Hitachi H8 testcase 'cmp.w'
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

.if (sim_cpu == h8sx)		; 3-bit immediate mode only for h8sx
cmp_w_imm3:			; 
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  cmp.w #xx:3,Rd	; Immediate 3-bit operand
	mov.w	#5, r0
	cmp.w	#5, r0
	beq	eq3
	fail
eq3:
	cmp.w	#6, r0
	blt	lt3
	fail
lt3:
	cmp.w	#4, r0
	bgt	gt3
	fail
gt3:	

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr32 0xa5a50005 er0	; er0 unchanged
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

.if (sim_cpu)			; non-zero means h8300h, s, or sx
cmp_w_imm16:			; cmp.w immediate not available in h8300 mode.
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  cmp.w #xx:16,Rd
	cmp.w	#0xa5a5, r0	; Immediate 16-bit operand
	beq	eqi
	fail
eqi:	cmp.w	#0xa5a6, r0
	blt	lti
	fail
lti:	cmp.w	#0xa5a4, r0
	bgt	gti
	fail
gti:	
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

cmp_w_imm16_less_than_zero:	; Test for less-than-zero immediate
	set_grs_a5a5
	;; cmp.w #xx:16, Rd, where #xx < 0 (ie. #xx > 0x7fff).
	sub.w	r0, r0
	cmp.w	#0x8001, r0
	bls	ltz
	fail
ltz:	test_gr_a5a5	1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

.endif
	
cmp_w_reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  cmp.w Rs,Rd
	mov.w	#0xa5a5, r1
	cmp.w	r1, r0		; Register operand
	beq	eqr
	fail
eqr:	mov.w	#0xa5a6, r1
	cmp.w	r1, r0
	blt	ltr
	fail
ltr:	mov.w	#0xa5a4, r1
	cmp.w	r1, r0
	bgt	gtr
	fail
gtr:
	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa5a5 r0	; r0 unchanged.
	test_h_gr16 0xa5a4 r1	; r1 unchanged.
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a5a5 er0	; r0 unchanged
	test_h_gr32 0xa5a5a5a4 er1	; r1 unchanged
.endif
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
	pass

	exit 0
