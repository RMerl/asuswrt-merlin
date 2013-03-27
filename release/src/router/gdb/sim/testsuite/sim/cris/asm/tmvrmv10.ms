#mach: crisv10
#output: Basic clock cycles, total @: 31\n
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

; Check that movem to memory basically looks ok cycle-wise.
; Nothing deep.

 .include "testutils.inc"
 startnostack
 move.d 0f,r4
 moveq 0,r0
 moveq 1,r3
 moveq 2,r1
 moveq 1,r2
 movem r3,[r4] ; 2 cycles penalty for v32
 movem r3,[r4] ; 0 cycles penalty for v32
 moveq 1,r3
 nop
 movem r3,[r4] ; 1 cycle penalty for v32
 moveq 1,r3
 nop
 nop
 movem r3,[r4] ; 0 cycles penalty for v32
 break 15

 .data
0:
 .dword 0
 .dword 0
 .dword 0
 .dword 0
