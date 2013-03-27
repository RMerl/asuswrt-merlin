# MDMX .OB op tests.
# mach:	 sb1
# as:		-mabi=eabi
# ld:		-N -Ttext=0x80010000
# output:	*\\npass\\n

	.include "testutils.inc"
	.include "utils-mdmx.inc"

	setup

	.set noreorder

	.ent DIAG
DIAG:

	enable_mdmx

	# set Status.SBX to enable SB-1 extensions.
	mfc0	$2, $12
	or	$2, $2, (1 << 16)
	mtc0	$2, $12


	###
	### SB-1 Non-accumulator .ob format ops.
	###
	### Key: v = vector
	###      ev = vector of single element
	###      cv = vector of constant.
	###


	writemsg "pavg.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	pavg.ob	$f10, $f8, $f9
	ck_ob $f10, 0x3c4d5e6f8091a2b3

	writemsg "pavg.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	pavg.ob	$f10, $f8, $f9[6]
	ck_ob $f10, 0x444d555e666f7780

	writemsg "pavg.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	pavg.ob	$f10, $f8, 0x10
	ck_ob $f10, 0x1119222a333b444c


	writemsg "pabsdiff.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	pabsdiff.ob	$f10, $f8, $f9
	ck_ob $f10, 0x5555555555555555

	writemsg "pabsdiff.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	pabsdiff.ob	$f10, $f8, $f9[7]
	ck_ob $f10, 0x5544332211001122

	writemsg "pabsdiff.ob (cv)"
	ld_ob	$f8, 0x0001020304050607
	pabsdiff.ob	$f10, $f8, 0x04
	ck_ob $f10, 0x0403020100010203


	###
	### SB-1 Accumulator .ob format ops
	###
	### Key: v = vector
	###      ev = vector of single element
	###      cv = vector of constant.
	###


	writemsg "pabsdiffc.ob (v)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	pabsdiffc.ob $f8, $f9
	ck_acc_ob 0x0001020304050607, 0x0000000000000000, 0x5555555555555555

	writemsg "pabsdiffc.ob (ev)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	pabsdiffc.ob $f8, $f9[7]
	ck_acc_ob 0x0001020304050607, 0x0000000000000000, 0x5544332211001122

	writemsg "pabsdiffc.ob (cv)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x0001020304050607
	pabsdiffc.ob $f8, 0x04
	ck_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0403020100010203


	pass

	.end DIAG
