#mach: crisv10 crisv32
#sim(crisv10): --hw-device "/rv/trace? true" --hw-device "/rv/watchdog-interval 1"
#sim(crisv32): --hw-device "/rv/trace? true" --hw-device "/rv/watchdog-interval 1" --hw-device "/rv/max-poll-ticks 1000"
#output: /rv: WD\n
#output: /rv: REG R 0xd0000036\n
#output: /rv: := 0x76543210\n
#output: /rv: WD\n
#output: /rv: DMA W 0x20000..0x20003\n
#output: /rv: 0x20000: 01 02 03 04\n
#output: /rv: REG R 0xd0000038\n
#output: /rv: := 0x76543211\n
#output: pass\n

#r W,
#r r,a8836,76543210
#r W,
#r s,e000,01020304
#r r,a8838,76543211

 .include "testutils.inc"
 start
 mvi_h_mem 0 0x20000
 test_h_mem 0x76543210 0xd0000036

 move.d 0x20000,$r1
0:
 test.b [$r1]
 beq 0b
 nop
 test_h_mem 0x76543211 0xd0000038
 pass

 .fill 65536*2+128,1,0
