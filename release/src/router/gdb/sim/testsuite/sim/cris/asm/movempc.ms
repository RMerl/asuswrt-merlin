# mach: crisv3 crisv8 crisv10
# xerror:
# output: General register write to PC is not implemented.\nprogram stopped with signal 5.\n

 .include "testutils.inc"
 start
 move.d _start,r12
 move.d [r12],pc
