#mach: crisv32
#output: Basic clock cycles, total @: 54\n
#output: Memory source stall cycles: 0\n
#output: Memory read-after-write stall cycles: 0\n
#output: Movem source stall cycles: 0\n
#output: Movem destination stall cycles: 0\n
#output: Movem address stall cycles: 0\n
#output: Multiplication source stall cycles: 0\n
#output: Jump source stall cycles: 0\n
#output: Branch misprediction stall cycles: 18\n
#output: Jump target stall cycles: 0\n
#sim: --cris-cycles=basic

; Check branch penalties.  It is assumed that the taken-counters
; in the bimodal branch-predictors start at 0, meaning two taken
; branches are required for a branch to be predicted as taken
; for each counter, from reset.  None of these branches go
; to the end of a cache-line and none map to the same counter.

 .include "testutils.inc"
 startnostack
 ba 0f		; No penalty: always-taken condition not "predicted".
 nop
 nop
0:
 setf c
 bcs 0f		; Penalty 2 cycles.
 nop

 nop
0:
 clearf c
 bcc 0f		; Penalty 2 cycles, though branch is a nop.
 moveq 4,r0	; Execute 5 times:

0:
 move.d r0,r0
 bne 0b		; Mispredicted 3 out of 5 times: penalty 3*2 cycles.
 subq 1,r0

0:
 beq 0f		; Not taken; no penalty.
 nop

 nop
0:

; (Almost) same insns, but with 16-bit bCC insns.

 ba 0f		; No penalty: always-taken condition not "predicted".
 nop
 .space 520
0:
 setf c
 bcs 0f		; Penalty 2 cycles.
 nop

 .space 520
0:
 moveq 4,r0	; Execute 5 times:
0:
 ba 1f
 move.d r0,r0	; Mispredicted 3 out of 5 times:
 .space 520
1:
 bne 0b		; Penalty 3*2 cycles.
 subq 1,r0

 beq 0f		; Not taken; no penalty.
 nop
0:
 break 15
