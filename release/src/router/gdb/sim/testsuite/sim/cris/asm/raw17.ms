; Checking read-after-write: different read-after-write combinations.
#mach: crisv32
#output: Basic clock cycles, total @: 11\n
#output: Memory source stall cycles: 0\n
#output: Memory read-after-write stall cycles: 8\n
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
 move.d $r1,[$r0]
 move.d [$r1],$r2
 move.d [$r1],$r2
 clear.d [$r0]
 move.d [$r1],$r2
 movem $r0,[$r1]
 movem [$r1],$r0
 move $srp,[$r1]
 move.d [$r1],$r0
 break 15
