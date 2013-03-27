#mach: crisv10
#ld: --section-start=.text=0
#output: 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 0\n
#output: 4 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#output: c 0 0 0 24 0 0 0 0 0 0 0 0 0 0 * ixnzvc 4\n
#output: e 0 0 0 24 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#output: 10 0 0 0 24 0 0 0 0 0 0 0 0 0 0 * ixnZvc 1\n
#output: 14 0 0 0 24 0 24 0 0 0 0 0 0 0 0 * ixnzvc 3\n
#output: 18 0 0 0 24 0 24 0 0 0 0 0 0 0 0 * ixnzvc 3\n
#output: 20 0 0 0 24 0 24 0 0 0 0 0 0 0 0 * ixnzvc 4\n
#sim: --cris-trace=basic

; With a "--cris-trace=all", cycles for the third and last line would be 5.

; Check that prefix+insn are traced as one.

 .include "testutils.inc"
 startnostack
 nop
 move.d [0f],r3
 nop
 moveq 0,r4
 move.d [r3+r4.b],r5
 move.d [r3+4],r5
 bdap.d 0,r3
 move.d [r3],r5
 break 15
 .p2align 2
0:
 .dword 0b
 .dword 0b
