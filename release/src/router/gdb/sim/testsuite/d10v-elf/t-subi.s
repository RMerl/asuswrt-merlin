.include "t-macros.i"

	start

;;  The d10v implements negated addition for subtraction

	.macro check_subi s x y r c v
	;; clear carry
	ldi	r6,#0x8004
	mvtc	r6,cr0
	;; subtract
	ldi	r10,#\x
	SUBI	r10,#\y
	;; verify result
	ldi	r11, #\r
	cmpeq	r10, r11
	brf0t	1f
	ldi	r6, 1
	ldi	r2, \s
	trap	15
1:
	;; verify carry
	mvfc	r6, cr0
	and3	r6, r6, #1
	cmpeqi	r6, #\c
	brf0t	1f
	ldi	r6, 1
	ldi	r2, \s
	trap	15
1:
	.endm

	check_subi 1 0000  0x0000  0xfff0 00 ;;  0 - 0x10
	check_subi 2 0x0000  0x0001  0xffff 0 0
	check_subi 3 0x0001  0x0000  0xfff1 0 0
	check_subi 4 0x0001  0x0001  0x0000 1 0
	check_subi 5 0x8000  0x0001  0x7fff 1 1

	exit0
