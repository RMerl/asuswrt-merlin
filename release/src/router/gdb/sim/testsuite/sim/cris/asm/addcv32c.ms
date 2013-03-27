# mach: crisv32
# output: 0\n0\n1\n1\n2\n1ffff\nfffffffe\ncc463bdc\n

 .include "testutils.inc"
 start
 clearf cz
 moveq 0,r3
 addc 0,r3
 test_cc 0 0 0 0
 dumpr3 ; 0

 setf z
 moveq 0,r3
 addc 0,r3
 test_cc 0 1 0 0
 dumpr3 ; 0

 setf cz
 moveq 0,r3
 addc 0,r3
 test_cc 0 0 0 0
 dumpr3 ; 1

 clearf c
 moveq -1,r3
 addc 2,r3
 test_cc 0 0 0 1
 dumpr3 ; 1+c

 moveq 2,r3
 addc -1,r3
 test_cc 0 0 0 1
 dumpr3 ; 2+c

 move.d 0xffff,r3
 addc 0xffff,r3
 test_cc 0 0 0 0
 dumpr3 ; 1ffff

 moveq -1,r3
 addc -1,r3
 test_cc 1 0 0 1
 dumpr3 ; fffffffe+c

 move.d 0x78134452,r3
 addc 0x5432f789,r3
 test_cc 1 0 1 0
 dumpr3 ; cc463bdc

 quit
