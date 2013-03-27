#mach: crisv10
#ld: --section-start=.text=0
#output: 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 0\n
#output: 4 0 0 0 0 0 0 0 0 0 0 0 0 0 0 * ixnzvc 1\n
#output: a 0 0 0 0 0 ff004567 0 0 0 0 0 0 0 0 * ixNzvc 2\n
#sim: --cris-trace=basic

; With a "--cris-trace=all", cycles for the last line would be 3.

 .include "movect10.ms"
