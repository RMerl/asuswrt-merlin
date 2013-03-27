# mach: crisv0 crisv3 crisv8 crisv10
# output: 15\nffff1234\n2\n7\nb\n16\nf\n2\nf\nffffffef\n7\nfffffff4\nf\nfffffff2\nd\n10\nfffffff2\n8\nfffffff4\n

 .include "testutils.inc"
 .data
x:
 .dword 8,9,10,11
y:
 .dword -12,13,-14,15,16

 start
 moveq 7,r0
 moveq 2,r1
 move.d 0xffff1234,r2
 moveq 21,r3
 move.d x,r4
 setf zcvn
 movem r2,[r4+]
 test_cc 1 1 1 1
 subq 12,r4

 dumpr3 ; 15

 move.d [r4+],r3
 dumpr3 ; ffff1234

 move.d [r4+],r3
 dumpr3 ; 2

 move.d [r4+],r3
 dumpr3 ; 7

 move.d [r4+],r3
 dumpr3 ; b

 subq 16,r4
 moveq 22,r0
 moveq 15,r1
 clearf zcvn
 movem r0,[r4]
 test_cc 0 0 0 0
 move.d [r4+],r3
 dumpr3 ; 16

 move.d r1,r3
 dumpr3 ; f

 move.d [r4+],r3
 dumpr3 ; 2

 moveq 10,r2
 moveq -17,r0
 clearf zc
 setf vn
 movem r1,[r4=r4-8]
 test_cc 1 0 1 0
 move.d [r4+],r3
 dumpr3 ; f

 move.d [r4+],r3
 dumpr3 ; ffffffef

 move.d [r4+],r3
 dumpr3 ; 7

 move.d y,r4
 setf zc
 clearf vn
 movem [r4+],r3
 test_cc 0 1 0 1
 dumpr3 ; fffffff4

 move.d r0,r3
 dumpr3 ; f

 move.d r1,r3
 dumpr3 ; fffffff2

 moveq -12,r1

 move.d r2,r3
 dumpr3 ; d

 move.d [r4],r3
 dumpr3 ; 10

 setf zcvn
 movem [r5=r4-8],r0
 test_cc 1 1 1 1
 move.d r0,r3
 dumpr3 ; fffffff2

 sub.d r5,r4
 move.d r4,r3
 dumpr3 ; 8

 move.d r1,r3
 dumpr3 ; fffffff4

 quit

