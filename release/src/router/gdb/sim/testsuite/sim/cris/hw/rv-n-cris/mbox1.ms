#mach: crisv10 crisv32
#sim(crisv10): --hw-device "/rv/trace? true"
#sim(crisv32): --hw-device "/rv/trace? true"
#output: /rv: WD\n
#output: /rv: MBOX H 0x1001d..0x10037\n
#output: /rv: 0x10020: 12 23 34 56 79 8a bd de\n
#output: /rv: 0x10028: fb ad ba db ad 56 78 9a\n
#output: /rv: 0x10030: fd e1 23 45 66 54 32 1a\n
#output: /rv: -> 0x1001d..0x10027\n
#output: /rv: 0x10020: aa 55 77 88 32 10 ee cc\n
#output: /rv: MBOX P 0xfffd..0x1001f\n
#output: /rv: 0x10000: aa 55 12 23 34 56 79 8a\n
#output: /rv: 0x10008: bd de fb ad ba db ad 56\n
#output: /rv: 0x10010: 78 9a fd e1 23 45 66 54\n
#output: /rv: 0x10018: 32 1a ac cb be ed db ed\n
#output: /rv: -> 0xfffd..0x10017\n
#output: /rv: 0x10000: 11 22 56 78 ee dd 12 ab\n
#output: /rv: 0x10008: 55 aa ee 00 42 12 27 98\n
#output: /rv: 0x10010: 88 55 22 33 66 77 22 45\n
#output: /rv: REG R 0xd0000038\n
#output: /rv: := 0x76543211\n
#output: pass\n

# Trivial test of mailbox commands.

#r W,
#r i,1b000512233456798abddefbadbadbad56789afde123456654321a
#r o,0b0005aa5577883210eecc
#r i,230006aa5512233456798abddefbadbadbad56789afde123456654321aaccbbeeddbed
#r o,1b000511225678eedd12ab55aaee00421227988855223366772245
#r r,a8838,76543211

 .include "testutils.inc"
 start
 move.w 0x1b,$r0
 move.d 0x1001d,$r1
 move.w $r0,[$r1+]
 moveq 5,$r0
 move.b $r0,[$r1]
 mvi_h_mem 0x56342312 0x10020
 mvi_h_mem 0xdebd8a79 0x10024
 mvi_h_mem 0xdbbaadfb 0x10028
 mvi_h_mem 0x9a7856ad 0x1002c
 mvi_h_mem 0x4523e1fd 0x10030
 mvi_h_mem 0x1a325466 0x10034

 mvi_h_mem 0x1001d 0xc000f000

 move.d 0x1001d,$r0
 movu.w [$r0+],$r1
 test_h_gr 0xb $r1
 movu.b [$r0],$r1
 test_h_gr 0x5 $r1
 test_h_mem 0x887755aa 0x10020
 test_h_mem 0xccee1032 0x10024

 move.w 0x23,$r0
 move.d 0xfffd,$r1
 move.w $r0,[$r1+]
 moveq 6,$r0
 move.b $r0,[$r1]
 mvi_h_mem 0x231255aa 0x10000
 mvi_h_mem 0x8a795634 0x10004
 mvi_h_mem 0xadfbdebd 0x10008
 mvi_h_mem 0x56addbba 0x1000c
 mvi_h_mem 0xe1fd9a78 0x10010
 mvi_h_mem 0x54664523 0x10014
 mvi_h_mem 0xcbac1a32 0x10018
 mvi_h_mem 0xeddbedbe 0x1001c

 mvi_h_mem 0xfffd 0xc000f000

 move.d 0xfffd,$r0
 movu.w [$r0+],$r1
 test_h_gr 0x1b $r1
 movu.b [$r0],$r1
 test_h_gr 0x6 $r1
 test_h_mem 0x78562211 0x10000
 test_h_mem 0xab12ddee 0x10004
 test_h_mem 0x00eeaa55 0x10008
 test_h_mem 0x98271242 0x1000c
 test_h_mem 0x33225588 0x10010
 test_h_mem 0x45227766 0x10014

 test_h_mem 0x76543211 0xd0000038
 pass

 .fill 65536*2+128,1,0
