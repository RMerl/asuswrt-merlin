# mach: crisv3 crisv8 crisv10
# xerror:
# output: General register write to PC is not implemented.\nprogram stopped with signal 5.\n

 .include "testutils.inc"
 start
 movu.w 0x4321,pc
 dumpr3

 quit
