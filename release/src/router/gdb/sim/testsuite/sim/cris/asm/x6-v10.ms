#mach: crisv10
#ld: --section-start=.text=0
#sim: --cris-trace=basic
#output: 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 0\n
#output: 4 0 0 0 1 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#output: 6 0 2 0 1 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#output: 8 1 2 0 1 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#output: a 1 2 0 1 0 0 0 0 0 0 0 0 0 0 * ixnzvc 2\n
#output: c 1 2 0 1 0 0 0 0 0 0 0 0 0 0 * ixnzvc 2\n
#output: e 1 2 0 2 0 0 0 0 0 0 0 0 0 0 * ixnzvc 2\n
 .include "tmulv10.ms"
