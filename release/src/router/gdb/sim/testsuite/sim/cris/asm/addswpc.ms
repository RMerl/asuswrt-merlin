# mach: crisv3 crisv8 crisv10
# output: 7\n

# Test that the special case adds.w [pc+rN.w],pc works.

 .include "testutils.inc"
 start
x:
 moveq 0,r3
 ba xy
 moveq 5,r2

ok:
 moveq 7,r3
 dumpr3
 quit

xy:
 adds.w [pc+r2.w],pc
y:
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word ok-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
 .word x0-y
x0:
 quit
