# mach: crisv0 crisv3 crisv8 crisv10 crisv32
# output: 1\n1\n101\n10001\n100fe\n1fffe\nfffe\nfffe\nfffffffe\nfe\nfffffffe\n781344db\n781343db\n78143bdb\n78133bdb\n800000ed\n0\n

 .include "testutils.inc"
 start
 moveq 2,r3
 move.d 0xff,r4
 adds.b r4,r3
 dumpr3 ; 1

 moveq 2,r3
 move.d 0xffff,r4
 adds.w r4,r3
 dumpr3 ; 1

 moveq 2,r3
 move.d 0xffff,r4
 addu.b r4,r3
 dumpr3 ; 101

 moveq 2,r3
 move.d 0xffffffff,r4
 addu.w r4,r3
 dumpr3 ; 10001

 move.d 0xffff,r3
 move.d 0xffffffff,r4
 addu.b r4,r3
 dumpr3 ; 100fe

 move.d 0xffff,r3
 move.d 0xffffffff,r4
 addu.w r4,r3
 dumpr3 ; 1fffe

 move.d 0xffff,r3
 move.d 0xff,r4
 adds.b r4,r3
 dumpr3 ; fffe

 move.d 0xffff,r4
 move.d r4,r3
 adds.w r4,r3
 dumpr3 ; fffe

 moveq -1,r3
 move.d 0xff,r4
 adds.b r4,r3
 dumpr3 ; fffffffe

 moveq -1,r3
 move.d 0xff,r4
 adds.w r4,r3
 dumpr3 ; fe

 moveq -1,r3
 move.d 0xffff,r4
 adds.w r4,r3
 dumpr3 ; fffffffe

 move.d 0x5432f789,r4
 move.d 0x78134452,r3
 addu.b r4,r3
 dumpr3 ; 781344db

 move.d 0x5432f789,r4
 move.d 0x78134452,r3
 adds.b r4,r3
 dumpr3 ; 781343db

 move.d 0x5432f789,r4
 move.d 0x78134452,r3
 addu.w r4,r3
 dumpr3 ; 78143bdb

 move.d 0x5432f789,r4
 move.d 0x78134452,r3
 adds.w r4,r3
 dumpr3 ; 78133bdb

 move.d 0x7fffffee,r3
 move.d 0xff,r4
 addu.b r4,r3
 test_cc 1 0 1 0
 dumpr3 ; 800000ed

 move.d 0x1,r3
 move.d 0xffff,r4
 adds.w r4,r3
 test_cc 0 1 0 1
 dumpr3 ; 0

 quit
