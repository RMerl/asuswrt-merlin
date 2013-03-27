; Checking read-after-write: swrite-then-read 2 cycles.
#mach: crisv32
#output: Basic clock cycles, total @: 4\n
#output: Memory source stall cycles: 0\n
#output: Memory read-after-write stall cycles: 2\n
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
 .lcomm x,4
 .lcomm y,4
 move.d x,$r0
 move.d y,$r1
 clear.d [$r0]
 move [$r1],$srp
 break 15
