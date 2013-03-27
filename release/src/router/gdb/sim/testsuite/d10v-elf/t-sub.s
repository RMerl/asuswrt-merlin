.include "t-macros.i"

	start

;;  The d10v implements negated addition for subtraction

	.macro check_sub s x y r c
	;; clear carry
	ldi	r6,#0x8004
	mvtc	r6,cr0
	;; subtract
	ldi	r10,#\x
	ldi	r11,#\y
	sub	r10, r11
	;; verify result
	ldi	r12, #\r
	cmpeq	r10, r12
	brf0t	1f
	ldi	r6, 1
	ldi	r2, #\s
	trap	15
1:
	;; verify carry
	mvfc	r6, cr0
	and3	r6, r6, #1
	cmpeqi	r6, #\c
	brf0t	1f
	ldi	r6, 1
	ldi	r2, #\s
	trap	15
1:
	.endm

check_sub 1 0x0000 0x0000  0x0000  1
check_sub 2 0x0000 0x0001  0xffff  0
check_sub 3 0x0001 0x0000  0x0001  1
check_sub 4 0x0001 0x0001  0x0000  1
check_sub 5 0x0000 0x8000  0x8000  0
check_sub 6 0x8000 0x0001  0x7fff  1
check_sub 7 0x7fff 0x7fff  0x0000  1

	exit0
