#mach: crisv32
#sim(crisv32): --hw-device "/rv/dummy 0x12"

# Test dummy settings: set from value.

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
 moveq -1,$r3
 move.d $r3,[$r0]
 cmp.d [$r0],$r3
 bne bad
 nop
 pass
