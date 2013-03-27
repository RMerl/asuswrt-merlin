# mach: crisv32
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
 addo.d [r13+],r5,acr
 move.d [acr],r3
 dumpr3 ; 4455aa77

 addu.w 32770,r5
 addo.w [r13+],r5,acr
 move.d [acr],r3
 dumpr3 ; 4455aa77

 addo.d [r13],acr,acr
 addq 4,r13
 move.d [acr],r3
 dumpr3 ; ee19ccff

 addo.b [r13+],r5,acr
 movu.w [acr],r3
 dumpr3 ; ff22

 addo.b [r13],acr,acr
 addq 1,r13
 move.d [acr],r3
 dumpr3 ; 4455aa77

 addo.w [r13],acr,acr
 addq 2,r13
 move.d [acr],r3
 dumpr3 ; ff224455

 addo.d [r13+],r5,acr
 add.d 76789885,acr
 move.d [acr],r3
 dumpr3 ; 55aa77ff

 quit
