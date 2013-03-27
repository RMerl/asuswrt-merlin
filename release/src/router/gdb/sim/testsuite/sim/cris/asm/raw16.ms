; Checking read-after-write: cycles included in "unaligned".
#mach: crisv32
#output: Clock cycles including stall cycles for unaligned accesses @: 4\n
#output: Memory source stall cycles: 0\n
#output: Memory read-after-write stall cycles: 2\n
#output: Movem source stall cycles: 0\n
#output: Movem destination stall cycles: 0\n
#output: Movem address stall cycles: 0\n
#output: Multiplication source stall cycles: 0\n
#output: Jump source stall cycles: 0\n
#output: Branch misprediction stall cycles: 0\n
#output: Jump target stall cycles: 0\n
#sim: --cris-cycles=unaligned
 .include "raw4.ms"
