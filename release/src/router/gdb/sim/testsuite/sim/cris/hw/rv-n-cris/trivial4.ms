#mach: crisv32
#r @,@srcdir@/trivial4.r

# Test read and writes.

 .include "testutils.inc"
 start
 move.d 0xd0000032,$r0
 move.d [$r0+],$r3
 cmp.d 0xabcdef01,$r3
 beq ok
 nop
bad:
 fail
ok:
 move.d 0xaabbccdd,$r3
 move.d $r3,[$r0]
 move.d [$r0],$r3
 cmp.d 0x76543210,$r3
 bne bad
 nop
 pass
