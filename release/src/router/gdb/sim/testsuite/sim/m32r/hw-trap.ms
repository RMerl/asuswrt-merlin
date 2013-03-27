# mach(): m32r m32rx
# output(): pass\n

	.include "testutils.inc"

	start

; construct bra trap2_handler in trap 2 slot
	ld24 r0,#bra_insn
	ld r0,@r0
	ld24 r1,#trap2_handler
	addi r1,#-0x48 ; pc relative address from trap 2 slot to handler
	srai r1,#2
	or r0,r1
	ld24 r2,#0x48 ; address of trap 2 slot
	st r0,@r2

; perform trap
	ldi r4,#0
	trap #2
	test_h_gr r4,42

	pass

; trap 2 handler
trap2_handler:
	ldi r4,#42
	rte

bra_insn:
	bra.l 0
