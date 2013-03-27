# mach: crisv32
# output: ef\n

; Check that "clearf x" doesn't trivially fail.

 .include "testutils.inc"
 start
 setf puixnzvc
 clearf x	; Actually, x would be cleared by almost-all other insns.
 move ccs,r3
 dumpr3
 quit
