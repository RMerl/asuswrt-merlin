# mach: crisv10
# output: ffffff0a\nbb113344\n

# Test v10-specific special registers.  FIXME: ccr, irp, bar, brp, usp.

 .include "testutils.inc"
 start
 moveq -1,r3
 setf zcvn
 move vr,r3
 test_cc 1 1 1 1
 dumpr3

 moveq -1,r3
 move.d 0xbb113344,r4
 clearf zcvn
 move r4,mof
 move mof,r3
 test_cc 0 0 0 0
 dumpr3
 quit
