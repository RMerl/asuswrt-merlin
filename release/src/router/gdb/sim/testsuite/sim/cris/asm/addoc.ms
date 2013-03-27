# mach: crisv32
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
 addo.d 32769,r5,acr
 move.d [acr],r3
 dumpr3 ; 4455aa77

 addu.w 32770,r5
 addo.w -1,r5,acr
 move.d [acr],r3
 dumpr3 ; 4455aa77

 addo.d 5,acr,acr
 move.d [acr],r3
 dumpr3 ; ee19ccff

 addo.b 3,r5,acr
 movu.w [acr],r3
 dumpr3 ; ff22

 addo.b -4,acr,acr
 move.d [acr],r3
 dumpr3 ; 4455aa77

 addo.w 2,acr,acr
 move.d [acr],r3
 dumpr3 ; ff224455

 addo.d -76789887,r5,acr
 add.d 76789885,acr
 move.d [acr],r3
 dumpr3 ; 55aa77ff

 quit
