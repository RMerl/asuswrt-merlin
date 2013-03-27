# mach: crisv0 crisv3 crisv8 crisv10
# output: ccff2244\n88ccee19\n55aa77ff\n19cc\n0\n

 .include "testutils.inc"
 .data
x:
 .dword 0x55aa77ff
 .dword 0xccff2244
 .dword 0x88ccee19
 .dword 0
 start
 moveq -1,r0
 move.d x+4,r5
 move.d [r5+0],r3
 test_move_cc 1 0 0 0
 dumpr3 ; ccff2244
 move.d [r5=r5+4],r3
 test_move_cc 1 0 0 0
 dumpr3 ; 88ccee19
 move.d [r5=r5-8],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 55aa77ff
 movu.w [r5+7],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 19cc
 move.d [r5+12],r3
 test_move_cc 0 1 0 0
 dumpr3 ; 0
 quit
