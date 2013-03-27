# mach: crisv8 crisv10
# output: fffffffe\n
# output: ffffffff\n
# output: fffffffe\n
# output: 1\n

; Check that carry is cleared on v8, v10.

 .include "testutils.inc"
 start
 moveq -1,r3
 moveq 2,r4
 setf c
 muls.d r4,r3
 test_cc 1 0 0 0
 dumpr3 ; fffffffe
 move mof,r3
 dumpr3 ; ffffffff

 moveq -1,r3
 moveq 2,r4
 setf c
 mulu.d r4,r3
 test_cc 0 0 1 0
 dumpr3 ; fffffffe
 move mof,r3
 dumpr3 ; 1

 quit
