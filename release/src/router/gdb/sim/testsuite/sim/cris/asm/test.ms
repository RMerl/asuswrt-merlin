# mach: crisv0 crisv3 crisv8 crisv10 crisv32
# output: 1\n

 .include "testutils.inc"
 .data
x:
 .dword 0,2,-1,0x80000000,0x5432f789
 .word 0,2,-1,0xffff,0xf789
 .byte 0,2,0xff,0x89

 start
 clearf nzvc
 moveq -1,r3
 move.d x,r5
 setf vc
 test.d [r5+]
 test_cc 0 1 0 0

 setf vc
 test.d [r5]
 test_cc 0 0 0 0

 addq 4,r5

 setf vc
 test.d [r5+]
 test_cc 1 0 0 0

 setf vc
 test.d [r5+]
 test_cc 1 0 0 0

 setf vc
 test.d [r5+]
 test_cc 0 0 0 0

 setf vc
 test.w [r5+]
 test_cc 0 1 0 0

 setf vc
 test.w [r5]
 test_cc 0 0 0 0

 addq 2,r5

 setf vc
 test.w [r5+]
 test_cc 1 0 0 0

 setf vc
 test.w [r5+]
 test_cc 1 0 0 0

 setf vc
 test.w [r5+]
 test_cc 1 0 0 0

 setf vc
 test.b [r5]
 test_cc 0 1 0 0

 addq 1,r5

 setf vc
 test.b [r5+]
 test_cc 0 0 0 0

 setf vc
 test.b [r5+]
 test_cc 1 0 0 0

 setf vc
 test.b [r5]
 test_cc 1 0 0 0

 moveq 1,r3
 dumpr3

 quit
