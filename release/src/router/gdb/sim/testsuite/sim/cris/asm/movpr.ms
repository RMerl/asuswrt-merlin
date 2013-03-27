# mach: crisv3 crisv8 crisv10 crisv32
# output: ffffff00\nffff0000\n0\nbb113344\n

# Test generic "move Ps,Rd" and "move Rs,Pd" insns; the ones with
# functionality common to all models.

 .include "testutils.inc"
 start
 moveq -1,r3
 clear.b r3
 dumpr3

 moveq -1,r3
 clear.w r3
 dumpr3

 moveq -1,r3
 clear.d r3
 dumpr3

 moveq -1,r3
 move.d 0xbb113344,r4
 setf zcvn
 move r4,srp
 move srp,r3
 test_cc 1 1 1 1
 dumpr3
 quit
