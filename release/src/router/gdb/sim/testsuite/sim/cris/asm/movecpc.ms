# mach: crisv3 crisv8 crisv10
# xerror:
# output: General register * PC is not implemented.\nprogram stopped with signal 5.\n

# We deliberately match both "read from" and "write to" above.

 .include "testutils.inc"
 startnostack
 moveq -1,r3
 move.b 0x42,pc
 dumpr3

 move.w 0x4321,pc
 dumpr3

 move.d 0x76543210,pc
 dumpr3

 quit
