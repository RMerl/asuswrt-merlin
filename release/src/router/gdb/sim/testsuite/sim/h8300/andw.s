# Hitachi H8 testcase 'and.w'
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
	
.if (sim_cpu)			; non-zero means h8300h, s, or sx
and_w_imm16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  and.w #xx:16,Rd
	and.w	#0xaaaa, r0	; Immediate 16-bit operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa0a0 r0	; and result:	a5a5 & aaaa
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a0a0 er0	; and result:	 a5a5 & aaaa
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

and_w_reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  and.w Rs,Rd
	mov.w	#0xaaaa, r1
	and.w	r1, r0		; Register operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa0a0 r0	; and result:	a5a5 & aaaa
	test_h_gr16 0xaaaa r1	; Make sure r1 is unchanged
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a0a0 er0	; and result:	a5a5 & aaaa
	test_h_gr32 0xa5a5aaaa er1	; Make sure er1 is unchanged
.endif
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
	pass

	exit 0
