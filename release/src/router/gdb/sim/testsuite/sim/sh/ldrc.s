# sh testcase for ldrc, strc
# mach: shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

setrc_imm:
	set_grs_a5a5
	# Test setrc
	#
	ldrs	lstart
	ldre	lend
	setrc	#0xff
	get_sr	r1
	shlr16	r1
	set_greg 0xfff, r0
	and	r0, r1
	assertreg 0xff, r1

	stc	rs, r0	! rs unchanged
	assertreg0	lstart
	stc	re, r0	! re unchanged
	assertreg0	lend

	set_greg 0xa5a5a5a5, r0
	set_greg 0xa5a5a5a5, r1

	test_grs_a5a5

setrc_reg:
	set_grs_a5a5
	# Test setrc
	#
	ldrs	lstart
	ldre	lend
	set_greg	0xfff, r0
	setrc	r0
	get_sr	r1
	shlr16	r1
	set_greg 0xfff, r0
	and	r0, r1
	assertreg 0xfff, r1

	stc	rs, r0	! rs unchanged
	assertreg0	lstart
	stc	re, r0	! re unchanged
	assertreg0	lend

	set_greg 0xa5a5a5a5, r0
	set_greg 0xa5a5a5a5, r1

	test_grs_a5a5

	bra	ldrc_imm

	.global lstart
	.align 2
lstart:	nop
	nop
	nop
	nop
	.global lend
	.align 2
lend:	nop
	nop
	nop
	nop

ldrc_imm:
	set_grs_a5a5
	# Test ldrc
	setrc	#0x0	! zero rc
	ldrc	#0xa5
	get_sr	r1
	shlr16	r1
	set_greg 0xfff, r0
	and	r0, r1
	assertreg 0xa5, r1
	stc	rs, r0	! rs unchanged
	assertreg0	lstart
	stc	re, r0
	assertreg0	lend+1	! bit 0 set in re

	# fix up re for next test
	dt	r0	! Ugh!  No DEC insn!
	ldc	r0, re

	set_greg 0xa5a5a5a5, r0
	set_greg 0xa5a5a5a5, r1

	test_grs_a5a5

ldrc_reg:
	set_grs_a5a5
	# Test ldrc
	setrc	#0x0	! zero rc
	set_greg 0xa5a, r0
	ldrc	r0
	get_sr	r1
	shlr16	r1
	set_greg 0xfff, r0
	and	r0, r1
	assertreg 0xa5a, r1
	stc	rs, r0	! rs unchanged
	assertreg0	lstart
	stc	re, r0
	assertreg0	lend+1	! bit 0 set in re

	set_greg 0xa5a5a5a5, r0
	set_greg 0xa5a5a5a5, r1

	test_grs_a5a5

	pass
	exit 0

