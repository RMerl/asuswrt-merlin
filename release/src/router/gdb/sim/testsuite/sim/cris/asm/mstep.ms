# mach: crisv3 crisv8 crisv10
#output: fffffffe\n
#output: 3\n
#output: 1fffe\n
#output: 2fffd\n
#output: fffffffd\n
#output: ffffffff\n
#output: f02688a4\n
#output: 1fffe\n
#output: fffffffe\n
#output: fffffffe\n
#output: fffffff9\n
#output: 0\n
#output: 4459802d\n
#output: 4459802d\n

 .include "testutils.inc"
 start
 moveq -1,r3
 moveq 2,r4
 mstep r4,r3
 test_move_cc 1 0 0 0
 dumpr3 ; fffffffe

 moveq 2,r3
 moveq -1,r4
 mstep r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 3

 move.d 0xffff,r4
 move.d r4,r3
 mstep r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1fffe

 move.d 0xffff,r4
 move.d r4,r3
 setf n
 mstep r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 2fffd

 moveq -1,r4
 move.d r4,r3
 mstep r4,r3
 test_move_cc 1 0 0 0
 dumpr3 ; fffffffd

 moveq -1,r3
 moveq 1,r4
 setf n
 mstep r4,r3
 test_move_cc 1 0 0 0
 dumpr3 ; ffffffff

 move.d 0x5432f789,r4
 move.d 0x78134452,r3
 mstep r4,r3
 test_move_cc 1 0 0 0
 dumpr3 ; f02688a4

 move.d 0xffff,r3
 move.d 0x1fffe,r4
 mstep r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1fffe

 move.d 0x7fffffff,r3
 moveq 5,r5
 mstep r5,r3
 test_move_cc 1 0 0 0
 dumpr3 ; fffffffe

 move.d 0x7fffffff,r3
 moveq 0,r5
 mstep r5,r3
 test_move_cc 1 0 0 0
 dumpr3 ; fffffffe

 move.d 0x7fffffff,r3
 moveq -5,r5
 mstep r5,r3
 test_move_cc 1 0 0 0
 dumpr3 ; fffffff9

 move.d 0x7fffffff,r3
 moveq 2,r5
 setf n
 mstep r5,r3
 test_move_cc 0 1 0 0
 dumpr3 ; 0

 move.d 0x5432f789,r4
 move.d 0x78134452,r3
 setf n
 mstep r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 4459802d

 move.d 0x5432f789,r4
 move.d 0x78134452,r3
 setf nc
 mstep r4,r3
 test_move_cc 0 0 0 0
 dumpr3 ; 4459802d

 quit
