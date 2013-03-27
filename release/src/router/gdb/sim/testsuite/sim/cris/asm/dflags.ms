# mach: crisv3 crisv8 crisv10 crisv32
# output: 31\n

; Check that flag settings in the delay slot for a conditional branch do
; not affect the branch.

 .include "testutils.inc"

 start
 moveq 1,r3
 moveq 0,r4

; 8-bit branches.

 move.d r4,r4
 bne 0f
 move.d r3,r3
 bne 1f
 move.d r4,r4
 nop
0:
 quit

1:
 move.d r3,r3
 beq 0b
 move.d r4,r4
 beq 4f
 move.d r3,r3
 nop
 quit
4:
 jump 2f
 nop
 .space 1000

; 16-bit branches

2:
 move.d r4,r4
 bne 0b
 move.d r3,r3
 bne 3f
 move.d r4,r4
 nop
 quit
 .space 1000

3:
 move.d r3,r3
 beq 0b
 move.d r4,r4
 beq 4f
 move.d r3,r3
 nop
 quit
 .space 1000

4:
 move.d 0x31,r3
 dumpr3
 quit
