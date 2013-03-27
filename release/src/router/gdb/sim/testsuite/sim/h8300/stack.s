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

	start

.data
	.align 4
stack:	
.if (sim_cpu == h8300)
	.fill	128, 2, 0
.else
	.fill	128, 4, 0
.endif
stacktop:

	.text

push_w:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero
.if (sim_cpu == h8300)
	mov.w	#stacktop, r7
.else
	mov.l	#stacktop, er7
.endif
	push.w	r0		; a5a5 is negative
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	push.w	r1
	push.w	r2
	push.w	r3

	test_gr_a5a5 0
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	
	mov	@stacktop-2, r0
	test_gr_a5a5 0
	mov	@stacktop-4, r0
	test_gr_a5a5 0
	mov	@stacktop-6, r0
	test_gr_a5a5 0
	mov	@stacktop-8, r0
	test_gr_a5a5 0

	mov.w	#1, r1
	mov.w	#2, r2
	mov.w	#3, r3
	mov.w	#4, r4

	push.w	r1		; #1 is non-negative, non-zero
	test_cc_clear
	
	push.w	r2
	push.w	r3
	push.w	r4

	test_h_gr16 1 r1
	test_h_gr16 2 r2
	test_h_gr16 3 r3
	test_h_gr16 4 r4

	mov	@stacktop-10, r0
	test_h_gr16 1 r0
	mov	@stacktop-12, r0
	test_h_gr16 2 r0
	mov	@stacktop-14, r0
	test_h_gr16 3 r0
	mov	@stacktop-16, r0
	test_h_gr16 4 r0

.if (sim_cpu == h8300)
	test_h_gr16	4 r0
	test_h_gr16	1 r1
	test_h_gr16	2 r2
	test_h_gr16	3 r3
	test_h_gr16	4 r4
;;; 	test_h_gr16	stacktop-16 r7	; FIXME
.else
	test_h_gr32	0xa5a50004  er0
	test_h_gr32	0xa5a50001  er1
	test_h_gr32	0xa5a50002  er2
	test_h_gr32	0xa5a50003  er3
	test_h_gr32	0xa5a50004  er4
	test_h_gr32	stacktop-16 er7
.endif
	test_gr_a5a5	5
	test_gr_a5a5	6

pop_w:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero
.if (sim_cpu == h8300)
	mov.w	#stacktop-16, r7
.else
	mov.l	#stacktop-16, er7
.endif
	pop.w	r4
	pop.w	r3
	pop.w	r2
	pop.w	r1		; Should set all flags zero
	test_cc_clear

	test_h_gr16	1 r1
	test_h_gr16	2 r2
	test_h_gr16	3 r3
	test_h_gr16	4 r4

	pop.w	r4
	pop.w	r3
	pop.w	r2
	pop.w	r1		; a5a5 is negative
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
.if (sim_cpu == h8300)
;;; 	test_h_gr16	stacktop r7	; FIXME
.else
	test_h_gr32	stacktop er7
.endif

.if (sim_cpu)			; non-zero means not h8300
push_l:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero
	mov.l	#stacktop, er7
	push.l	er0		; a5a5 is negative
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	push.l	er1
	push.l	er2
	push.l	er3

	test_gr_a5a5 0
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	
	mov	@stacktop-4, er0
	test_gr_a5a5 0
	mov	@stacktop-8, er0
	test_gr_a5a5 0
	mov	@stacktop-12, er0
	test_gr_a5a5 0
	mov	@stacktop-16, er0
	test_gr_a5a5 0

	mov	#1, er1
	mov	#2, er2
	mov	#3, er3
	mov	#4, er4

	push.l	er1		; #1 is non-negative, non-zero
	test_cc_clear
	
	push.l	er2
	push.l	er3
	push.l	er4

	test_h_gr32 1 er1
	test_h_gr32 2 er2
	test_h_gr32 3 er3
	test_h_gr32 4 er4

	mov	@stacktop-20, er0
	test_h_gr32 1 er0
	mov	@stacktop-24, er0
	test_h_gr32 2 er0
	mov	@stacktop-28, er0
	test_h_gr32 3 er0
	mov	@stacktop-32, er0
	test_h_gr32 4 er0

	test_h_gr32	4  er0
	test_h_gr32	1  er1
	test_h_gr32	2  er2
	test_h_gr32	3  er3
	test_h_gr32	4  er4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_h_gr32	stacktop-32 er7

pop_l:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero
	mov.l	#stacktop-32, er7
	pop.l	er4
	pop.l	er3
	pop.l	er2
	pop.l	er1		; Should set all flags zero
	test_cc_clear

	test_h_gr32	1 er1
	test_h_gr32	2 er2
	test_h_gr32	3 er3
	test_h_gr32	4 er4

	pop.l	er4
	pop.l	er3
	pop.l	er2
	pop.l	er1		; a5a5 is negative
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_h_gr32	stacktop er7
.endif

	;; Jump over subroutine
	jmp	_bsr

bsr_jsr_func:	
	test_ccr 0		; call should not affect ccr
	mov.w	#0, r0
	mov.w	#1, r1
	mov.w	#2, r2
	mov.w	#3, r3
	mov.w	#4, r4
	mov.w	#5, r5
	mov.w	#6, r6
	rts

_bsr:	set_grs_a5a5
.if (sim_cpu == h8300)
	mov.w	#stacktop, r7
.else
	mov.l	#stacktop, er7
.endif
	set_ccr_zero
	bsr	bsr_jsr_func

	test_h_gr16	0 r0
	test_h_gr16	1 r1
	test_h_gr16	2 r2
	test_h_gr16	3 r3
	test_h_gr16	4 r4
	test_h_gr16	5 r5
	test_h_gr16	6 r6
.if (sim_cpu == h8300)
;;; 	test_h_gr16	stacktop, r7	; FIXME
.else
	test_h_gr32	stacktop, er7
.endif

_jsr:	set_grs_a5a5
.if (sim_cpu == h8300)
	mov.w	#stacktop, r7
.else
	mov.l	#stacktop, er7
.endif
	set_ccr_zero
	jsr	bsr_jsr_func

	test_h_gr16	0 r0
	test_h_gr16	1 r1
	test_h_gr16	2 r2
	test_h_gr16	3 r3
	test_h_gr16	4 r4
	test_h_gr16	5 r5
	test_h_gr16	6 r6
.if (sim_cpu == h8300)
;;; 	test_h_gr16	stacktop, r7	; FIXME
.else
	test_h_gr32	stacktop, er7
.endif

.if (sim_cpu)			; not zero ie. not h8300
_trapa:
	set_grs_a5a5
	mov.l	#trap_handler, er7	; trap vector
	mov.l	er7, @0x2c
	mov.l	#stacktop, er7
	set_ccr_zero
	trapa	#3

	test_cc_clear		; ccr should be restored by rte
	test_h_gr16	0x10 r0
	test_h_gr16	0x11 r1
	test_h_gr16	0x12 r2
	test_h_gr16	0x13 r3
	test_h_gr16	0x14 r4
	test_h_gr16	0x15 r5
	test_h_gr16	0x16 r6
	test_h_gr32	stacktop er7
.endif

.if (sim_cpu == h8sx)
_rtsl:				; Test rts/l insn.
	set_grs_a5a5
	mov	#0,r0l
	mov	#1,r1l
	mov	#2,r2l
	mov	#3,r3l
	mov	#4,r4l
	mov	#5,r5l
	mov	#6,r6l
	mov	#stacktop, er7

	jsr	rtsl1_func
	test_h_gr32	0xa5a5a500 er0
	test_h_gr32	0xa5a5a501 er1
	test_h_gr32	0xa5a5a502 er2
	test_h_gr32	0xa5a5a503 er3
	test_h_gr32	0xa5a5a504 er4
	test_h_gr32	0xa5a5a505 er5
	test_h_gr32	0xa5a5a506 er6
	test_h_gr32	stacktop   er7

	jsr	rtsl2_func
	test_h_gr32	0xa5a5a500 er0
	test_h_gr32	0xa5a5a501 er1
	test_h_gr32	0xa5a5a502 er2
	test_h_gr32	0xa5a5a503 er3
	test_h_gr32	0xa5a5a504 er4
	test_h_gr32	0xa5a5a505 er5
	test_h_gr32	0xa5a5a506 er6
	test_h_gr32	stacktop   er7

	jsr	rtsl3_func
	test_h_gr32	0xa5a5a500 er0
	test_h_gr32	0xa5a5a501 er1
	test_h_gr32	0xa5a5a502 er2
	test_h_gr32	0xa5a5a503 er3
	test_h_gr32	0xa5a5a504 er4
	test_h_gr32	0xa5a5a505 er5
	test_h_gr32	0xa5a5a506 er6
	test_h_gr32	stacktop   er7

	jsr	rtsl4_func
	test_h_gr32	0xa5a5a500 er0
	test_h_gr32	0xa5a5a501 er1
	test_h_gr32	0xa5a5a502 er2
	test_h_gr32	0xa5a5a503 er3
	test_h_gr32	0xa5a5a504 er4
	test_h_gr32	0xa5a5a505 er5
	test_h_gr32	0xa5a5a506 er6
	test_h_gr32	stacktop   er7
.endif				; h8sx

	pass

	exit 0

	;; Handler for a software exception (trap).
trap_handler:
	;; Test the 'i' interrupt mask flag.
	stc	ccr, r0l
	test_h_gr8	0x80, r0l
	;; Change the registers (so we know we've been here)
	mov.w	#0x10, r0
	mov.w	#0x11, r1
	mov.w	#0x12, r2
	mov.w	#0x13, r3
	mov.w	#0x14, r4
	mov.w	#0x15, r5
	mov.w	#0x16, r6
	;; Change the ccr (which will be restored by RTE)
	orc	#0xff, ccr
	rte

.if (sim_cpu == h8sx)
	;; Functions for testing rts/l
rtsl1_func:			; Save and restore R0
	push.l	er0
	;; Now modify it, and verify the modification.
	mov	#0xfeedface, er0
	test_h_gr32 0xfeedface, er0
	;; Then use rts/l to restore them and return.
	rts/l	er0

rtsl2_func:			; Save and restore R5 and R6
	push.l	er5
	push.l	er6
	;; Now modify them, and verify the modification.
	mov	#0xdeadbeef, er5
	mov	#0xfeedface, er6
	test_h_gr32 0xdeadbeef, er5
	test_h_gr32 0xfeedface, er6
	;; Then use rts/l to restore them and return.
	rts/l	(er5-er6)

rtsl3_func:			; Save and restore R4, R5, and R6
	push.l	er4
	push.l	er5
	push.l	er6
	;; Now modify them, and verify the modification.
	mov	#0xdeafcafe, er4
	mov	#0xdeadbeef, er5
	mov	#0xfeedface, er6
	test_h_gr32 0xdeafcafe, er4
	test_h_gr32 0xdeadbeef, er5
	test_h_gr32 0xfeedface, er6
	;; Then use rts/l to restore them and return.
	rts/l	(er4-er6)

rtsl4_func:			; Save and restore R0 - R3
	push.l	er0
	push.l	er1
	push.l	er2
	push.l	er3
	;; Now modify them, and verify the modification.
	mov	#0xdadacafe, er0
	mov	#0xfeedbeef, er1
	mov	#0xdeadface, er2
	mov	#0xf00dd00d, er3
	test_h_gr32 0xdadacafe, er0
	test_h_gr32 0xfeedbeef, er1
	test_h_gr32 0xdeadface, er2
	test_h_gr32 0xf00dd00d, er3
	;; Then use rts/l to restore them and return.
	rts/l	(er0-er3)
.endif				; h8sx
