#mach: crisv32
#output: Basic clock cycles, total @: 18\n
#output: Memory source stall cycles: 0\n
#output: Memory read-after-write stall cycles: 0\n
#output: Movem source stall cycles: 0\n
#output: Movem destination stall cycles: 6\n
#output: Movem address stall cycles: 0\n
#output: Multiplication source stall cycles: 0\n
#output: Jump source stall cycles: 0\n
#output: Branch misprediction stall cycles: 0\n
#output: Jump target stall cycles: 0\n
#sim: --cris-cycles=basic

; Check that movem to register followed by register write dword
; to one of the registers is logged as needing two stall cycles,
; regardless of size.

 .include "testutils.inc"
 startnostack
 move.d 0f,r5
 moveq 0,r8
 moveq 0,r9

 movem [r5],r4
 move.d r8,r1
 addq 1,r1	; 2 cycles.

 movem [r5],r4
 move.w r8,r1
 addq 1,r1	; 2 cycles.

 movem [r5],r4
 move.b r8,r1
 addq 1,r1	; 2 cycles.

 movem [r5],r4
 move.b r8,r1
 addq 1,r9

 movem [r5],r4
 move.d r8,r1
 addq 1,r8

 break 15

 .data
 .p2align 5
0:
 .dword 0b
 .dword 0b
 .dword 0b
 .dword 0b
 .dword 0b
