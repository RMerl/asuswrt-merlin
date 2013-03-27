 .include "../../asm/testutils.inc"

# Define an exception vector table "vecname" with a single
# vector number "n" as "entry", all others "other".
# V32 only needs 1<<10 alignment, earlier versions need 1<<16.
 .macro singlevec vecname vecno entry other=killme
 .section .text.exvec
 .p2align 16
\vecname:
 .if (\vecno)
 .rept \vecno
 .dword \other
 .endr
 .endif
 .dword \entry
 .if (\vecno)-255
 .rept 256-(\vecno)-1
 .dword \other
 .endr
 .endif
 .previous
 .endm
