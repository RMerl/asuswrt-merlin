#mach: crisv32
#sim(crisv32): --hw-info
#output: /\n
#output: /rv\n
#output: /rv/reg 0xd0000000 0x40\n
#output: /rv/remote-reg 0xa8800\n
#output: /rv/intnum 0x4 0x2\n
#output: /rv/mem 0x20000 0x400\n
#output: /rv/remote-mem 0xe000\n
#output: /rv/mbox 0xc000f000\n
#output: /rv > int int /cris\n
#output: /cris\n
#output: /cris/vec-for-int 0x4 0x33 0x8 0x34 0xaa 0xea\n

# Test expected --hw-info output and startup paths of components.

 .include "quit.s"
