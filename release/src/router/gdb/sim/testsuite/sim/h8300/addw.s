# Hitachi H8 testcase 'add.w'
# mach(): all
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"

	# Instructions tested:
	# add.w xx:3, rd	; 0 a 0xxx rd	(sx only)
	# add.w xx:16, rd	; 7 9 1 rd imm16
	# add.w rs, rd		; 0 9 rs rd
	#

	start
	
.if (sim_cpu == h8sx)		; 3-bit immediate mode only for h8sx
add_w_imm3:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  add.w #xx:3,Rd	; Immediate 3-bit operand
	add.w	#7, r0		; FIXME will not assemble yet
;	.word	0x0a70		; Fake it until assembler will take it.

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa5ac r0	; add result:	a5a5 + 7
	test_h_gr32 0xa5a5a5ac er0	; add result:	a5a5 + 7
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif
	
.if (sim_cpu)			; non-zero means h8300h, s, or sx
add_w_imm16:
	;; add.w immediate not available in h8300 mode.
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  add.w #xx:16,Rd
	add.w	#0x111, r0	; Immediate 16-bit operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa6b6 r0	; add result:	a5a5 + 111
	test_h_gr32 0xa5a5a6b6 er0	; add result:	a5a5 + 111
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif
	
add_w_reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	;;  fixme set ccr

	;;  add.w Rs,Rd
	mov.w	#0x111, r1
	add.w	r1, r0		; Register operand

	;; fixme test ccr	; H=0 N=1 Z=0 V=0 C=0
	test_h_gr16 0xa6b6 r0	; add result:	a5a5 + 111
	test_h_gr16 0x0111 r1
.if (sim_cpu)			; non-zero means h8300h, s, or sx
	test_h_gr32 0xa5a5a6b6 er0	; add result:	a5a5 + 111
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
