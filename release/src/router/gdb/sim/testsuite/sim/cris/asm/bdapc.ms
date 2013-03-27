# mach: crisv0 crisv3 crisv8 crisv10
# output: 4455aa77\n4455aa77\nee19ccff\n88ccee19\nff22\n4455aa77\nff224455\n55aa77ff\n

 .include "testutils.inc"
 .data
x:
 .dword 0x55aa77ff
 .dword 0xccff2244
 .dword 0x88ccee19
 .dword 0xb232765a

 start
 moveq -1,r0
 moveq -1,r2
 move.d x-32768,r5
 move.d [r5+32769],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 4455aa77

 addu.w 32770,r5
 bdap.w -1,r5
 move.d [r0],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 4455aa77

 bdap.d 4,r5
 move.d [r2+],r3
 test_move_cc 1 0 0 0
 dumpr3 ; ee19ccff

 bdap.b 2,r2
 move.d [r3],r3
 test_move_cc 1 0 0 0
 dumpr3 ; 88ccee19

 bdap.b 3,r5
 movu.w [r4+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; ff22

 bdap.b -4,r4
 move.d [r6+],r3
 test_move_cc 0 0 0 0
 dumpr3 ; 4455aa77

 bdap.w 2,r6
 move.d [r3],r9
 test_move_cc 1 0 0 0
 dumpr3 ; ff224455

 add.d 76789885,r5
 bdap.d -76789887,r5
 move.d [r3],r9
 test_move_cc 0 0 0 0
 dumpr3 ; 55aa77ff

 quit
