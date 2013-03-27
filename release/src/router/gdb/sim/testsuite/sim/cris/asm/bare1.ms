# mach: crisv32
# ld: --section-start=.text=0
# output: 0\n0\n4\n42\n
# sim: --cris-naked

; Check that we don't get signs of an initialized environment
; when --cris-naked.

 .include "testutils.inc"
 .text
 .global _start
_start:
 nop
 nop
start2:
 move.d $r10,$r3
 dumpr3
 move.d $sp,$r3
 dumpr3
 lapc start2,$r3
 dumpr3
 move.d 0x42,$r3
 dumpr3
 quit
