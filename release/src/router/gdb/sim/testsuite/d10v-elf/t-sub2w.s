.include "t-macros.i"

	start

;;  The d10v implements negated addition for subtraction

	.macro check_sub2w s x y r c v
	
	;; clear carry
	ldi	r6,#0x8004
	mvtc	r6,cr0
	
	;; load opnds
	ld2w	r6, @(1f,r0)
	ld2w	r8, @(2f,r0)
	.data
1:	.long	\x
2:	.long	\y
	.text
	
	;; subtract
	SUB2W	r6, r8
	
	;; verify result
	ld2w	r10, @(1f,r0)
	.data
1:	.long	\r
	.text
	cmpeq	r6, r10
	brf0f	2f
	cmpeq	r7, r11
	brf0t	3f
2:	ldi	r4, 1
	ldi	r0, \s
	trap	15
3:
	
	;; verify carry
	mvfc	r6, cr0
	and3	r6, r6, #1
	cmpeqi	r6, #\c
	brf0t	1f
	ldi	r4, 1
	ldi	r0, \s
	trap	15
1:
	.endm
	
check_sub2w 1 0x00000000 0x00000000  0x00000000 1 0
check_sub2w 2 0x00000000 0x00000001  0xffffffff 0 0
check_sub2w 3 0x00000001 0x00000000  0x00000001 1 0
check_sub2w 3 0x00000001 0x00000001  0x00000000 1 0
check_sub2w 5 0x00000000 0x80000000  0x80000000 0 1
check_sub2w 6 0x80000000 0x00000001  0x7fffffff 1 1
check_sub2w 7 0x7fffffff 0x7fffffff  0x00000000 1 0

	exit0
