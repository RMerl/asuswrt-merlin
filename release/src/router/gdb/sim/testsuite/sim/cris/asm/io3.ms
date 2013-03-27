# mach: crisv32
# sim: --cris-900000xx
# output: ce11d0c\n

; Check correct "pass" exit.

 .include "testutils.inc"
 start
 move.d 0x0ce11d0c,$r3
 dumpr3
 move.d 0x90000004,$acr
 move.d $acr,[$acr]
 move.d 0xbadc0de,$r3
 dumpr3
0:
 ba 0b
 nop
