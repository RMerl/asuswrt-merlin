#mach: crisv32
#ld: --section-start=.text=0
#sim: --cris-trace=basic
#output: 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 0 0\n
#output: 8 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixNzvc 1 aa424243\n
#output: a 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1 55212121\n
#output: c 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1 1\n
#output: e 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1 0\n

; Check that trace with changing ACR works.

 .include "testutils.inc"
 startnostack
 move.d 0xaa424243,$acr
 lsrq 1,$acr
 moveq 1,$acr
 clear.d $acr
 break 15
 nop
