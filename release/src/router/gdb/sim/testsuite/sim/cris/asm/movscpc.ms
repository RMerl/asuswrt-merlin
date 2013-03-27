# mach: crisv3 crisv8 crisv10
# xerror:
# output: General register write to PC is not implemented.\nprogram stopped with signal 5.\n

 .include "testutils.inc"
 start
 movs.b 0x42,pc
 dumpr3

 movs.w 0x4321,pc
 dumpr3

 quit
