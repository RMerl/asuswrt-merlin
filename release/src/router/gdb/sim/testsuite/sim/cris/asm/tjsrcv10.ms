#mach: crisv10
#output: Basic clock cycles, total @: 6\n
#output: Memory source stall cycles: 1\n
#output: Memory read-after-write stall cycles: 0\n
#output: Movem source stall cycles: 0\n
#output: Movem destination stall cycles: 0\n
#output: Movem address stall cycles: 0\n
#output: Multiplication source stall cycles: 0\n
#output: Jump source stall cycles: 0\n
#output: Branch misprediction stall cycles: 0\n
#output: Jump target stall cycles: 0\n
#sim: --cris-cycles=basic

; Check that the 4-byte-skip doesn't make the simulator barf.
; Nothing deeper.

 .include "testutils.inc"
 startnostack
 nop
 move.d 0f,r5
 jsrc r5
 nop
 .dword -1
0:
 jsrc 1f
 nop
 .dword -2
1:
 break 15
