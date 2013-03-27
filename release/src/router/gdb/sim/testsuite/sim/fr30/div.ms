# fr30 testcase for division
# mach(): fr30

	.include "testutils.inc"

	START

	.text
	.global div
div:
	; simple division 12 / 3
	mvi_h_gr   	0x00000003,r2
	mvi_h_dr   	0xdeadbeef,mdh
	mvi_h_dr   	0x0000000c,mdl
	div0s      	r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div2            r2
	div3
	div4s
	test_h_gr  	0x00000003,r2
	test_h_dr  	0x00000000,mdh
	test_h_dr  	0x00000004,mdl
	test_dbits	0x0

	; example 1 from div0s the manual
	mvi_h_gr   	0x01234567,r2
	mvi_h_dr   	0xdeadbeef,mdh
	mvi_h_dr   	0xfedcba98,mdl
	div0s      	r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div2            r2
	div3
	div4s
	test_h_gr  	0x01234567,r2
	test_h_dr  	0xffffffff,mdh
	test_h_dr  	0xffffffff,mdl
	test_dbits	0x3

	; example 2 from div0s the manual
	mvi_h_dr   	0xdeadbeef,mdh
	mvi_h_dr   	0xfedcba98,mdl
	mvi_h_gr   	0x1234567,r2
	mvi_h_gr   	1,r0
	mvi_h_gr   	32,r1
	div0s      	r2
loop1:	sub		r0,r1
	bne:d		loop1
	div1		r2
	div2		r2
	div3
	div4s
	test_h_gr  	0x01234567,r2
	test_h_dr  	0xffffffff,mdh
	test_h_dr  	0xffffffff,mdl
	test_dbits	0x3

	; example 1 from div0u in the manual
	mvi_h_gr   	0x01234567,r2
	mvi_h_dr   	0xdeadbeef,mdh
	mvi_h_dr   	0xfedcba98,mdl
	div0u      	r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	div1		r2
	test_h_gr  	0x01234567,r2
	test_h_dr  	0x00000078,mdh
	test_h_dr  	0x000000e0,mdl
	test_dbits	0x0

	; example 2 from div0u in the manual
	mvi_h_dr   	0xdeadbeef,mdh
	mvi_h_dr   	0xfedcba98,mdl
	mvi_h_gr   	0x1234567,r2
	mvi_h_gr   	1,r0
	mvi_h_gr   	32,r1
	div0u      	r2
loop2:	sub		r0,r1
	bne:d		loop2
	div1		r2
	test_h_gr  	0x01234567,r2
	test_h_dr  	0x00000078,mdh
	test_h_dr  	0x000000e0,mdl
	test_dbits	0x0

	pass
