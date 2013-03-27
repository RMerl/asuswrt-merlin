# mach: m32r m32rx
# xerror:
# output: *misaligned read*

	.include "testutils.inc"

	start

; construct bra trap2_handler in trap 2 slot
	ld24 r0,#foo+1
	ld r0,@r0
	fail
	exit 0

.data
	.p2align 2
foo:
	.word 42
