#mach: crisv10
#ld: --section-start=.text=0
#output: 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 0\n
#output: 4 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#output: a 0 0 0 0 0 12 0 0 0 0 0 0 0 0 * ixnzvc 2\n
#output: 12 0 0 0 0 0 12 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#output: 1e 0 0 0 0 0 12 0 0 0 0 0 0 0 0 * ixnzvc 2\n
#sim: --cris-trace=basic

; With a "--cris-trace=all", cycles for the third line would be 3.

 .include "tjsrcv10.ms"
