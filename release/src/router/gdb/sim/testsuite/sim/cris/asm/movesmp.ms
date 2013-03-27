# mach: crisv3 crisv8 crisv10
# output: bed0bed1\nabedab0d\nbed0bed1\n

# Test that move to and from special register and memory clears the
# "prefixed" bit.

 .include "testutils.inc"
 .data
w:
 .dword 0
y:
 .dword 0xbed0bed1
z:
 .dword 0xabedab0d

 start
x:
 move.d y,r3
 clear.d [w]
 move.d [r3],r3
 dumpr3 ; bed0bed1
 move.d z,r3
 move [w+4],srp
 move.d [r3],r3
 dumpr3 ; abedab0d
 move srp,r3
 dumpr3 ; bed0bed1
 quit
