#mach: crisv10
#ld: --section-start=.text=0
#output: 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 0\n
#output: 4 0 0 0 0 0 0 0 0 0 1 0 0 0 0 * ixnzvc 1\n
#output: 10 0 0 0 0 0 0 0 0 0 1 0 0 0 0 * ixnzvc 4\n
#output: 12 0 0 0 0 0 0 0 0 0 1 0 0 0 0 * ixnzvc 1\n
#sim: --cris-trace=basic

; Check that "adds.w [$pc+$r9.w],$pc" gets 4 cycles.

 .include "testutils.inc"
 startnostack
 moveq 1,r9
 adds.w [$pc+$r9.w],$pc
0:
 .word 1f-0b
 .word 2f-0b
 .word 1f-0b
1:
 break 15
2:
 nop
 break 15
