#mach: crisv10
#ld: --section-start=.text=0
#output: 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 0\n
#output: 4 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#output: 6 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#output: e 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 3\n
#output: 10 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#sim: --cris-trace=basic

; Check that "add.d x,pc" gets 3 cycles.

 .include "testutils.inc"
 startnostack
 nop
 nop
 add.d 1f-0f,$pc
0:
 nop
1:
 nop
 break 15
