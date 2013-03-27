# mach: crisv32
# output: bf\n0\n80\n20\n10\n8\n4\n2\n1\n40\nfade040\n3ade0040\nfade040\n42\n

; Check flag settings.

 .include "testutils.inc"
 start
 clear.d r3
 setf pixnzvc	; Setting U(ser mode) would restrict tests of other flags.
 move ccs,r3
 dumpr3

 clear.d r3
 clearf puixnzvc
 move ccs,r3
 dumpr3

 .macro testf BIT
 clear.d r3
 clearf puixnzvc
 setf \BIT
 move ccs,r3
 dumpr3
 .endm

 testf p
 testf i
 testf x
 testf n
 testf z
 testf v
 testf c
 testf u	; Can't test i-flag or clear u after this point.

 .macro test_get_cc N Z V C
 clearf znvc
 move ((\N << 3)|(\Z << 2)|(\V << 1)|\C),ccs
 test_cc \N \Z \V \C
 setf znvc
 move ((\N << 3)|(\Z << 2)|(\V << 1)|\C),ccs
 test_cc \N \Z \V \C
 move.d ((\N << 3)|(\Z << 2)|(\V << 1)|\C),r4
 setf znvc
 move r4,ccs
 test_cc \N \Z \V \C
 clearf znvc
 move r4,ccs
 test_cc \N \Z \V \C
 .endm

 test_get_cc 1 0 0 0
 test_get_cc 0 1 0 0
 test_get_cc 0 0 1 0
 test_get_cc 0 0 0 1

; Test that the U bit sticks.
 move 0x0fade000,ccs
 move ccs,r3
 dumpr3

; Check that the M and Q bits can't be set in user mode.
 move 0xfade0000,ccs
 move ccs,r3
 dumpr3

 move 0x0fade000,ccs
 move ccs,r3
 dumpr3

 move.d 0x42,r3
 dumpr3

 quit
