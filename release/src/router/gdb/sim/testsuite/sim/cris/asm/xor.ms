# mach: crisv0 crisv3 crisv8 crisv10 crisv32
# output: 3\n3\nff0\n0\n2c21b3db\n0\nffffffff\n

 .include "testutils.inc"
 start
 moveq 1,r3
 moveq 2,r4
 xor r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 3

 moveq 2,r3
 moveq 1,r4
 xor r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 3

 move.d 0xff0f,r4
 move.d 0xf0ff,r3
 xor r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; ff0

 moveq -1,r4
 move.d r4,r3
 xor r4,r3
 test_move_cc 0 1 0 0
 dumpr3 ; 0

 move.d 0x5432f789,r4
 move.d 0x78134452,r3
 xor r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 2c21b3db

 moveq 0,r4
 moveq 0,r3
 xor r4,r3
 test_move_cc 0 1 0 0
 dumpr3 ; 0

 move.d 0x7fffffff,r3
 move.d 0x80000000,r4
 xor r4,r3
 test_move_cc 1 0 0 0
 dumpr3 ; ffffffff
 quit
