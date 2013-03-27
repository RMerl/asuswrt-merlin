# mach: crisv0 crisv3 crisv8 crisv10 crisv32
# output: 3\n3\nffffff03\nffff0003\nff00\n0\n10000\n10000\n0\nffffff00\n0\n781343c9\n781344c9\n78124cc9\n78134cc9\nc450\n7ffff8ce\n

 .include "testutils.inc"
 .data
x:
 .byte 0xff
 .word 0xffff
 .word 0xff
 .word 0xffff
 .byte 0x89
 .word 0xf789
 .word 0x8002
 .word 0x764

 start
 moveq 2,r3
 move.d x,r5
 subs.b [r5+],r3
 test_cc 0 0 0 1
 dumpr3 ; 3

 moveq 2,r3
 subs.w [r5+],r3
 test_cc 0 0 0 1
 dumpr3 ; 3

 moveq 2,r3
 subq 3,r5
 subu.b [r5+],r3
 test_cc 1 0 0 1
 dumpr3 ; ffffff03

 moveq 2,r3
 subu.w [r5+],r3
 test_cc 1 0 0 1
 subq 3,r5
 dumpr3 ; ffff0003

 move.d 0xffff,r3
 subu.b [r5],r3
 test_cc 0 0 0 0
 dumpr3 ; ff00

 move.d 0xffff,r3
 subu.w [r5],r3
 test_cc 0 1 0 0
 dumpr3 ; 0

 move.d 0xffff,r3
 subs.b [r5],r3
 test_cc 0 0 0 1
 dumpr3 ; 10000

 move.d 0xffff,r3
 subs.w [r5],r3
 test_cc 0 0 0 1
 dumpr3 ; 10000

 moveq -1,r3
 subs.b [r5],r3
 test_cc 0 1 0 0
 addq 3,r5
 dumpr3 ; 0

 moveq -1,r3
 subs.w [r5+],r3
 test_cc 1 0 0 0
 dumpr3 ; ffffff00

 moveq -1,r3
 subs.w [r5+],r3
 test_cc 0 1 0 0
 dumpr3 ; 0

 move.d 0x78134452,r3
 subu.b [r5],r3
 test_cc 0 0 0 0
 dumpr3 ; 781343c9

 move.d 0x78134452,r3
 subs.b [r5+],r3
 test_cc 0 0 0 1
 dumpr3 ; 781344c9

 move.d 0x78134452,r3
 subu.w [r5],r3
 test_cc 0 0 0 0
 dumpr3 ; 78124cc9

 move.d 0x78134452,r3
 subs.w [r5+],r3
 test_cc 0 0 0 1
 dumpr3 ; 78134cc9

 move.d 0x4452,r3
 subs.w [r5+],r3
 test_cc 0 0 0 1
 dumpr3 ; c450

 move.d 0x80000032,r3
 subu.w [r5+],r3
 test_cc 0 0 1 0
 dumpr3 ; 7ffff8ce

 quit
