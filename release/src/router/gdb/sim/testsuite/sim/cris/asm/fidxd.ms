# mach: crisv32
# xerror:
# output: FIDXD isn't implemented\nprogram stopped with signal 5.\n

 .include "testutils.inc"
 start
 fidxd [r3]

 quit
