# mach: crisv32
# output: 2222\n

# Test that jump Pd works.

 .include "testutils.inc"
 start
x:
 setf zvnc
 move 0f,srp
 test_cc 1 1 1 1
 jump srp
 nop
 quit

0:
 test_cc 1 1 1 1
 move 1f,mof
 jump mof
 nop
 quit

 .space 32768,0
 quit
1:
 move.d 0x2222,r3
 dumpr3
 quit
