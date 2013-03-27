# mach: crisv3 crisv8 crisv10
# output: aaeebb11\nde378218\n

# Test that the special case "X [pc+I],Y" works, where I byte-sized.

 .include "testutils.inc"
 start
x:
; FIXME: Gas bugs are making this a bit harder than necessary.
; move.d [pc+y-(.+2)],r3
 move.d [pc+8],r3
yy:
 jump zz

y:
 .dword 0xaaeebb11
y2:
 .dword 0xde378218

zz:
 dumpr3
 jump z
 quit

; Check a negative offset.
 .space 50
z:
 move.d [pc+y2-(.+2)],r3
 dumpr3
 quit
