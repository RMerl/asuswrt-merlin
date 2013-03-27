# Hitachi H8 testcase 'dec.b, dec.w, dec.l'
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

dec_b:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  dec.b Rd
	dec.b	r0h		; Decrement 8-bit reg by one

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa4a5 r0	; dec result: a4|a5
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a4a5 er0	; dec result: a5|a5|a4|a5
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu)			; non-zero means h8300h, s, or sx
dec_w_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  dec.w #1, Rd
	dec.w	#1, r0		; Decrement 16-bit reg by one

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa5a4 r0	; dec result: a5|a4

	test_h_gr32 0xa5a5a5a4 er0	; dec result:	a5|a5|a5|a4

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

dec_w_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  dec.w #2, Rd
	dec.w	#2, r0		; Decrement 16-bit reg by two

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa5a3 r0	; dec result: a5|a3

	test_h_gr32 0xa5a5a5a3 er0	; dec result:	a5|a5|a5|a3

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

dec_l_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  dec.l #1, eRd
	dec.l	#1, er0		; Decrement 32-bit reg by one

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0

	test_h_gr32 0xa5a5a5a4 er0	; dec result:	a5|a5|a5|a4

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

dec_l_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  dec.l #2, eRd
	dec.l	#2, er0		; Decrement 32-bit reg by two

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0

	test_h_gr32 0xa5a5a5a3 er0	; dec result:	a5|a5|a5|a3

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

	pass

	exit 0
