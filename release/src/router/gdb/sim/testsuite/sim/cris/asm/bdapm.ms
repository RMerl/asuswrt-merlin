# mach: crisv0 crisv3 crisv8 crisv10
# output: 4455aa77\n4455aa77\nee19ccff\nff22\n4455aa77\nff224455\n55aa77ff\n

 .include "testutils.inc"
 .data
x:
 .dword 0x55aa77ff
 .dword 0xccff2244
 .dword 0x88ccee19
y:
 .dword 32769
 .word -1
 .dword 5
 .byte 3,-4
 .word 2
 .dword -76789887

 start
 moveq -1,r0
 move.d x-32768,r5
 move.d y,r13
 bdap.d [r13+],r5
 move.d [r3],r9
 test_move_cc 0 0 0 0
 dumpr3 ; 4455aa77

 addu.w 32770,r5
 bdap.w [r13+],r5
 move.d [r9+],r3
 dumpr3 ; 4455aa77

 bdap.d [r13],r9
 move.d [r3],r7
 addq 4,r13
 dumpr3 ; ee19ccff

 bdap.b [r13+],r5
 movu.w [r7+],r3
 dumpr3 ; ff22

 bdap.b [r13],r7
 move.d [r7+],r3
 addq 1,r13
 dumpr3 ; 4455aa77

 bdap.w [r13],r7
 move.d [r3],r3
 addq 2,r13
 dumpr3 ; ff224455

 add.d 76789885,r5
 bdap.d [r13+],r5
 move.d [r3],r9
 dumpr3 ; 55aa77ff

 quit
