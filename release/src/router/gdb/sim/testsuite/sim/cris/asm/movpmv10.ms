# mach: crisv10
# output: 1122330a\nbb113344\naa557711\n

# Test v10-specific special registers.  FIXME: ccr, irp, bar, brp, usp.

 .include "testutils.inc"
 start
 .data
store:
 .dword 0x11223344
 .dword 0x77665544

 .text
 moveq -1,r3
 move.d store,r4
 clearf zcvn
 move vr,[r4]
 test_cc 0 0 0 0
 move [r4+],mof
 move mof,r3
 dumpr3

 moveq -1,r3
 move 0xbb113344,mof
 move mof,r3
 dumpr3

 move 0xaa557711,mof
 setf zcvn
 move mof,[r4]
 test_cc 1 1 1 1
 move.d [r4],r3
 dumpr3
 
 quit
