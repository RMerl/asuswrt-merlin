#mach: crisv10
#output: Basic clock cycles, total @: 9\n
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

; Check that multiplications do not make the simulator barf.
; Nothing deeper.

 .include "testutils.inc"
 startnostack
 moveq 1,r3
 moveq 2,r1
 moveq 1,r0
 muls.d r0,r1
 muls.d r0,r3
 mulu.d r1,r3
 break 15
 nop
