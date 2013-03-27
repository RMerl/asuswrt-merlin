# MDMX .OB op tests.
# mach:	 mips64 sb1
# as:		-mabi=eabi
# as(mips64):	-mabi=eabi -mdmx
# ld:		-N -Ttext=0x80010000
# output:	*\\npass\\n

	.include "testutils.inc"
	.include "utils-mdmx.inc"

	setup

	.set noreorder

	.ent DIAG
DIAG:

	enable_mdmx


	###
	### Non-accumulator, non-CC-using .ob format ops.
	###
	### Key: v = vector
	###      ev = vector of single element
	###      cv = vector of constant.
	###


	writemsg "add.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	add.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x7799bbddffffffff

	writemsg "add.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	add.ob	$f10, $f8, $f9[6]
	ck_ob	$f10, 0x8899aabbccddeeff

	writemsg "add.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	add.ob	$f10, $f8, 0x10
	ck_ob	$f10, 0x2132435465768798


	writemsg "alni.ob"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	alni.ob	$f10, $f8, $f9, 3
	ck_ob	$f10, 0x4455667788667788


	writemsg "alnv.ob"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	li	$4, 5
	alnv.ob	$f10, $f8, $f9, $4
	ck_ob	$f10, 0x66778866778899aa


	writemsg "and.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	and.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x0022000000224488

	writemsg "and.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	and.ob	$f10, $f8, $f9[4]
	ck_ob	$f10, 0x1100110011001188

	writemsg "and.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	and.ob	$f10, $f8, 0x1e
	ck_ob	$f10, 0x1002120414061608


	writemsg "max.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	max.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x66778899aabbccdd

	writemsg "max.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	max.ob	$f10, $f8, $f9[7]
	ck_ob	$f10, 0x6666666666667788

	writemsg "max.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	max.ob	$f10, $f8, 0x15
	ck_ob	$f10, 0x1522334455667788


	writemsg "min.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	min.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x1122334455667788

	writemsg "min.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	min.ob	$f10, $f8, $f9[7]
	ck_ob	$f10, 0x1122334455666666

	writemsg "min.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	min.ob	$f10, $f8, 0x15
	ck_ob	$f10, 0x1115151515151515


	writemsg "mul.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x0001020304050607
	mul.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x002266ccffffffff

	writemsg "mul.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x0001020304050607
	mul.ob	$f10, $f8, $f9[4]
	ck_ob	$f10, 0x336699ccffffffff

	writemsg "mul.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	mul.ob	$f10, $f8, 2
	ck_ob	$f10, 0x22446688aacceeff


	writemsg "nor.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	nor.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x8888442200000022

	writemsg "nor.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	nor.ob	$f10, $f8, $f9[6]
	ck_ob	$f10, 0x8888888888888800

	writemsg "nor.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	nor.ob	$f10, $f8, 0x08
	ck_ob	$f10, 0xe6d5c4b3a2918077


	writemsg "or.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	or.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x7777bbddffffffdd

	writemsg "or.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	or.ob	$f10, $f8, $f9[6]
	ck_ob	$f10, 0x77777777777777ff

	writemsg "or.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	or.ob	$f10, $f8, 0x08
	ck_ob	$f10, 0x192a3b4c5d6e7f88


	writemsg "shfl.mixh.ob"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	shfl.mixh.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x1166227733884499


	writemsg "shfl.mixl.ob"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	shfl.mixl.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x55aa66bb77cc88dd


	writemsg "shfl.pach.ob"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	shfl.pach.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x113355776688aacc


	writemsg "shfl.upsl.ob"
	ld_ob	$f8, 0x1122334455667788
	shfl.upsl.ob	$f10, $f8, $f8
	ck_ob	$f10, 0x005500660077ff88


	writemsg "sll.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x0001020304050607
	sll.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x1144cc2050c0c000

	writemsg "sll.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x0001020304050607
	sll.ob	$f10, $f8, $f9[3]
	ck_ob	$f10, 0x1020304050607080

	writemsg "sll.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	sll.ob	$f10, $f8, 1
	ck_ob	$f10, 0x22446688aaccee10


	writemsg "srl.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x0001020304050607
	srl.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x11110c0805030101

	writemsg "srl.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x0001020304050607
	srl.ob	$f10, $f8, $f9[3]
	ck_ob	$f10, 0x0102030405060708

	writemsg "srl.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	srl.ob	$f10, $f8, 1
	ck_ob	$f10, 0x081119222a333b44


	writemsg "sub.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x0001020304050607
	sub.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x1121314151617181

	writemsg "sub.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	sub.ob	$f10, $f8, $f9[7]
	ck_ob	$f10, 0x0000000000001122

	writemsg "sub.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	sub.ob	$f10, $f8, 0x10
	ck_ob	$f10, 0x0112233445566778


	writemsg "xor.ob (v)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	xor.ob	$f10, $f8, $f9
	ck_ob	$f10, 0x7755bbddffddbb55

	writemsg "xor.ob (ev)"
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	xor.ob	$f10, $f8, $f9[6]
	ck_ob	$f10, 0x66554433221100ff

	writemsg "xor.ob (cv)"
	ld_ob	$f8, 0x1122334455667788
	xor.ob	$f10, $f8, 0x08
	ck_ob	$f10, 0x192a3b4c5d6e7f80


	###
	### Accumulator .ob format ops (in order: rd/wr, math, scale/round)
	###
	### Key: v = vector
	###      ev = vector of single element
	###      cv = vector of constant.
	###


	writemsg "wacl.ob / rac[hml].ob"
	ld_ob	$f8, 0x8001028304850687
	ld_ob	$f9, 0x1011121314151617
	wacl.ob	$f8, $f9
	ck_acc_ob 0xff0000ff00ff00ff, 0x8001028304850687, 0x1011121314151617

	# Note: relies on data left in accumulator by previous test.
	writemsg "wach.ob / rac[hml].ob"
	ld_ob	$f8, 0x2021222324252627
	wach.ob	$f8
	ck_acc_ob 0x2021222324252627, 0x8001028304850687, 0x1011121314151617


	writemsg "adda.ob (v)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	adda.ob	$f8, $f9
	ck_acc_ob 0x0001020304050607, 0x0000000000010101, 0x7799bbddff214365

	writemsg "adda.ob (ev)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	adda.ob	$f8, $f9[2]
	ck_acc_ob 0x0001020304050607, 0x0000000001010101, 0xccddeeff10213243

	writemsg "adda.ob (cv)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	adda.ob	$f8, 0x1f
	ck_acc_ob 0x0001020304050607, 0x0000000000000000, 0x30415263748596a7


	writemsg "addl.ob (v)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	addl.ob	$f8, $f9
	ck_acc_ob 0x0000000000000000, 0x0000000000010101, 0x7799bbddff214365

	writemsg "addl.ob (ev)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	addl.ob	$f8, $f9[2]
	ck_acc_ob 0x0000000000000000, 0x0000000001010101, 0xccddeeff10213243

	writemsg "addl.ob (cv)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	addl.ob	$f8, 0x1f
	ck_acc_ob 0x0000000000000000, 0x0000000000000000, 0x30415263748596a7


	writemsg "mula.ob (v)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	mula.ob	$f8, $f9
	ck_acc_ob 0x0001020304050607, 0x060f1b28384a5e75, 0xc6ce18a47282d468

	writemsg "mula.ob (ev)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	mula.ob	$f8, $f9[2]
	ck_acc_ob 0x0001020304050607, 0x0c1825313e4a5663, 0x6bd641ac1782ed58

	writemsg "mula.ob (cv)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	mula.ob	$f8, 0x1f
	ck_acc_ob 0x0001020304050607, 0x020406080a0c0e10, 0x0f1e2d3c4b5a6978


	writemsg "mull.ob (v)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	mull.ob	$f8, $f9
	ck_acc_ob 0x0000000000000000, 0x060f1b28384a5e75, 0xc6ce18a47282d468

	writemsg "mull.ob (ev)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	mull.ob	$f8, $f9[2]
	ck_acc_ob 0x0000000000000000, 0x0c1825313e4a5663, 0x6bd641ac1782ed58

	writemsg "mull.ob (cv)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	mull.ob	$f8, 0x1f
	ck_acc_ob 0x0000000000000000, 0x020406080a0c0e10, 0x0f1e2d3c4b5a6978


	writemsg "muls.ob (v)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	muls.ob	$f8, $f9
	ck_acc_ob 0xff00010203040506, 0xf9f0e4d7c7b5a18a, 0x3a32e85c8e7e2c98

	writemsg "muls.ob (ev)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	muls.ob	$f8, $f9[2]
	ck_acc_ob 0xff00010203040506, 0xf3e7dacec1b5a99c, 0x952abf54e97e13a8

	writemsg "muls.ob (cv)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	muls.ob	$f8, 0x1f
	ck_acc_ob 0xff00010203040506, 0xfdfbf9f7f5f3f1ef, 0xf1e2d3c4b5a69788


	writemsg "mulsl.ob (v)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	mulsl.ob $f8, $f9
	ck_acc_ob 0xffffffffffffffff, 0xf9f0e4d7c7b5a18a, 0x3a32e85c8e7e2c98

	writemsg "mulsl.ob (ev)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	mulsl.ob $f8, $f9[2]
	ck_acc_ob 0xffffffffffffffff, 0xf3e7dacec1b5a99c, 0x952abf54e97e13a8

	writemsg "mulsl.ob (cv)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	mulsl.ob $f8, 0x1f
	ck_acc_ob 0xffffffffffffffff, 0xfdfbf9f7f5f3f1ef, 0xf1e2d3c4b5a69788


	writemsg "suba.ob (v)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	suba.ob	$f8, $f9
	ck_acc_ob 0xff00010203040506, 0xffffffffffffffff, 0xabababababababab

	writemsg "suba.ob (ev)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	suba.ob	$f8, $f9[2]
	ck_acc_ob 0xff00010203040506, 0xffffffffffffffff, 0x566778899aabbccd

	writemsg "suba.ob (cv)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	suba.ob	$f8, 0x1f
	ck_acc_ob 0xff01020304050607, 0xff00000000000000, 0xf203142536475869


	writemsg "subl.ob (v)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	subl.ob	$f8, $f9
	ck_acc_ob 0xffffffffffffffff, 0xffffffffffffffff, 0xabababababababab

	writemsg "subl.ob (ev)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	ld_ob	$f9, 0x66778899aabbccdd
	subl.ob	$f8, $f9[2]
	ck_acc_ob 0xffffffffffffffff, 0xffffffffffffffff, 0x566778899aabbccd

	writemsg "subl.ob (cv)"
	ld_acc_ob 0x0001020304050607, 0x0000000000000000, 0x0000000000000000
	ld_ob	$f8, 0x1122334455667788
	subl.ob	$f8, 0x1f
	ck_acc_ob 0xff00000000000000, 0xff00000000000000, 0xf203142536475869


	writemsg "rnau.ob (v)"
	ld_acc_ob 0x0000000000000000, 0x0000000003030303, 0x40424446f8fafcfe
	ld_ob	$f8, 0x0001020304050607
	rnau.ob	$f9, $f8
	ck_ob	$f9, 0x4021110940201008

	writemsg "rnau.ob (ev)"
	ld_acc_ob 0x0000000000000000, 0x0000000003030303, 0x40424446f8fafcfe
	ld_ob	$f8, 0x0001020304050607
	rnau.ob	$f9, $f8[4]
	ck_ob	$f9, 0x080809097f7f8080

	writemsg "rnau.ob (cv)"
	ld_acc_ob 0x0000000000000000, 0x0000000003030303, 0x40424446f8fafcfe
	rnau.ob	$f9, 2
	ck_ob	$f9, 0x10111112feffffff


	writemsg "rneu.ob (v)"
	ld_acc_ob 0x0000000000000000, 0x0000000003030303, 0x40424446f8fafcfe
	ld_ob	$f8, 0x0001020304050607
	rneu.ob	$f9, $f8
	ck_ob	$f9, 0x4021110940201008

	writemsg "rneu.ob (ev)"
	ld_acc_ob 0x0000000000000000, 0x0000000003030303, 0x40424446f8fafcfe
	ld_ob	$f8, 0x0001020304050607
	rneu.ob	$f9, $f8[4]
	ck_ob	$f9, 0x080808097f7f8080

	writemsg "rneu.ob (cv)"
	ld_acc_ob 0x0000000000000000, 0x0000000003030303, 0x40424446f8fafcfe
	rneu.ob	$f9, 2
	ck_ob	$f9, 0x10101112fefeffff


	writemsg "rzu.ob (v)"
	ld_acc_ob 0x0000000000000000, 0x0000000003030303, 0x40424446f8fafcfe
	ld_ob	$f8, 0x0001020304050607
	rzu.ob	$f9, $f8
	ck_ob	$f9, 0x402111083f1f0f07

	writemsg "rzu.ob (ev)"
	ld_acc_ob 0x0000000000000000, 0x0000000003030303, 0x40424446f8fafcfe
	ld_ob	$f8, 0x0001020304050607
	rzu.ob	$f9, $f8[4]
	ck_ob	$f9, 0x080808087f7f7f7f

	writemsg "rzu.ob (cv)"
	ld_acc_ob 0x0000000000000000, 0x0000000003030303, 0x40424446f8fafcfe
	rzu.ob	$f9, 2
	ck_ob	$f9, 0x10101111fefeffff


	###
	### CC-using .ob format ops.
	###
	### Key: v = vector
	###      ev = vector of single element
	###      cv = vector of constant.
	###


	writemsg "c.eq.ob (v)"
	ld_ob	$f8, 0x0001010202030304
	ld_ob	$f9, 0x0101020203030404
	clr_fp_cc 0xff
	c.eq.ob	$f8, $f9
	ck_fp_cc 0x55

	writemsg "c.eq.ob (ev)"
	ld_ob	$f8, 0x0001010202030304
	ld_ob	$f9, 0x0101020203030404
	clr_fp_cc 0xff
	c.eq.ob	$f8, $f9[5]
	ck_fp_cc 0x18

	writemsg "c.eq.ob (cv)"
	ld_ob	$f8, 0x0001010202030304
	clr_fp_cc 0xff
	c.eq.ob	$f8, 0x03
	ck_fp_cc 0x06


	writemsg "c.le.ob (v)"
	ld_ob	$f8, 0x0001010202030304
	ld_ob	$f9, 0x0101020203030404
	clr_fp_cc 0xff
	c.le.ob	$f8, $f9
	ck_fp_cc 0xff

	writemsg "c.le.ob (ev)"
	ld_ob	$f8, 0x0001010202030304
	ld_ob	$f9, 0x0101020203030404
	clr_fp_cc 0xff
	c.le.ob	$f8, $f9[5]
	ck_fp_cc 0xf8

	writemsg "c.le.ob (cv)"
	ld_ob	$f8, 0x0001010202030304
	clr_fp_cc 0xff
	c.le.ob	$f8, 0x03
	ck_fp_cc 0xfe


	writemsg "c.lt.ob (v)"
	ld_ob	$f8, 0x0001010202030304
	ld_ob	$f9, 0x0101020203030404
	clr_fp_cc 0xff
	c.lt.ob	$f8, $f9
	ck_fp_cc 0xaa

	writemsg "c.lt.ob (ev)"
	ld_ob	$f8, 0x0001010202030304
	ld_ob	$f9, 0x0101020203030404
	clr_fp_cc 0xff
	c.lt.ob	$f8, $f9[5]
	ck_fp_cc 0xe0

	writemsg "c.lt.ob (cv)"
	ld_ob	$f8, 0x0001010202030304
	clr_fp_cc 0xff
	c.lt.ob	$f8, 0x03
	ck_fp_cc 0xf8


	writemsg "pickf.ob (v)"
	ld_ob	$f8, 0x0001020304050607
	ld_ob	$f9, 0x08090a0b0c0d0e0f
	clrset_fp_cc 0xff, 0xaa
	pickf.ob $f10, $f8, $f9
	ck_ob	$f10, 0x08010a030c050e07

	writemsg "pickf.ob (ev)"
	ld_ob	$f8, 0x0001020304050607
	ld_ob	$f9, 0x08090a0b0c0d0e0f
	clrset_fp_cc 0xff, 0xaa
	pickf.ob $f10, $f8, $f9[4]
	ck_ob	$f10, 0x0b010b030b050b07

	writemsg "pickf.ob (cv)"
	ld_ob	$f8, 0x0001020304050607
	clrset_fp_cc 0xff, 0xaa
	pickf.ob $f10, $f8, 0x10
	ck_ob	$f10, 0x1001100310051007


	writemsg "pickt.ob (v)"
	ld_ob	$f8, 0x0001020304050607
	ld_ob	$f9, 0x08090a0b0c0d0e0f
	clrset_fp_cc 0xff, 0xaa
	pickt.ob $f10, $f8, $f9
	ck_ob	$f10, 0x0009020b040d060f

	writemsg "pickt.ob (ev)"
	ld_ob	$f8, 0x0001020304050607
	ld_ob	$f9, 0x08090a0b0c0d0e0f
	clrset_fp_cc 0xff, 0xaa
	pickt.ob $f10, $f8, $f9[5]
	ck_ob	$f10, 0x000a020a040a060a

	writemsg "pickt.ob (cv)"
	ld_ob	$f8, 0x0001020304050607
	clrset_fp_cc 0xff, 0xaa
	pickt.ob $f10, $f8, 0x10
	ck_ob	$f10, 0x0010021004100610


	pass

	.end DIAG
