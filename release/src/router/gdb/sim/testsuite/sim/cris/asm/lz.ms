# mach: crisv0 crisv3 crisv8 crisv10 crisv32
# output: 0\n20\n0\n1\n1\n1a\n1f\n10\n1e\n

 .include "testutils.inc"
 start
 moveq -1,r3

 lz r3,r3
 test_move_cc 0 1 0 0
 dumpr3 ; 0

 moveq 0,r3
 lz r3,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 20

 move.d 0x80000000,r4
 lz r4,r3
 test_move_cc 0 1 0 0
 dumpr3 ; 0

 move.d 0x40000000,r4
 lz r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1

 move.d 0x7fffffff,r4
 lz r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1

 move.d 42,r3
 lz r3,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1a

 moveq 1,r6
 lz r6,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1f

 move.d 0xffff,r3
 lz r3,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 10

 moveq 2,r5
 lz r5,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1e

 quit
