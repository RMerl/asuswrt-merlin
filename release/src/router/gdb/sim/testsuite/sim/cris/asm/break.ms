# mach: crisv3 crisv8 crisv10 crisv32
# sim: --trace-core=on
# ld: --section-start=.text=0
# output: read-2 exec:0x00000002 -> 0x05b0\nread-2 exec:0x00000004 -> 0xe93f\n

; First test: Must exit gracefully.

 .include "testutils.inc"

; This first insn isn't executed (it's a filler); it would fail
; ungracefully if executed.

 startnostack
 setf
 quit
