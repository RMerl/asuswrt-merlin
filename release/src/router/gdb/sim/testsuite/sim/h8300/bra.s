# Hitachi H8 testcase 'bra'
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
.if (sim_cpu == h8sx)	
	.data
	.align 4
disp8:	.long	tgt_reg8 
disp16:	.long	tgt_reg16
disp32:	.long	tgt_reg32
dslot:	.byte	0
	.text
.endif

bra_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  bra dd:8		; 8-bit displacement
	bra tgt_8:8
;;;	.word	0x40xx		; where "xx" is tgt_8 - '.'.
	fail

tgt_8:	
	test_cc_clear
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu)			; not available in h8/300 mode
bra_16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  bra dd:16		; 16-bit displacement
	bra tgt_24:16	; NOTE: hard-coded to avoid relaxing.
;;; 	.word 0x5800
;;; 	.word tgt_24 - .
	fail

tgt_24:	
	test_cc_clear
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

.if	(sim_cpu == h8sx)
bra_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  bra rn.b		; 8-bit register indirect
	sub.l	#src8, @disp8
	mov.l	@disp8, er5
	bra	r5l.b
;;; 	.word	0x5955
src8:	fail
	
tgt_reg8:
	test_cc_clear
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
;;; 	test_h_gr32 tgt_reg8 er5
	test_gr_a5a5 6
	test_gr_a5a5 7

bra_reg16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  bra rn.w		; 16-bit register indirect
	sub.l	#src16, @disp16
	mov.l	@disp16, er5
	bra	r5.w
;;; 	.word	0x5956
src16:	fail
	
tgt_reg16:
	test_cc_clear
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
;;; 	test_h_gr32 tgt_reg16 er5
	test_gr_a5a5 6
	test_gr_a5a5 7

bra_reg32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  bra ern		; 32-bit register indirect
	sub.l	#src32, @disp32
	mov.l	@disp32, er5
	bra	er5.l
;;; 	.word	0x5957
src32:	fail

tgt_reg32:	
	test_cc_clear
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
;;; 	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

bra_s:	set_grs_a5a5
	set_ccr_zero

	bra/s	tgt_post_delay
;;; 	.word	0x4017
	;; The following instruction is in the delay slot, and should execute.
	mov.b	#1, @dslot
	;; After this, the next instructions should not execute.
	fail
	
tgt_post_delay:
	test_cc_clear
	cmp.b	#0, @dslot	; Should be non-zero if delay slot executed.
	bne	dslot_ok
	fail

dslot_ok:
	test_gr_a5a5 0		; Make sure all general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.endif

	pass
	exit 0

	