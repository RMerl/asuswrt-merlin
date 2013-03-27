# mach: crisv0 crisv3 crisv8 crisv10
# output: 4455aa77\nee19ccff\nb232765a\nff22\n5a88ccee\n

 .include "testutils.inc"
 .data
x:
 .dword 0x55aa77ff
 .dword 0xccff2244
 .dword 0x88ccee19
 .dword 0xb232765a
y:
 .dword x+12
 .dword x+5
 .dword x+9

 start
 moveq -1,r0
 moveq -1,r2
 move.d [x+1],r3
 test_cc 0 0 0 0
 dumpr3 ; 4455aa77

 move.d [x+6],r3
 test_cc 1 0 0 0
 dumpr3 ; ee19ccff

 move.d y,r8
 move.d [[r8+]],r3
 test_cc 1 0 0 0
 dumpr3 ; b232765a

 movu.w [[r8]],r3
 test_cc 0 0 0 0
 dumpr3 ; ff22
 addq 4,r8

 move.d [[r8]],r3
 test_cc 0 0 0 0
 dumpr3 ; 5a88ccee

 quit
