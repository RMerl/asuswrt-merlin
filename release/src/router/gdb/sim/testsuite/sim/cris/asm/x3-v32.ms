#mach: crisv32
#ld: --section-start=.text=0
#output: 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 0 0\n
#output: 4 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1 0\n
#output: a 0 0 0 0 0 12 0 0 0 0 0 0 0 0 * ixnzvc 1 0\n
#output: 12 0 0 0 0 0 12 0 0 0 0 0 0 0 0 * ixnzvc 1 0\n
#output: 1e 0 0 0 0 0 12 0 0 0 0 0 0 0 0 * ixnzvc 1 0\n
#sim: --cris-trace=basic

 .include "tjsrcv10.ms"
