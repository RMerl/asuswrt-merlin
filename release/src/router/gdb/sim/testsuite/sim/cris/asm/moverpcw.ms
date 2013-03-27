# mach: crisv3 crisv8 crisv10
# xerror: 
# output: General register read of PC is not implemented.\nprogram stopped with signal 5.\n

 .include "testutils.inc"
 startnostack
 setf
 move.w pc,r2
 quit
