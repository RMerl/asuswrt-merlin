# mach: crisv10
# output: ef\n

; Check that "clearf x" doesn't trivially fail.

 .include "testutils.inc"
 start
 setf mbixnzvc
 clearf x	; Actually, x would be cleared by almost-all other insns.
 move dccr,r3
 dumpr3
 quit
