# Hitachi H8 testcase 'cmp.w'
# mach(): h8300h h8300s h8sx
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
cmp_l_imm3:			; 
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  cmp.l #xx:3,eRd	; Immediate 3-bit operand
	mov.l	#5, er0
	cmp.l	#5, er0
	beq	eq3
	fail
eq3:
	cmp.l	#6, er0
	blt	lt3
	fail
lt3:
	cmp.l	#4, er0
	bgt	gt3
	fail
gt3:	

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0

	test_h_gr32 0x00000005 er0	; er0 unchanged
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

cmp_l_imm16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  cmp.l #xx:8,Rd
	cmp.l	#0xa5a5a5a5, er0	; Immediate 16-bit operand
	beq	eqi
	fail
eqi:	cmp.l	#0xa5a5a5a6, er0
	blt	lti
	fail
lti:	cmp.l	#0xa5a5a5a4, er0
	bgt	gti
	fail
gti:	
	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0

	test_h_gr32 0xa5a5a5a5 er0	; er0 unchanged

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

cmp_w_reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  cmp.l Rs,Rd
	mov.l	#0xa5a5a5a5, er1
	cmp.l	er1, er0		; Register operand
	beq	eqr
	fail
eqr:	mov.l	#0xa5a5a5a6, er1
	cmp.l	er1, er0
	blt	ltr
	fail
ltr:	mov.l	#0xa5a5a5a4, er1
	cmp.l	er1, er0
	bgt	gtr
	fail
gtr:
	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0

	test_h_gr32 0xa5a5a5a5 er0	; r0 unchanged
	test_h_gr32 0xa5a5a5a4 er1	; r1 unchanged

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
	pass

	exit 0
