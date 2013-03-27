#mach: crisv32
#output: Basic clock cycles, total @: 17\n
#output: Memory source stall cycles: 0\n
#output: Memory read-after-write stall cycles: 0\n
#output: Movem source stall cycles: 0\n
#output: Movem destination stall cycles: 0\n
#output: Movem address stall cycles: 0\n
#output: Multiplication source stall cycles: 0\n
#output: Jump source stall cycles: 5\n
#output: Branch misprediction stall cycles: 0\n
#output: Jump target stall cycles: 0\n
#sim: --cris-cycles=basic

; Check that "ret"-type insns get the right number of penalty
; cycles for the special register source.

 .include "testutils.inc"
 startnostack
 move.d 1f,$r1
 move.d 0f,$r0
 move $r0,$mof
 jump $mof	; 2 cycles penalty.
 nop

0:
 move [$r1],$srp
 nop
 ret		; 1 cycle penalty. 
 nop

 break 15

0:
 move 2f,$nrp
 nop
 nop
 jump $nrp	; no penalty. 
 nop

 break 15

2:
 move 3f,$srp	; 2 cycles penalty.
 ret
 nop

3:
 break 15
1:
 .dword 0b
