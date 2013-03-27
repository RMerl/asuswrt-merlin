; Checking read-after-write: movemwrite-then-nop-read 2 cycles.
#mach: crisv32
#ld: --section-start=.text=0
#output: Basic clock cycles, total @: 7\n
#output: Memory source stall cycles: 0\n
#output: Memory read-after-write stall cycles: 2\n
#output: Movem source stall cycles: 0\n
#output: Movem destination stall cycles: 0\n
#output: Movem address stall cycles: 1\n
#output: Multiplication source stall cycles: 0\n
#output: Jump source stall cycles: 0\n
#output: Branch misprediction stall cycles: 0\n
#output: Jump target stall cycles: 0\n
#sim: --cris-cycles=basic
 .include "testutils.inc"
 startnostack
 .lcomm x,4*11
 .lcomm y,4
 move.d x,$r0
 move.d y,$r1
 nop
 nop
 movem $r10,[$r0]
 nop
 move.d [$r1],$r2
 break 15
