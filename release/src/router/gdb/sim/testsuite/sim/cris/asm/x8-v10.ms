#mach: crisv10
#ld: --section-start=.text=0
#output: 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 0\n
#output: 8 0 0 0 0 0 10 0 0 0 0 0 0 0 0 * ixnzvc 2\n
#output: c 0 0 0 0 0 10 0 0 0 0 0 0 0 0 * ixnzvc 2\n
#output: e 0 0 0 0 0 10 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#sim: --cris-trace=basic

; Check that "jump [rN]" gets 2 cycles.

 .include "testutils.inc"
 startnostack
 move.d 0f,r5
 jump [r5]
 break 15
1:
 nop
 break 15
0:
 .dword 1b
