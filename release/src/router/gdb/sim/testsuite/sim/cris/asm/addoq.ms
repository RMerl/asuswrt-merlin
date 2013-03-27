# mach: crisv32
# output: ccff2244\n88ccee19\n55aa77ff\n19cc\n

 .include "testutils.inc"
 .data
x:
 .dword 0x55aa77ff
 .dword 0xccff2244
 .dword 0x88ccee19
 start
 moveq -1,r0
 move.d x+4,r5
 setf zvnc
 addoq 0,r5,acr
 test_cc 1 1 1 1
 move.d [acr],r3
 dumpr3 ; ccff2244
 setf zvnc
 addoq 4,r5,acr
 test_cc 1 1 1 1
 move.d [acr],r3
 dumpr3 ; 88ccee19
 clearf zvnc
 addoq -8,acr,acr
 test_cc 0 0 0 0
 move.d [acr],r3
 dumpr3 ; 55aa77ff
 addoq 3,r5,acr
 movu.w [acr],r3
 dumpr3 ; 19cc
 quit
