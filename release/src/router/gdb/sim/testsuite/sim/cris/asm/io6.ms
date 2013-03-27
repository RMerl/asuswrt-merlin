# mach: crisv32
# ld: --section-start=.text=0
# sim: --cris-900000xx
# xerror:
# output: b1e\n
# output: core: 4 byte write to unmapped address 0x90000008 at 0x16\n
# output: program stopped with signal 11.\n

; Check that invalid access to the simulator area is recognized.
; "FAIL" area.

 .include "testutils.inc"
 start
 move.d 0xb1e,$r3
 dumpr3
 move.d 0x90000008,$acr
 clear.d [$acr]
 move.d 0xbadc0de,$r3
 dumpr3
0:
 ba 0b
 nop
