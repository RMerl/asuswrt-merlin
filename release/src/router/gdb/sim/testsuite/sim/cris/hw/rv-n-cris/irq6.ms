#mach: crisv10 crisv32
#sim(crisv10): --hw-device "/rv/trace? true"
#sim(crisv32): --hw-device "/rv/trace? true"
#output: /rv: WD\n
#output: /rv: REG R 0xd0000032\n
#output: /rv: := 0xabcdef01\n
#output: /rv: IRQ 0x4\n
#output: /rv: REG R 0xd0000036\n
#output: /rv: := 0x76543210\n
#output: /rv: REG R 0xd0000030\n
#output: /rv: IRQ 0x0\n
#output: /rv: IRQ 0x8\n
#output: /rv: := 0xeeff4455\n
#output: /rv: REG R 0xd0000034\n
#output: /rv: := 0xdd001122\n
#output: /rv: REG R 0xd0000038\n
#output: /rv: := 0xaaeeff44\n
#output: /rv: REG R 0xd000003c\n
#output: /rv: := 0xff445511\n
#output: pass\n

# Test two successive ints; that flags are disabled when an interrupt
# is taken, and then automatically (or by register restore) enabled at
# return.

#r W,
#r r,a8832,abcdef01
#r I,4
#r r,a8836,76543210
#r I,0
#r I,8
#r r,a8830,eeff4455
#r r,a8834,dd001122
#r r,a8838,aaeeff44
#r r,a883c,ff445511

 .lcomm dummy,4

 .include "testutils.inc"
 start
 test_h_mem 0xabcdef01 0xd0000032
 moveq -1,$r4

 .if ..asm.arch.cris.v32
 move irqvec1,$ebp
 .else
 move irqvec1,$ibr
 .endif

 ei
 test_h_mem 0,dummy

 ; Here after the first interrupt, or perhaps the second interrupt is
 ; taken directly; leave it optional.  Anyway, the second interrupt
 ; should be taken no later than this branch.
 test_h_mem 0,dummy

killme:
 fail

irq0x33:
 .if ..asm.arch.cris.v32
 ; Nothing needed to save flags - "shift" should happen, and back at rfe.
 .else
 ; The missing sim support for interrupt-excluding instructions is matched
 ; by the flaw that sim doesn't service interrupts in straight code.
 ; So, we can use a sequence that would work on actual hardware.
 move $dccr,$r5
 di
 .endif

 test_h_mem 0x76543210 0xd0000036
 test_h_mem 0xeeff4455 0xd0000030
 test_h_mem 0xdd001122 0xd0000034
 moveq -22,$r4

 .if ..asm.arch.cris.v32
 move irqvec2,$ebp
 rete
 rfe
 .else
 move irqvec2,$ibr
 reti
 move $r5,$dccr
 .endif

 pass

irq0x34:
 test_h_mem 0xaaeeff44 0xd0000038
 test_h_mem 0xff445511 0xd000003c
 cmpq -22,$r4
 bne killme
 nop
 pass

 singlevec irqvec1,0x33,irq0x33

 singlevec irqvec2,0x34,irq0x34
