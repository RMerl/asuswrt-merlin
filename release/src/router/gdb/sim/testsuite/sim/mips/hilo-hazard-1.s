# Test for architectures with mf{hi,lo} -> mult/div/mt{hi,lo} hazards.
#
# mach:		mips1 mips2 mips3 mips4 vr4100 vr4111 vr4120 vr5000 vr5400
# as:		-mabi=eabi
# ld:		-N -Ttext=0x80010000
# output:	HILO: * too close to MF at *\\n\\nprogram stopped*\\n
# xerror:

	.include "hilo-hazard.inc"
	.include "testutils.inc"

	setup

	.set noreorder
	.ent DIAG
DIAG:
	hilo
	pass
	.end DIAG
