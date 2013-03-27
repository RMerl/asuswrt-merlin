# mach: crisv0 crisv3 crisv8 crisv10

; Test that a forward as well as backward 32-bit "branch" expansion
; works including that the right offset is applied.

	.macro nop32
	.rept 32
	nop
	.endr
	.endm

	.include "testutils.inc"
	start
	jump start1
	fail

	nop32
	subq 63,$r10
9:	subq 1,$r10
	nop32
	jump 0f

	fail
0:	move [$pc=$pc+1f-6-0b],$p0
	nop32
	fail

	.skip 32768,0

	nop32
	subq 63,$r10
1:
	subq 1,$r10
	nop32
	test.d $r10
	bne 7f
	nop
	pass
7:
	fail

start1:
	moveq 2,$r10
0:	move [$pc=$pc+9b-6-0b],$p0
	subq 63,$r10
	fail
