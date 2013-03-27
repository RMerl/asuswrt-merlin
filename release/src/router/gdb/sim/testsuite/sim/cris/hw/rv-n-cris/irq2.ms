#mach: crisv10 crisv32
#sim(crisv10): --hw-device "/rv/trace? true" --hw-device "/rv/intmultiple 0xaa"
#sim(crisv32): --hw-device "/rv/trace? true" --hw-device "/rv/intmultiple 0xaa"
#output: /rv: WD\n
#output: /rv: REG R 0xd0000032\n
#output: /rv: := 0xabcdef01\n
#output: /rv: IRQ 0xaa\n
#output: /rv: REG R 0xd0000036\n
#output: /rv: := 0x76543210\n
#output: /rv: REG R 0xd0000030\n
#output: /rv: IRQ 0x0\n
#output: /rv: := 0xeeff4455\n
#output: pass\n

# Primarily to test multiple-int-bits set in dv-rv.c.

#r W,
#r r,a8832,abcdef01
#r I,6
#r r,a8836,76543210
#r I,0
#r r,a8830,eeff4455

 .lcomm dummy,4

 .include "testutils.inc"
 start
 test_h_mem 0xabcdef01 0xd0000032
 .if ..asm.arch.cris.v32
 move irqvec1,$ebp
 .else
 move irqvec1,$ibr
 .endif
 ei
 test_h_mem 0,dummy
killme:
 fail

irq0xea:
 test_h_mem 0x76543210 0xd0000036
 test_h_mem 0xeeff4455 0xd0000030
 pass

 singlevec irqvec1,0xea,irq0xea
