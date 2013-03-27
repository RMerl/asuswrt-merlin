	.text
	.global _main
	.global add_tests
	.global bCC_tests
	.global bit_tests
	.global cmp_tests
	.global extend_tests
	.global extended_tests
	.global logical_tests
	.global loop_tests
	.global mov_tests_1
	.global mov_tests_2
	.global mov_tests_3
	.global mov_tests_4
	.global movbu_tests
	.global movhu_tests
	.global movm_tests
	.global muldiv_tests
	.global other_tests
	.global shift_tests
	.global sub_tests

_main:
	nop

add_tests:
	add d1,d2
	add d2,a3
	add a3,a2
	add a2,d1
	add 16,d1
	add 256,d2
	add 131071,d3
	add 16,a1
	add 256,a2
	add 131071,a3
	add 16,sp
	add 256,sp
	add 131071,sp
	addc d1,d2

bCC_tests:
	beq bCC_tests
	bne bCC_tests
	bgt bCC_tests
	bge bCC_tests
	ble bCC_tests
	blt bCC_tests
	bhi bCC_tests
	bcc bCC_tests
	bls bCC_tests	
	bcs bCC_tests
	bvc bCC_tests
	bvs bCC_tests
	bnc bCC_tests
	bns bCC_tests
	bra bCC_tests

bit_tests:
	btst 64,d1
	btst 8192,d2
	btst 131071,d3
	btst 64,(8,a1)
	btst 64,(0x1ffff)
	bset d1,(a2)
	bset 64,(8,a1)
	bset 64,(0x1ffff)
	bclr d1,(a2)
	bclr 64,(8,a1)
	bclr 64,(0x1ffff)

cmp_tests:
	cmp d1,d2
	cmp d2,a3
	cmp a3,d3
	cmp a3,a2
	cmp 16,d3
	cmp 256,d2
	cmp 131071,d1
	cmp 16,a3
	cmp 256,a2
	cmp 131071,a1

	
extend_tests:
	ext d1
	extb d2
	extbu d3
	exth d2
	exthu d1

extended_tests:
	putx d1
	getx d2
	mulq d1,d2
	mulq 16,d2
	mulq 256,d3
	mulq 131071,d3
	mulqu d1,d2
	mulqu 16,d2
	mulqu 256,d3
	mulqu 131071,d3
	sat16 d2,d3
	sat24 d3,d2
	bsch d1,d2
	
logical_tests:
	and d1,d2
	and 127,d2
	and 32767,d3
	and 131071,d3
	and 32767,psw
	or d1,d2
	or 127,d2
	or 32767,d3
	or 131071,d3
	or 32767,psw
	xor d1,d2
	xor 32767,d3
	xor 131071,d3
	not d3

loop_tests:
	leq
	lne
	lgt
	lge
	lle
	llt
	lhi
	lcc
	lls	
	lcs
	lra
	setlb

mov_tests_1:
	mov d1,d2
	mov d1,a2
	mov a2,d1
	mov a2,a1
	mov sp,a2
	mov a1,sp
	mov d2,psw
	mov mdr,d1
	mov d2,mdr
	mov (a2),d1
	mov (8,a2),d1
	mov (256,a2),d1
	mov (131071,a2),d1
	mov (8,sp),d1
	mov (256,sp),d1
	mov psw,d3

mov_tests_2:
        mov (131071,sp),d1
	mov (d1,a1),d2
	mov (32768),d1
	mov (131071),d1
	mov (a2),a1
	mov (8,a2),a1
	mov (256,a2),a1
	mov (131071,a2),a1
	mov (8,sp),a1
	mov (256,sp),a1
	mov (131071,sp),a1
	mov (d1,a1),a2
        mov (32768),a1
        mov (131071),a1
        mov (32,a1),sp

mov_tests_3:
        mov d1,(a2)
        mov d1,(32,a2)
	mov d1,(256,a2)
	mov d1,(131071,a2)
	mov d1,(32,sp)
	mov d1,(32768,sp)
	mov d1,(131071,sp)
	mov d1,(d2,a2)
	mov d1,(128)
	mov d1,(131071)
	mov a1,(a2)
	mov a1,(32,a2)
	mov a1,(256,a2)
	mov a1,(131071,a2)
	mov a1,(32,sp)

mov_tests_4:
	mov a1,(32768,sp)
	mov a1,(131071,sp)
	mov a1,(d2,a2)
	mov a1,(128)
	mov a1,(131071)
	mov sp,(32,a1)
	mov 8,d1
	mov 256,d1
	mov 131071,d1
	mov 8,a1
	mov 256,a1
	mov 131071,a1

movbu_tests:
	movbu (a2),d1
	movbu (8,a2),d1
	movbu (256,a2),d1
	movbu (131071,a2),d1
	movbu (8,sp),d1
	movbu (256,sp),d1
	movbu (131071,sp),d1
	movbu (d1,a1),d2
	movbu (32768),d1
	movbu (131071),d1
	movbu d1,(a2)
	movbu d1,(32,a2)
	movbu d1,(256,a2)
	movbu d1,(131071,a2)
	movbu d1,(32,sp)
	movbu d1,(32768,sp)
	movbu d1,(131071,sp)
	movbu d1,(d2,a2)
	movbu d1,(128)
	movbu d1,(131071)

movhu_tests:
	movhu (a2),d1
	movhu (8,a2),d1
	movhu (256,a2),d1
	movhu (131071,a2),d1
	movhu (8,sp),d1
	movhu (256,sp),d1
	movhu (131071,sp),d1
	movhu (d1,a1),d2
	movhu (32768),d1
	movhu (131071),d1
	movhu d1,(a2)
	movhu d1,(32,a2)
	movhu d1,(256,a2)
	movhu d1,(131071,a2)
	movhu d1,(32,sp)
	movhu d1,(32768,sp)
	movhu d1,(131071,sp)
	movhu d1,(d2,a2)
	movhu d1,(128)
	movhu d1,(131071)

movm_tests:
	movm (sp),[a2,a3]
	movm (sp),[d2,d3,a2,a3,other]
	movm [a2,a3],(sp)
	movm [d2,d3,a2,a3,other],(sp)


muldiv_tests:
	mul d1,d2
	mulu d2,d3
	div d3,d3
	divu d3,d2

other_tests:
	clr d2
	inc d1
	inc a2
	inc4 a3
	jmp (a2)
	jmp _main
	jmp _start
	call _main,[a2,a3],9
	call _start,[a2,a3],32
	calls (a2)
	calls _main
	calls _start
	ret [a2,a3],7
	retf [a2,a3],5
	rets
	rti
	trap
	nop
	rtm

shift_tests:
	asr d1,d2
	asr 4,d2
	lsr d2,d3
	lsr 4,d3
	asl d3,d2
	asl 4,d2
	asl2 d2
	ror d1
	rol d2

sub_tests:
	sub d1,d2
	sub d2,a3
	sub a3,d3
	sub a3,a2
	sub 131071,d2
	sub 131071,a1
	subc d1,d2

