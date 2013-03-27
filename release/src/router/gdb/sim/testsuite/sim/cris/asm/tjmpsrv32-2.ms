#mach: crisv32
#output: Basic clock cycles, total @: 37\n
#output: Memory source stall cycles: 0\n
#output: Memory read-after-write stall cycles: 0\n
#output: Movem source stall cycles: 0\n
#output: Movem destination stall cycles: 0\n
#output: Movem address stall cycles: 0\n
#output: Multiplication source stall cycles: 0\n
#output: Jump source stall cycles: 6\n
#output: Branch misprediction stall cycles: 0\n
#output: Jump target stall cycles: 0\n
#sim: --cris-cycles=basic

; Check that we correctly account for that a "jas N,Pn",
; "jasc N,Pn", "bas N,Pn" and "basc N,Pn" sets the specific
; special register and causes a pipeline hazard.  The amount
; of nops below is a bit inflated, in an attempt to make
; errors more discernible.  For special registers, we just
; check SRP.

 .include "testutils.inc"
 startnostack
 move.d 0f,$r0
 jsr 0f
 nop
 nop
 nop
 jsrc 0f
 nop
 .dword -1
 nop
 nop
 jsr $r0
 nop
 nop
 nop
 jsrc $r0
 nop
 .dword -1
 nop
 nop
 bsr 0f
 nop
 nop
 nop
 bsrc 0f
 nop
 .dword -1
 nop
 nop
 break 15

0:
 ret		; 1 cycle penalty.
 nop
