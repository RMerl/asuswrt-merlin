# mach: crisv3 crisv8 crisv10 crisv32
# output: 0\nffffffff\nfffffffe\nffff\nff\n56788f9\n56788d9\n567889a\n0\n7ffffffc\n

 .include "testutils.inc"
 start
 moveq 1,r3
 subq 1,r3
 test_cc 0 1 0 0
 dumpr3 ; 0

 subq 1,r3
 test_cc 1 0 0 1
 dumpr3 ; ffffffff

 subq 1,r3
 test_cc 1 0 0 0
 dumpr3 ; fffffffe

 move.d 0x10000,r3
 subq 1,r3
 test_cc 0 0 0 0
 dumpr3 ; ffff

 move.d 0x100,r3
 subq 1,r3
 test_cc 0 0 0 0
 dumpr3 ; ff

 move.d 0x5678900,r3
 subq 7,r3
 test_cc 0 0 0 0
 dumpr3 ; 56788f9

 subq 32,r3
 test_cc 0 0 0 0
 dumpr3 ; 56788d9

 subq 63,r3
 test_cc 0 0 0 0
 dumpr3 ; 567889a

 move.d 34,r3
 subq 34,r3
 test_cc 0 1 0 0
 dumpr3 ; 0

 move.d 0x80000024,r3
 subq 40,r3
 test_cc 0 0 1 0
 dumpr3 ; 7ffffffc

 quit
