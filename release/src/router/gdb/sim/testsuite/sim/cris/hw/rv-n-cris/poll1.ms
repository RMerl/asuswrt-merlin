#mach: crisv32
#sim(crisv32): --hw-device "/rv/dummy 0x12"

# A variant of trivial2.ms to check that the right thing happens when
# we reach the poll function with a dummy device.

 .include "testutils.inc"
 start
 move.d 0xd0000000,$r0
 move.d [$r0+],$r3
 cmp.d 0x12121212,$r3
 beq ok
 nop
bad:
 fail
ok:
 move.d 0x10000,$r10
0:
 bne 0b
 subq 1,$r10

 pass
