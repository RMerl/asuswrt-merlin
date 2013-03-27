# mach: crisv32
# sim: --cris-900000xx --memory-region 0x90000000,0x10
# xerror:
# output: Seeing --cris-900000xx with memory defined there\n

; Check that I/O region overlap is detected.

 .include "nopv32t.ms"
