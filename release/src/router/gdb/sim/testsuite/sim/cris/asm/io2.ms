# mach: crisv32
# sim: --cris-900000xx
# xerror:
# output: b1e\n

; Check correct "fail" exit.

 .include "testutils.inc"
 start
 move.d 0xb1e,$r3
 dumpr3
 move.d 0x90000008,$acr
 move.d $acr,[$acr]
 move.d 0xbadc0de,$r3
 dumpr3
0:
 ba 0b
 nop
