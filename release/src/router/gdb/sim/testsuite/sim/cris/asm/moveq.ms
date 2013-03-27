# mach: crisv3 crisv8 crisv10 crisv32
# sim: --trace-core=on
# ld: --section-start=.text=0
# output: read-2 exec:0x00000002 -> 0x3262\nread-2 exec:0x00000004 -> 0xe93e\nffffffe2\nread-2 exec:0x00000006 -> 0x324d\nread-2 exec:0x00000008 -> 0xe93e\nd\nread-2 exec:0x0000000a -> 0xe93f\n

; Output a positive and a negative number, set from moveq.

 .include "testutils.inc"
 startnostack
 moveq -30,r3
 dumpr3
 moveq 13,r3
 dumpr3
 quit

