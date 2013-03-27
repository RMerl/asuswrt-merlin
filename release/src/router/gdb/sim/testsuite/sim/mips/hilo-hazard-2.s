# Test for architectures without mf{hi,lo} -> mult/div/mt{hi,lo} hazards.
#
# mach:		vr5500 mips32 mips64
# as:		-mabi=eabi
# ld:		-N -Ttext=0x80010000
# output:	pass\\n

	.include "hilo-hazard.inc"
	.include "testutils.inc"

	setup

	.set noreorder
	.ent DIAG
DIAG:
	hilo
	pass
	.end DIAG
