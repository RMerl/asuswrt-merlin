.include "t-macros.i"

	start

	; Test ld and st
	ld r4, @foo
	check 1 r4 0xdead

	ldi r4, #0x2152
	st r4, @foo
	ld r4, @foo
	check 2 r4 0x2152

	; Test ld2w and st2w
	ldi r4, #0xdead
	st r4, @foo
	ld2w r4, @foo
	check2w2 3 r4 0xdead 0xf000

	ldi r4, #0x2112
	ldi r5, #0x1984
	st2w r4, @foo
	ld2w r4, @foo
	check2w2 4 r4 0x2112 0x1984

	.data
	.align 2
foo:	.short 0xdead
bar:	.short 0xf000
	.text

	exit0
