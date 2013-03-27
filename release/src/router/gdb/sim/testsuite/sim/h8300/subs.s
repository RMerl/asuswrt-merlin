# Hitachi H8 testcase 'subs'
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
	# subs #1, erd		; 1 b 0 0erd
	# subs #2, erd		; 1 b 8 0erd
	# subs #4, erd		; 1 b 9 0erd
	#

	start
.if (sim_cpu)			; 32 bit only
subs_1:	
	set_grs_a5a5
	set_ccr_zero

	subs	#1, er0

	test_cc_clear		; subs should not affect any condition codes
	test_h_gr32  0xa5a5a5a4 er0	; result of subs #1

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subs_2:
	set_grs_a5a5
	set_ccr_zero

	subs	#2, er0

	test_cc_clear		; subs should not affect any condition codes
	test_h_gr32  0xa5a5a5a3 er0	; result of subs #2

	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

subs_4:	
	set_grs_a5a5
	set_ccr_zero

	subs	#4, er0

	test_cc_clear		; subs should not affect any condition codes
	test_h_gr32  0xa5a5a5a1 er0	; result of subs #4

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
