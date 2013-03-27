# mach: crisv0 crisv3 crisv8 crisv10 crisv32
# output: 1\n1\n1fffe\nfffffffe\ncc463bdb\nffff0001\n1\nfffe\nfedafffe\n78133bdb\nffffff01\n1\nfe\nfeda49fe\n781344db\n

 .include "testutils.inc"
 start
 moveq -1,r3
 add.d 2,r3
 test_cc 0 0 0 1
 dumpr3 ; 1

 moveq 2,r3
 add.d -1,r3
 test_cc 0 0 0 1
 dumpr3 ; 1

 move.d 0xffff,r3
 add.d 0xffff,r3
 test_cc 0 0 0 0
 dumpr3 ; 1fffe

 moveq -1,r3
 add.d -1,r3
 test_cc 1 0 0 1
 dumpr3 ; fffffffe

 move.d 0x78134452,r3
 add.d 0x5432f789,r3
 test_cc 1 0 1 0
 dumpr3 ; cc463bdb

 moveq -1,r3
 add.w 2,r3
 test_cc 0 0 0 1
 dumpr3 ; ffff0001

 moveq 2,r3
 add.w -1,r3
 test_cc 0 0 0 1
 dumpr3 ; 1

 move.d 0xffff,r3
 add.w 0xffff,r3
 test_cc 1 0 0 1
 dumpr3 ; fffe

 move.d 0xfedaffff,r3
 add.w 0xffff,r3
 test_cc 1 0 0 1
 dumpr3 ; fedafffe

 move.d 0x78134452,r3
 add.w 0xf789,r3
 test_cc 0 0 0 1
 dumpr3 ; 78133bdb

 moveq -1,r3
 add.b 2,r3
 test_cc 0 0 0 1
 dumpr3 ; ffffff01

 moveq 2,r3
 add.b -1,r3
 test_cc 0 0 0 1
 dumpr3 ; 1

 move.d 0xff,r3
 add.b 0xff,r3
 test_cc 1 0 0 1
 dumpr3 ; fe

 move.d 0xfeda49ff,r3
 add.b 0xff,r3
 test_cc 1 0 0 1
 dumpr3 ; feda49fe

 move.d 0x78134452,r3
 add.b 0x89,r3
 test_cc 1 0 0 0
 dumpr3 ; 781344db

 quit
