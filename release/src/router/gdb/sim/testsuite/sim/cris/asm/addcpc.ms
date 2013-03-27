# mach: crisv3 crisv8 crisv10
# output: 2f\n31\n

# Test that the special case add.d const,pc works.

 .include "testutils.inc"
 start
x:
 add.d y-y0,pc
y0:
 quit

 .space 1000
 quit
 quit
 quit
 quit
 quit
z:
 move.d 49,r3
 dumpr3
 quit

 .space 1000
 quit
 quit
 quit
 quit
 quit
y:
 move.d 47,r3
 dumpr3
 add.d z-z0,pc
z0:
 quit
