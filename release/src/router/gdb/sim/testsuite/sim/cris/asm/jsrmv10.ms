# mach: crisv3 crisv8 crisv10
# output: 23\n

# Test that jsr [] records the correct return-address.

 .include "testutils.inc"
 start
x:
 moveq 0,r3
 jsr [z]
 addq 1,r3
 nop
 nop
 nop
 nop
 nop
 move.d w,r2
 jsr [r2]
 addq 1,r3
 nop
 nop
 nop
 nop
 nop
 dumpr3 ; 23
 quit
y:
 ret
 addq 1,r3
 quit

v:
 ret
 addq 32,r3
 quit

z:
 .dword y
w:
 .dword v
