# mach: crisv0 crisv3 crisv8 crisv10 crisv32
# output: 1\n0\n80000000\n7fffffff\n2a\n1\nffff\n1f\n0\n

 .include "testutils.inc"
 start
 moveq -1,r3

 abs r3,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1

 moveq 0,r3
 dumpr3 ; 0

 move.d 0x80000000,r4
 abs r4,r3
 test_move_cc 1 0 0 0
 dumpr3 ; 80000000

 move.d 0x7fffffff,r4
 abs r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 7fffffff

 move.d 42,r3
 abs r3,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 2a

 moveq 1,r6
 abs r6,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1

 move.d 0xffff,r3
 abs r3,r3
 test_move_cc 0 0 0 0
 dumpr3 ; ffff

 moveq -31,r5
 abs r5,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1f

 moveq 0,r5
 abs r5,r3
 test_move_cc 0 1 0 0
 dumpr3 ; 0

 quit
