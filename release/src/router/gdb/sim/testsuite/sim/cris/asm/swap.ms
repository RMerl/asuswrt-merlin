# mach: crisv8 crisv10 crisv32
# output: 1ec8224a\n13785244\nc81e4a22\n44527813\n224a1ec8\n52441378\n4a22c81e\n87ecbbad\ne137ddb5\nec87adbb\n37e1b5dd\nbbad87ec\nddb5e137\nadbbec87\nb5dd37e1\n0\n

 .include "testutils.inc"
 start
 move.d 0x78134452,r4
 move.d r4,r3
 swapr r3
 test_move_cc 0 0 0 0
 dumpr3 ; 1ec8224a

 move.d r4,r3
 swapb r3
 test_move_cc 0 0 0 0
 dumpr3 ; 13785244

 move.d r4,r3
 swapbr r3
 test_move_cc 1 0 0 0
 dumpr3 ; c81e4a22

 move.d r4,r3
 swapw r3
 test_move_cc 0 0 0 0
 dumpr3 ; 44527813

 move.d r4,r3
 swapwr r3
 test_move_cc 0 0 0 0
 dumpr3 ; 224a1ec8

 move.d r4,r3
 swapwb r3
 test_move_cc 0 0 0 0
 dumpr3 ; 52441378

 move.d r4,r3
 swapwbr r3
 test_move_cc 0 0 0 0
 dumpr3 ; 4a22c81e

 move.d r4,r3
 swapn r3
 test_move_cc 1 0 0 0
 dumpr3 ; 87ecbbad

 move.d r4,r3
 swapnr r3
 test_move_cc 1 0 0 0
 dumpr3 ; e137ddb5

 move.d r4,r3
 swapnb r3
 test_move_cc 1 0 0 0
 dumpr3 ; ec87adbb

 move.d r4,r3
 swapnbr r3
 test_move_cc 0 0 0 0
 dumpr3 ; 37e1b5dd

 move.d r4,r3
 swapnw r3
 test_move_cc 1 0 0 0
 dumpr3 ; bbad87ec

 move.d r4,r3
 swapnwr r3
 test_move_cc 1 0 0 0
 dumpr3 ; ddb5e137

 move.d r4,r3
 swapnwb r3
 test_move_cc 1 0 0 0
 dumpr3 ; adbbec87

 move.d r4,r3
 swapnwbr r3
 test_move_cc 1 0 0 0
 dumpr3 ; b5dd37e1

 moveq -1,r3
 swapnwbr r3
 test_move_cc 0 1 0 0
 dumpr3 ; 0

 quit
