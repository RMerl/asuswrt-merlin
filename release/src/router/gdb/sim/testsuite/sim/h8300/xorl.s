# Hitachi H8 testcase 'xor.l'
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
	
.if (sim_cpu == h8sx)		; 16-bit immediate is only available on sx.
xor_l_imm16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  xor.l #xx:16,Rd
	xor.l	#0xffff:16, er0	; Immediate 16-bit operand 

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0

	test_h_gr32 0xa5a55a5a er0	; xor result:	 a5a5a5a5 | ffff

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

xor_l_imm32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  xor.l #xx:32,Rd
	xor.l	#0xffffffff, er0	; Immediate 32-bit operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0

	test_h_gr32 0x5a5a5a5a er0	; xor result:	 a5a5a5a5 ^ ffffffff

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

xor_l_reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  xor.l Rs,Rd
	mov.l	#0xffffffff, er1
	xor.l	er1, er0	; Register operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0

	test_h_gr32 0x5a5a5a5a er0	; xor result:	a5a5a5a5 ^ ffffffff
	test_h_gr32 0xffffffff er1	; Make sure er1 is unchanged

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
	pass

	exit 0
