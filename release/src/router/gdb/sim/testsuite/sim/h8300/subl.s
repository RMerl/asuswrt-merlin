# Hitachi H8 testcase 'sub.l'
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

.if (sim_cpu == h8sx)		; 
sub_l_imm3:			; 3-bit immediate mode only for h8sx
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  sub.l #xx:3,eRd	; Immediate 3-bit operand
	sub.l	#7:3, er0

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr32 0xa5a5a59e er0	; sub result:	a5a5 - 7
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
sub_l_imm16:			; sub immediate 16-bit value
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  sub.l #xx:16,eRd	; Immediate 16-bit operand
	sub.l	#0x1111:16, er0

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0x9494 r0	; sub result:	a5a5 - 1111
	test_h_gr32 0xa5a59494 er0	; sub result:	a5a5 - 1111
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.endif
	
sub_l_imm32:
	;; sub.l immediate not available in h8300 mode.
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  sub.l #xx:32,Rd
	sub.l	#0x11111111, er0	; Immediate 32-bit operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr32 0x94949494 er0	; sub result:	a5a5a5a5 - 11111111
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
sub.l.reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  add.l Rs,Rd
	mov.l	#0x11111111, er1
	sub.l	er1, er0	; Register operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr32 0x94949494 er0	; sub result:	a5a5a5a5 - 11111111
	test_h_gr32 0x11111111 er1
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
	pass

	exit 0
