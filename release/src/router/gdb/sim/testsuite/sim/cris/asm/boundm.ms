# mach: crisv0 crisv3 crisv8 crisv10
# output: 2\n2\nffff\nffffffff\n5432f789\n2\nffff\n2\nffff\nffff\nf789\n2\n2\nff\nff\nff\n89\n0\n

 .include "testutils.inc"
 .data
x:
 .dword 2,-1,0xffff,-1,0x5432f789
 .word 2,0xffff,0xf789
 .byte 2,0xff,0x89,0

 start
 move.d x,r5

 moveq -1,r3
 moveq 2,r4
 bound.d [r5+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 2

 moveq 2,r3
 bound.d [r5],r3
 test_move_cc 0 0 0 0
 addq 4,r5
 dumpr3 ; 2

 move.d 0xffff,r3
 bound.d [r5+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; ffff

 moveq -1,r3
 bound.d [r5+],r3
 test_move_cc 1 0 0 0
 dumpr3 ; ffffffff

 move.d 0x78134452,r3
 bound.d [r5+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 5432f789

 moveq -1,r3
 bound.w [r5+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 2

 moveq -1,r3
 bound.w [r5],r3
 test_move_cc 0 0 0 0
 dumpr3 ; ffff

 moveq 2,r3
 bound.w [r5],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 2

 move.d 0xffff,r3
 bound.w [r5],r3
 test_move_cc 0 0 0 0
 dumpr3 ; ffff

 move.d 0xfedaffff,r3
 bound.w [r5+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; ffff

 move.d 0x78134452,r3
 bound.w [r5+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; f789

 moveq -1,r3
 bound.b [r5+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 2

 moveq 2,r3
 bound.b [r5],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 2

 moveq -1,r3
 bound.b [r5],r3
 test_move_cc 0 0 0 0
 dumpr3 ; ff

 move.d 0xff,r3
 bound.b [r5],r3
 test_move_cc 0 0 0 0
 dumpr3 ; ff

 move.d 0xfeda49ff,r3
 bound.b [r5+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; ff

 move.d 0x78134452,r3
 bound.b [r5+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 89

 bound.b [r5],r3
 test_move_cc 0 1 0 0
 dumpr3 ; 0

 quit
