# r0-r3 are used as tmps, consider them call clobbered by these macros.

	.macro start
	.data
failmsg:
	.ascii "fail\n"
passmsg:
	.ascii "pass\n"
	.text
	.global _start
_start:
	.endm

	.macro exit rc
	ldi8 r1, \rc
	ldi8 r0, #1
	trap #0
	.endm

	.macro pass
	ldi8 r3, 5
	ld24 r2, passmsg
	ldi8 r1, 1
	ldi8 r0, 5
	trap #0
	exit 0
	.endm

	.macro fail
	ldi8 r3, 5
	ld24 r2, failmsg
	ldi8 r1, 1
	ldi8 r0, 5
	trap #0
	exit 1
	.endm

	.macro mvi_h_gr reg, val
	.if (\val >= -128) && (\val <= 127)
	ldi8 \reg, \val
	.else
	seth \reg, high(\val)
	or3 \reg, \reg, low(\val)
	.endif
	.endm

	.macro mvaddr_h_gr reg, addr
	seth \reg, high(\addr)
	or3 \reg, \reg, low(\addr)
	.endm

# Other macros know this only clobbers r0.
	.macro test_h_gr reg, val
	mvaddr_h_gr r0, \val
	beq \reg, r0, test_gr\@
	fail
test_gr\@:
	.endm

	.macro mvi_h_condbit val
	ldi8 r0, 0
	ldi8 r1, 1
	.if \val
	cmp r0, r1
	.else
	cmp r1, r0
	.endif
	.endm

	.macro test_h_condbit val
	.if \val
	bc test_c1\@
	fail
test_c1\@:
	.else
	bnc test_c0\@
	fail
test_c0\@:
	.endif
	.endm

	.macro mvi_h_accum0 hi, lo
	mvi_h_gr r0, \hi
	mvtachi r0
	mvi_h_gr r0, \lo
	mvtaclo r0
	.endm

	.macro test_h_accum0 hi, lo
	mvfachi r1
	test_h_gr r1, \hi
	mvfaclo r1
	test_h_gr r1, \lo
	.endm

