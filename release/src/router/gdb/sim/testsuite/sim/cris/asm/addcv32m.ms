# mach: crisv32
# output: 0\n0\n1\n0\n1\n1\n2\n1ffff\nfffffffe\ncc463bdc\n

 .include "testutils.inc"
 .data
x:
 .dword 0,0,2,-1,0xffff,-1,0x5432f789

 start
 move.d x,r5
 clearf cz
 moveq 0,r3
 addc [r5],r3
 test_cc 0 0 0 0
 dumpr3 ; 0

 setf z
 moveq 0,r3
 addc [r5],r3
 test_cc 0 1 0 0
 dumpr3 ; 0

 setf c
 moveq 0,r3
 addc [r5],r3
 test_cc 0 0 0 0
 dumpr3 ; 1

 clearf c
 moveq 0,r3
 addc [r5+],r3
 test_cc 0 0 0 0
 dumpr3 ; 0

 setf c
 moveq 0,r3
 addc [r5+],r3
 test_cc 0 0 0 0
 dumpr3 ; 1

 clearf c
 moveq -1,r3
 addc [r5+],r3
 test_cc 0 0 0 1
 dumpr3 ; 1+c

 moveq 2,r3
 addc [r5],r3
 moveq 4,r6
 addi r6.b,r5
 test_cc 0 0 0 1
 dumpr3 ; 2+c

 move.d 0xffff,r3
 addc [r5+],r3
 test_cc 0 0 0 0
 dumpr3 ; 1ffff

 moveq -1,r3
 addc [r5+],r3
 test_cc 1 0 0 1
 dumpr3 ; fffffffe+c

 move.d 0x78134452,r3
 addc [r5+],r3
 test_cc 1 0 1 0
 dumpr3 ; cc463bdc

 quit
