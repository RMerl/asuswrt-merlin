# mach: crisv32
# xerror:
# output: program stopped with signal 4.\n
 .include "testutils.inc"

; Check that bound with a memory operand is invalid.
 start
 move.d 0f,r5
 move.d r5,r3
 .byte 0xd5,0x39	; bound.d [r5],r3  -- we can't assemble it.
 pass

0:
 .dword 0b

