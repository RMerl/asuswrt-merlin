# sh testcase for loop control
# mach:	 shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start
loop1:
	set_grs_a5a5

	ldrs	Loop1_start0+8
	ldre	Loop1_start0+4
	setrc	#5
Loop1_start0:
	add	#1, r1	! Before loop
	# Loop should execute one instruction five times.
Loop1_begin:
	add	#1, r1	! Within loop
Loop1_end:
	add	#2, r1	! After loop

	# r1 = 0xa5a5a5a5 + 8 (five in loop, two after, one before)
	assertreg	0xa5a5a5a5+8, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

loop2:
	set_grs_a5a5

	ldrs	Loop2_start0+6
	ldre	Loop2_start0+4
	setrc	#5
Loop2_start0:	
	add	#1, r1	! Before loop
	# Loop should execute two instructions five times.
Loop2_begin:
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
Loop2_end:
	add	#3, r1	! After loop

	# r1 = 0xa5a5a5a5 + 14 (ten in loop, three after, one before)
	assertreg	0xa5a5a5a5+14, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

loop3:
	set_grs_a5a5

	ldrs	Loop3_start0+4
	ldre	Loop3_start0+4
	setrc	#5
Loop3_start0:
	add	#1, r1	! Before loop
	# Loop should execute three instructions five times.
Loop3_begin:
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
Loop3_end:
	add	#2, r1	! After loop

	# r1 = 0xa5a5a5a5 + 18 (fifteen in loop, two after, one before)
	assertreg	0xa5a5a5a5+18, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

loop4:
	set_grs_a5a5

	ldrs	Loop4_begin
	ldre	Loop4_last3+4
	setrc	#5
	add	#1, r1	! Before loop
	# Loop should execute four instructions five times.
Loop4_begin:
Loop4_last3:
	add	#1, r1	! Within loop
Loop4_last2:
	add	#1, r1	! Within loop
Loop4_last1:
	add	#1, r1	! Within loop
Loop4_last:
	add	#1, r1	! Within loop
Loop4_end:
	add	#2, r1	! After loop

	# r1 = 0xa5a5a5a5 + 23 (20 in loop, two after, one before)
	assertreg	0xa5a5a5a5+23, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

loop5:
	set_grs_a5a5

	ldrs	Loop5_begin
	ldre	Loop5_last3+4
	setrc	#5
	add	#1, r1	! Before loop
	# Loop should execute five instructions five times.
Loop5_begin:
	add	#1, r1	! Within loop
Loop5_last3:
	add	#1, r1	! Within loop
Loop5_last2:
	add	#1, r1	! Within loop
Loop5_last1:
	add	#1, r1	! Within loop
Loop5_last:
	add	#1, r1	! Within loop
Loop5_end:
	add	#2, r1	! After loop

	# r1 = 0xa5a5a5a5 + 28 (25 in loop, two after, one before)
	assertreg	0xa5a5a5a5+28, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

loopn:
	set_grs_a5a5

	ldrs	Loopn_begin
	ldre	Loopn_last3+4
	setrc	#5
	add	#1, r1	! Before loop
	# Loop should execute n instructions five times.
Loopn_begin:
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
Loopn_last3:
	add	#1, r1	! Within loop
Loopn_last2:
	add	#1, r1	! Within loop
Loopn_last1:
	add	#1, r1	! Within loop
Loopn_last:
	add	#1, r1	! Within loop
Loopn_end:
	add	#3, r1	! After loop

	# r1 = 0xa5a5a5a5 + 64 (60 in loop, three after, one before)
	assertreg	0xa5a5a5a5+64, r1

	set_greg 0xa5a5a5a5, r0
	set_greg 0xa5a5a5a5, r1
	test_grs_a5a5

loop1e:
	set_grs_a5a5

	ldrs	Loop1e_begin
	ldre	Loop1e_last
	ldrc	#5
	add	#1, r1	! Before loop
	# Loop should execute one instruction five times.
Loop1e_begin:
Loop1e_last:
	add	#1, r1	! Within loop
Loop1e_end:
	add	#2, r1	! After loop

	# r1 = 0xa5a5a5a5 + 8 (five in loop, two after, one before)
	assertreg	0xa5a5a5a5+8, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

loop2e:
	set_grs_a5a5

	ldrs	Loop2e_begin
	ldre	Loop2e_last
	ldrc	#5
	add	#1, r1	! Before loop
	# Loop should execute two instructions five times.
Loop2e_begin:
	add	#1, r1	! Within loop
Loop2e_last:
	add	#1, r1	! Within loop
Loop2e_end:
	add	#2, r1	! After loop

	# r1 = 0xa5a5a5a5 + 13 (ten in loop, two after, one before)
	assertreg	0xa5a5a5a5+13, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

loop3e:
	set_grs_a5a5

	ldrs	Loop3e_begin
	ldre	Loop3e_last
	ldrc	#5
	add	#1, r1	! Before loop
	# Loop should execute three instructions five times.
Loop3e_begin:
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
Loop3e_last:
	add	#1, r1	! Within loop
Loop3e_end:
	add	#2, r1	! After loop

	# r1 = 0xa5a5a5a5 + 18 (fifteen in loop, two after, one before)
	assertreg	0xa5a5a5a5+18, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

loop4e:
	set_grs_a5a5

	ldrs	Loop4e_begin
	ldre	Loop4e_last
	ldrc	#5
	add	#1, r1	! Before loop
	# Loop should execute four instructions five times.
Loop4e_begin:
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
Loop4e_last:
	add	#1, r1	! Within loop
Loop4e_end:
	add	#2, r1	! After loop

	# r1 = 0xa5a5a5a5 + 23 (twenty in loop, two after, one before)
	assertreg	0xa5a5a5a5+23, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

loop5e:
	set_grs_a5a5

	ldrs	Loop5e_begin
	ldre	Loop5e_last
	ldrc	#5
	add	#1, r1	! Before loop
	# Loop should execute five instructions five times.
Loop5e_begin:
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
Loop5e_last:
	add	#1, r1	! Within loop
Loop5e_end:
	add	#2, r1	! After loop

	# r1 = 0xa5a5a5a5 + 28 (twenty five in loop, two after, one before)
	assertreg	0xa5a5a5a5+28, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

loop_n_e:
	set_grs_a5a5

	ldrs	Loop_n_e_begin
	ldre	Loop_n_e_last
	ldrc	#5
	add	#1, r1	! Before loop
	# Loop should execute n instructions five times.
Loop_n_e_begin:
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
	add	#1, r1	! Within loop
Loop_n_e_last:
	add	#1, r1	! Within loop
Loop_n_e_end:
	add	#2, r1	! After loop

	# r1 = 0xa5a5a5a5 + 48 (forty five in loop, two after, one before)
	assertreg	0xa5a5a5a5+48, r1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5

	pass

	exit 0

