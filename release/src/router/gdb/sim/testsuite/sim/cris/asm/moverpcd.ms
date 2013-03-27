# mach: crisv3 crisv8 crisv10
# output: 4\n

# Test that move.d pc,R works.

 .include "testutils.inc"
 start
x:
 move.d pc,r3
y:
 sub.d y-4,r3
 dumpr3
 quit
