	.text
	.global _main
	.global add_tests
	.global bCC_tests
	.global bCCx_tests
	.global bit_tests
	.global cmp_tests
	.global extend_tests
	.global logical_tests
	.global mov_tests_1
	.global mov_tests_2
	.global mov_tests_3
	.global mov_tests_4
	.global movb_tests
	.global movbu_tests
	.global movx_tests
	.global misc_tests
	.global shift_tests
	.global sub_tests

_main:
	nop

add_tests:
	add d1,d2
	add d2,a3
	add a2,d1
	add a3,a2
	add 16,d1
	add 256,d2
	add 131071,d3
	add 16,a1
	add 256,a2
	add 131071,a3
	addc d1,d2
	addnf 16,a2

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

bCCx_tests:
	beqx bCCx_tests
	bnex bCCx_tests
	bgtx bCCx_tests
	bgex bCCx_tests
	blex bCCx_tests
	bltx bCCx_tests
	bhix bCCx_tests
	bccx bCCx_tests
	blsx bCCx_tests	
	bcsx bCCx_tests
	bvcx bCCx_tests
	bvsx bCCx_tests
	bncx bCCx_tests
	bnsx bCCx_tests

bit_tests:
	btst 64,d1
	btst 8192,d2
	bset d1,(a2)
	bclr d1,(a2)

cmp_tests:
	cmp d1,d2
	cmp d2,a3
	cmp a3,d3
	cmp a3,a2
	cmp 16,d3
	cmp 256,d2
	cmp 131071,d1
	cmp 256,a2
	cmp 131071,a1

extend_tests:
	ext d1
	extx d2
	extxu d3
	extxb d2
	extxbu d1
	
logical_tests:
	and d1,d2
	and 127,d2
	and 32767,d3
	and 32767,psw
	or d1,d2
	or 127,d2
	or 32767,d3
	or 32767,psw
	xor d1,d2
	xor 32767,d3
	not d3

mov_tests_1:
	mov d1,a2
	mov a2,d1
	mov d1,d2
	mov a2,a1
	mov psw,d3
	mov d2,psw
	mov mdr,d1
	mov d2,mdr
	mov (a2),d1
	mov (8,a2),d1
	mov (256,a2),d1
	mov (131071,a2),d1

mov_tests_2:
	mov (d1,a1),d2
	mov (32768),d1
	mov (131071),d1
	mov (8,a2),a1
	mov (256,a2),a1
	mov (131071,a2),a1
	mov (d1,a1),a2
        mov (32768),a1
        mov (131071),a1

mov_tests_3:
        mov d1,(a2)
        mov d1,(32,a2)
	mov d1,(256,a2)
	mov d1,(131071,a2)
	mov d1,(d2,a2)
	mov d1,(128)
	mov d1,(131071)
	mov a1,(32,a2)
	mov a1,(256,a2)
	mov a1,(131071,a2)

mov_tests_4:
	mov a1,(d2,a2)
	mov a1,(128)
	mov a1,(131071)
	mov 8,d1
	mov 256,d1
	mov 131071,d1
	mov 256,a1
	mov 131071,a1

movb_tests:
	movb (8,a2),d1
	movb (256,a2),d1
	movb (131071,a2),d1
	movb (d2,a2),d3
	movb (131071),d2
	movb d1,(a2)
	movb d1,(8,a2)
	movb d1,(256,a2)
	movb d1,(131071,a2)
	movb d1,(d2,a2)
	movb d1,(256)
	movb d1,(131071)

movbu_tests:
	movbu (a2),d1
	movbu (8,a2),d1
	movbu (256,a2),d1
	movbu (131071,a2),d1
	movbu (d1,a1),d2
	movbu (32768),d1
	movbu (131071),d1

movx_tests:
	movx (8,a2),d1
	movx (256,a2),d1
	movx (131071,a2),d1
	movx d1,(8,a2)
	movx d1,(256,a2)
	movx d1,(131071,a2)

muldiv_tests:
	mul d1,d2
	mulu d2,d3
	divu d3,d2

misc_tests:
	jmp _main
	jmp _start
	jmp (a2)
	jsr _main
	jsr _start
	jsr (a2)
	rts
	rti
	nop

shift_tests:
	asr d2
	lsr d3
	ror d1
	rol d2

sub_tests:
	sub d1,d2
	sub d2,a3
	sub a3,d3
	sub a3,a2
	sub 32767,d2
	sub 131071,d2
	sub 32767,a2
	sub 131071,a2
	subc d1,d2
