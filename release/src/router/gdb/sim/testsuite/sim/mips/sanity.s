# mips test sanity, expected to pass.
# mach:	 all
# as:		-mabi=eabi
# ld:		-N -Ttext=0x80010000
# output:	*\\npass\\n

	.include "testutils.inc"

	setup

	.set noreorder

	.ent DIAG
DIAG:

	writemsg "Sanity is good!"

	pass

	.end DIAG
