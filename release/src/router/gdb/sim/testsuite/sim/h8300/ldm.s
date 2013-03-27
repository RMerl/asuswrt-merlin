# Hitachi H8 testcase 'ldm', 'stm'
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
	.align 4
_stack:	.long	0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
_stack_top:

	start

.if (sim_cpu == h8300s || sim_cpu == h8sx)	; Earlier versions, no exr
stm_2reg:
	set_grs_a5a5
	mov	#_stack_top, er7
	mov	#2, er2
	mov	#3, er3
	
	set_ccr_zero
	stm	er2-er3, @-sp
	test_cc_clear
	
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_h_gr32  2	er2
	test_h_gr32  3	er3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_h_gr32  _stack_top-8, er7

	mov	@_stack_top-4, er0
	cmp	#2, er0
	bne	fail1

	mov	@_stack_top-8, er0
	cmp	#3, er0
	bne	fail1

	mov	@_stack_top-12, er0
	cmp	#0, er0
	bne	fail1

stm_3reg:
	set_grs_a5a5
	mov	#_stack_top, er7
	mov	#4, er4
	mov	#5, er5
	mov	#6, er6
	
	set_ccr_zero
	stm	er4-er6, @-sp
	test_cc_clear
	
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_h_gr32  4	er4
	test_h_gr32  5	er5
	test_h_gr32  6	er6
	test_h_gr32  _stack_top-12, er7

	mov	@_stack_top-4, er0
	cmp	#4, er0
	bne	fail1

	mov	@_stack_top-8, er0
	cmp	#5, er0
	bne	fail1

	mov	@_stack_top-12, er0
	cmp	#6, er0
	bne	fail1

	mov	@_stack_top-16, er0
	cmp	#0, er0
	bne	fail1

stm_4reg:
	set_grs_a5a5
	mov	#_stack_top, er7
	mov	#1, er0
	mov	#2, er1
	mov	#3, er2
	mov	#4, er3
	
	set_ccr_zero
	stm	er0-er3, @-sp
	test_cc_clear
	
	test_h_gr32  1	er0
	test_h_gr32  2	er1
	test_h_gr32  3	er2
	test_h_gr32  4	er3
	test_gr_a5a5 4		; Make sure other general regs not disturbed
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_h_gr32  _stack_top-16, er7

	mov	@_stack_top-4, er0
	cmp	#1, er0
	bne	fail1

	mov	@_stack_top-8, er0
	cmp	#2, er0
	bne	fail1

	mov	@_stack_top-12, er0
	cmp	#3, er0
	bne	fail1

	mov	@_stack_top-16, er0
	cmp	#4, er0
	bne	fail1

	mov	@_stack_top-20, er0
	cmp	#0, er0
	bne	fail1

ldm_2reg:
	set_grs_a5a5
	mov	#_stack, er7
	
	set_ccr_zero
	ldm	@sp+, er2-er3
	test_cc_clear
	
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_h_gr32  1	er2
	test_h_gr32  0	er3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_h_gr32  _stack+8, er7

ldm_3reg:
	set_grs_a5a5
	mov	#_stack+4, er7
	
	set_ccr_zero
	ldm	@sp+, er4-er6
	test_cc_clear
	
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_h_gr32  3	er4
	test_h_gr32  2	er5
	test_h_gr32  1	er6
	test_h_gr32  _stack+16, er7

ldm_4reg:
	set_grs_a5a5
	mov	#_stack+4, er7
	
	set_ccr_zero
	ldm	@sp+, er0-er3
	test_cc_clear
	
	test_h_gr32  4	er0
	test_h_gr32  3	er1
	test_h_gr32  2	er2
	test_h_gr32  1	er3
	test_gr_a5a5 4		; Make sure other general regs not disturbed
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_h_gr32  _stack+20, er7
.endif

pushpop:
	set_grs_a5a5
.if (sim_cpu == h8300)
	mov	#_stack_top, r7
	mov	#12, r1
	mov	#34, r2
	mov	#56, r3
	push	r1
	push	r2
	push	r3
	pop	r4
	pop	r5
	pop	r6

	test_gr_a5a5 0		; Make sure other general _reg_ not disturbed
	test_h_gr16  12	r1
	test_h_gr16  34	r2
	test_h_gr16  56	r3
	test_h_gr16  56	r4
	test_h_gr16  34	r5
	test_h_gr16  12	r6
	mov	#_stack_top, r0
	cmp.w	r0, r7
	bne	fail1
.else
	mov	#_stack_top, er7
	mov	#12, er1
	mov	#34, er2
	mov	#56, er3
	push	er1
	push	er2
	push	er3
	pop	er4
	pop	er5
	pop	er6

	test_gr_a5a5 0		; Make sure other general _reg_ not disturbed
	test_h_gr32  12	er1
	test_h_gr32  34	er2
	test_h_gr32  56	er3
	test_h_gr32  56	er4
	test_h_gr32  34	er5
	test_h_gr32  12	er6
	test_h_gr32  _stack_top, er7
.endif
		
	pass

	exit 0

fail1:	fail
