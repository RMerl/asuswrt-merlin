#mach: crisv10 crisv32
#output: Schedulable clock cycles, total @: 5\n
#output: Memory source stall cycles: 0\n
#output: Memory read-after-write stall cycles: 0\n
#output: Movem source stall cycles: 0\n
#output: Movem destination stall cycles: 0\n
#output: Movem address stall cycles: 0\n
#output: Multiplication source stall cycles: 0\n
#output: Jump source stall cycles: 0\n
#output: Branch misprediction stall cycles: 0\n
#output: Jump target stall cycles: 0\n
#sim: --cris-cycles=schedulable
 .include "nopv32t.ms"
