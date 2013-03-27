# mach: crisv3 crisv8 crisv10

# Make sure that "move [$sp=$sp+16],$p8" works; used in Linux.

 .include "testutils.inc"
 startnostack
 move.d x,$sp
 moveq 0,$r3
 move [$sp=$sp+16],$p8
 ; Z not changed.
 bne 0f
 nop
 cmp.d x+16,$sp
 bne 0f
 nop
 move $p8,$r3
 ; Z not changed.
 bne 0f
 ; P8 still 0.
 test.d $r3
 bne 0f
 nop
 pass
0:
 fail

 .data
x:
 .dword 0xffffffff
 .dword 0xffffffff
 .dword 0xffffffff
 .dword 0xffffffff
 .dword 0xffffffff
