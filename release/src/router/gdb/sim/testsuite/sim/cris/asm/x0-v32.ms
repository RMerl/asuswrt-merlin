#mach: crisv32
#ld: --section-start=.text=0
#output: 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 3dfff[c-f][0-9a-f][0-9a-f] ixnzvc 0 0\n
#output: 4 0 0 0 0 0 0 0 0 0 0 0 0 0 0 3dfff[c-f][0-9a-f][0-9a-f] ixnzvc 1 0\n
#sim: --cris-trace=basic

 .include "break.ms"
