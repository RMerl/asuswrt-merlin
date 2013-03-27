# mach: crisv10
# output: ff\nff\n0\n0\n80\n40\n20\n10\n8\n4\n2\n1\n80\n40\n20\n10\n8\n4\n2\n1\n42\n

; Check that flag settings affect ccr and dccr and vice versa.

 .include "testutils.inc"
 start
 clear.d r3
 setf mbixnzvc
 move ccr,r3
 dumpr3

 clear.d r3
 setf mbixnzvc
 move dccr,r3
 dumpr3

 clear.d r3
 clearf mbixnzvc
 move ccr,r3
 dumpr3

 clear.d r3
 clearf mbixnzvc
 move dccr,r3
 dumpr3

 .macro testfr BIT REG
 clear.d r3
 clearf mbixnzvc
 setf \BIT
 move \REG,r3
 dumpr3
 .endm

 testfr m ccr
 testfr b ccr
 testfr i ccr
 testfr x ccr
 testfr n ccr
 testfr z ccr
 testfr v ccr
 testfr c ccr

 testfr m dccr
 testfr b dccr
 testfr i dccr
 testfr x dccr
 testfr n dccr
 testfr z dccr
 testfr v dccr
 testfr c dccr

; Check only the nzvc bits; do the other bits in special tests as they're
; implemented.
 .macro test_get_cc N Z V C
 clearf znvc
 move ((\N << 3)|(\Z << 2)|(\V << 1)|\C),ccr
 test_cc \N \Z \V \C
 setf znvc
 move ((\N << 3)|(\Z << 2)|(\V << 1)|\C),dccr
 test_cc \N \Z \V \C
 move.d ((\N << 3)|(\Z << 2)|(\V << 1)|\C),r4
 setf znvc
 move r4,ccr
 test_cc \N \Z \V \C
 clearf znvc
 move r4,dccr
 test_cc \N \Z \V \C
 .endm

 test_get_cc 1 0 0 0 
 test_get_cc 0 1 0 0 
 test_get_cc 0 0 1 0 
 test_get_cc 0 0 0 1 

 move.d 0x42,r3
 dumpr3
 quit
