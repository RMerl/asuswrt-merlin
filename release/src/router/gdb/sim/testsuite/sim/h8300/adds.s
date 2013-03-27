# Hitachi H8 testcase 'adds'
# mach(): h8300h h8300s h8sx
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf	

	.include "testutils.inc"

	# Instructions tested:
	# adds #1, erd		; 0 b 0 xerd
	# adds #2, erd		; 0 b 8 xerd
	# adds #4, erd		; 0 b 9 xerd
	#

	start
.if (sim_cpu)			; 32 bit only
adds_1:	
	set_grs_a5a5
	set_ccr_zero

	adds	#1, er0

	test_cc_clear		; adds should not affect any condition codes
	test_h_gr32  0xa5a5a5a6 er0	; result of adds #1

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

adds_2:
	set_grs_a5a5
	set_ccr_zero

	adds	#2, er0

	test_cc_clear		; adds should not affect any condition codes
	test_h_gr32  0xa5a5a5a7 er0	; result of adds #2

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

adds_4:	
	set_grs_a5a5
	set_ccr_zero

	adds	#4, er0

	test_cc_clear		; adds should not affect any condition codes
	test_h_gr32  0xa5a5a5a9 er0	; result of adds #4

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	pass
.endif	
	exit 0
