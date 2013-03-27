# mach: crisv0 crisv3 crisv8 crisv10
# output: 4455aa77\n4455aa77\nee19ccff\nff22\n4455aa77\nff224455\n55aa77ff\n

 .include "testutils.inc"
 .data
x:
 .dword 0x55aa77ff
 .dword 0xccff2244
 .dword 0x88ccee19

 start
 moveq -1,r0
 move.d x-32768,r5
 move.d 32769,r6
 move.d [r5+r6.b],r3
 test_cc 0 0 0 0
 dumpr3 ; 4455aa77

 addu.w 32771,r5
 moveq -1,r8
 move.d [r11=r5+r8.w],r3
 test_cc 0 0 0 0
 dumpr3 ; 4455aa77

 moveq 5,r10
 move.d [r11+r10.b],r3
 test_cc 1 0 0 0
 dumpr3 ; ee19ccff

 subq 1,r5
 move.d r5,r8
 subq 1,r8
 moveq 1,r9
 movu.w [r12=r8+r9.d],r3
 test_cc 0 0 0 0
 dumpr3 ; ff22

 moveq -2,r11
 move.d [r13=r12+r11.w],r3
 test_cc 0 0 0 0
 dumpr3 ; 4455aa77

 subq 18,r13
 moveq 5,r9
 move.d [r13+r9.d],r3
 test_cc 1 0 0 0
 dumpr3 ; ff224455

 move.d r5,r7
 add.d 76789886,r7
 move.d -76789888/4,r12
 move.d [r7+r12.d],r3
 test_cc 0 0 0 0
 dumpr3 ; 55aa77ff

 quit
