# mach: crisv0 crisv3 crisv8 crisv10
# output: ee19cd0b\nee197761\nccff2244\n55aa77ff\nffffaa77\naa\n4243ab11\n424377ab\nfdedaaf0\n4242dd68\n4242dd68\n40025567\n57eb77ff\n55aa77ff\n

 .include "testutils.inc"
 .data
x:
 .dword 0x55aa77ff
 .dword 0xccff2244
 .dword 0x88ccee19

 start
 move.d x,r10
 moveq 0,r3
 moveq 12,r4
 add.d [r10+6],r4,r3
 test_cc 1 0 0 0
 dumpr3 ; ee19cd0b

 move.d 0x1267,r7
 subu.w [r10+2],r3,r8
 test_cc 1 0 0 0
 move.d r8,r3
 dumpr3 ; ee197761

 moveq 1,r8
 bound.d [r10+r8.d],r3,r5
 test_move_cc 1 0 0 0
 move.d r5,r3
 dumpr3 ; ccff2244

; Also applies to move insns.  Bleah.
 moveq 0,r5
 bdap 0,r10
 move.d [r3],r5
 test_move_cc 0 0 0 0
 dumpr3 ; 55aa77ff

 moveq 0,r5
 bdap 1,r10
 movs.w [r3],r5
 test_move_cc 1 0 0 0
 dumpr3 ; ffffaa77

 moveq 0,r5
 bdap 2,r10
 test_move_cc 0 0 0 0
 movu.b [r3],r5
 dumpr3 ; aa

 move.d 0x42435567,r8
 bdap 2,r10
 adds.w [r3],r8
 test_cc 0 0 0 0
 dumpr3 ; 4243ab11

 move.d 0x42435567,r8
 bdap 4,r10
 addu.w [r3],r8
 test_cc 0 0 0 0
 dumpr3 ; 424377ab

 move.d 0x42435567,r8
 bdap 1,r10
 sub.d [r3],r8
 test_cc 1 0 0 1
 dumpr3 ; fdedaaf0

 move.d 0x42435567,r8
 bdap 0,r10
 subs.w [r3],r8
 test_cc 0 0 0 0
 dumpr3 ; 4242dd68

 move.d 0x42435567,r8
 bdap 0,r10
 subu.w [r3],r8
 test_cc 0 0 0 0
 dumpr3 ; 4242dd68

 move.d 0x42435567,r8
 bdap 0,r10
 and.d [r3],r8
 test_move_cc 0 0 0 0
 dumpr3 ; 40025567

 move.d 0x42435567,r8
 bdap 0,r10
 or.d [r3],r8
 test_move_cc 0 0 0 0
 dumpr3 ; 57eb77ff

 move.d 0xc2435567,r8
 bdap 0,r10
 bound.d [r3],r8
 test_move_cc 0 0 0 0
 dumpr3 ; 55aa77ff

 quit
