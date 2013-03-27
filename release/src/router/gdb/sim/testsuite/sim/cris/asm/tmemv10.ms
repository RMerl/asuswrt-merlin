#mach: crisv10
#output: Basic clock cycles, total @: 8\n
#output: Memory source stall cycles: 0\n
#output: Memory read-after-write stall cycles: 0\n
#output: Movem source stall cycles: 0\n
#output: Movem destination stall cycles: 0\n
#output: Movem address stall cycles: 0\n
#output: Multiplication source stall cycles: 0\n
#output: Jump source stall cycles: 0\n
#output: Branch misprediction stall cycles: 0\n
#output: Jump target stall cycles: 0\n
#sim: --cris-cycles=basic

; Check that the memory indirection doesn't make the simulator barf.
; Nothing deeper.

 .include "testutils.inc"
 startnostack
 move.d 0f,r5
 move.d [r5],r4
 move.d [r5+],r3
 move.d [r5],r2
 break 15
 nop
 .p2align 2
0:
 .dword 1,2,3
