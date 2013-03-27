# Hitachi H8 testcase 'inc, inc.w, inc.l'
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

inc_b:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  inc.b Rd
	inc.b	r0h		; Increment 8-bit reg by one

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa6a5 r0	; inc result: a6|a5
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a6a5 er0	; inc result: a5|a5|a6|a5
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu)			; non-zero means h8300h, s, or sx
inc_w_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  inc.w #1, Rd
	inc.w	#1, r0		; Increment 16-bit reg by one

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa5a6 r0	; inc result: a5|a6

	test_h_gr32 0xa5a5a5a6 er0	; inc result:	a5|a5|a5|a6

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

inc_w_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  inc.w #2, Rd
	inc.w	#2, r0		; Increment 16-bit reg by two

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa5a7 r0	; inc result: a5|a7

	test_h_gr32 0xa5a5a5a7 er0	; inc result:	a5|a5|a5|a7

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

inc_l_1:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  inc.l #1, eRd
	inc.l	#1, er0		; Increment 32-bit reg by one

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0

	test_h_gr32 0xa5a5a5a6 er0	; inc result:	a5|a5|a5|a6

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

inc_l_2:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  inc.l #2, eRd
	inc.l	#2, er0		; Increment 32-bit reg by two

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0

	test_h_gr32 0xa5a5a5a7 er0	; inc result:	a5|a5|a5|a7

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
