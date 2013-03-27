# mach: crisv32
# output: 0\n0\n4\n42\n
# sim: --cris-naked --target binary --architecture crisv32 
# ld: --oformat binary

; Check that we can run a naked binary with the same expected
; results as an ELF "executable".

 .include "bare1.ms"
