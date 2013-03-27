#mach: crisv10 crisv32
#sim(crisv10): --hw-device "/rv/trace? true"
#sim(crisv32): --hw-device "/rv/trace? true"
#output: /rv: WD\n
#output: /rv: REG R 0xd0000032\n
#output: /rv: := 0xabcdef01\n
#output: /rv: IRQ 0x4\n
#output: /rv: REG R 0xd0000036\n
#output: /rv: := 0x76543210\n
#output: /rv: REG R 0xd0000036\n
#output: /rv: := 0x76543211\n
#output: /rv: REG R 0xd0000030\n
#output: /rv: IRQ 0x0\n
#output: /rv: := 0xeeff4455\n
#output: pass\n

# Trivial test of interrupts.
# Locations of IRQ notifiers above depend on when the simulator is
# polled; adjustments may be needed (after checking that no poll is
# gone due to a bug!)

#r W,
#r r,a8832,abcdef01
#r I,4
#r r,a8836,76543210
#r r,a8836,76543211
#r I,0
#r r,a8830,eeff4455

 .lcomm dummy,4

 .include "testutils.inc"
 start
 .if ..asm.arch.cris.v32
 move irqvec1,$ebp
 .else
 move irqvec1,$ibr
 .endif
 test_h_mem 0xabcdef01 0xd0000032
 nop
 nop
 test_h_mem 0x76543210 0xd0000036
 ei
 test_h_mem 0,dummy
wouldreturnhere:
 nop
killme:
 fail

returnhere:
 test_h_mem 0x76543211 0xd0000036
 test_h_mem 0xeeff4455 0xd0000030
 pass

irq0x33:
 .if ..asm.arch.cris.v32
 test_h_dr wouldreturnhere,$erp
 move returnhere,$erp
 rete
 rfe
 .else
 move $dccr,$r0
 test_h_dr wouldreturnhere,$irp
 move returnhere,$irp
 reti
 move $r0,$dccr
 .endif

 singlevec irqvec1,0x33,irq0x33
