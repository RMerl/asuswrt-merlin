# Hitachi H8 testcase 'sub.w'
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
sub_w_imm3:			; sub.w immediate not available in h8300 mode. 
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  sub.w #xx:3,Rd	; Immediate 3-bit operand
	sub.w	#7:3, r0

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa59e r0	; sub result:	a5a5 - 7
	test_h_gr32 0xa5a5a59e er0	; sub result:	a5a5 - 7
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif
	
.if (sim_cpu)			; non-zero means h8300h, s, or sx
sub_w_imm16:			; sub.w immediate not available in h8300 mode. 
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  sub.w #xx:16,Rd
	sub.w	#0x111, r0	; Immediate 16-bit operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa494 r0	; sub result:	a5a5 - 111
	test_h_gr32 0xa5a5a494 er0	; sub result:	a5a5 - 111
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif
	
sub.w.reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  sub.w Rs,Rd
	mov.w	#0x111, r1
	sub.w	r1, r0		; Register operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa494 r0	; sub result:	a5a5 - 111
	test_h_gr16 0x0111 r1
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a494 er0	; sub result:	a5a5 - 111
	test_h_gr32 0xa5a50111 er1
.endif
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
	pass

	exit 0
