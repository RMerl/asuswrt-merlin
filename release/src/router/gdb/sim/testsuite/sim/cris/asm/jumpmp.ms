# mach: crisv3 crisv8 crisv10
# output: bed0bed1\n

# Test that jump indirect clears the "prefixed"
# bit.

 .include "testutils.inc"
 .data
w:
 .dword x1
y:
 .dword 0xbed0bed1

 start
x:
 move.d y,r3
 jump [w]
x1:
 move.d [r3],r3
 dumpr3 ; bed0bed1
 quit
