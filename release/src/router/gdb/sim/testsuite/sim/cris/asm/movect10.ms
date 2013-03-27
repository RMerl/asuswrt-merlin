#mach: crisv10
#output: Basic clock cycles, total @: 3\n
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

 .include "testutils.inc"
 startnostack
 nop
 move.d 0xff004567,r5
 break 15
