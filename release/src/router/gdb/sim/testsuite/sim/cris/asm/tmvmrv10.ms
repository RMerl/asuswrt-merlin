#mach: crisv10
#output: Basic clock cycles, total @: 45\n
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

; Check that movem to register basically looks ok cycle-wise.
; Nothing deep.

 .include "testutils.inc"
 startnostack
 move.d 0f,r5
 moveq 0,r8
 moveq 0,r9

; Adapted from crisv32 movem-to-memory penalty examples many
; revisions ago.

 movem [r5],r4
 test.d [r3]	; 3 cycle penalty on v32 (2 memory source, 1 movem dest).
 movem [r5],r4
 subq 1,r8
 test.d [r3]	; 2 cycle penalty on v32.
 movem [r5],r4
 subq 1,r1	; 3 cycle penalty on v32.
 movem [r5],r4
 add.d r8,r9
 subq 1,r1	; 2 cycle penalty on v32.
 movem [r5],r4
 add.d r8,r9
 subq  1, r9
 subq  1, r1	; 1 cycle penalty on v32.
 break 15

 .data
 .p2align 5
0:
 .dword 0b
 .dword 0b
 .dword 0b
 .dword 0b
 .dword 0b

