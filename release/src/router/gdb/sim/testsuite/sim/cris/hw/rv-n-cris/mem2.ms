#mach: crisv10 crisv32
#sim(crisv10): --hw-device "/rv/trace? true"
#sim(crisv32): --hw-device "/rv/trace? true"
#output: /rv: WD\n
#output: /rv: REG R 0xd0000036\n
#output: /rv: := 0x76543210\n
#output: /rv: DMA W 0x20020..0x20043\n
#output: /rv: 0x20020: 12 23 34 56\n
#output: /rv: 0x20024: 79 8a bd de\n
#output: /rv: 0x20028: fb ad ba db\n
#output: /rv: 0x2002c: ad 56 78 9a\n
#output: /rv: 0x20030: fd e1 23 45\n
#output: /rv: 0x20034: 66 54 32 1a\n
#output: /rv: 0x20038: ac cb be ed\n
#output: /rv: 0x2003c: db ed aa da\n
#output: /rv: 0x20040: 00 aa bb cc\n
#output: /rv: REG R 0xd0000038\n
#output: /rv: := 0x76543211\n
#output: /rv: DMA R 0x20000..0x20023\n
#output: /rv: 0x20000: aa 55 12 23\n
#output: /rv: 0x20004: 34 56 79 8a\n
#output: /rv: 0x20008: bd de fb ad\n
#output: /rv: 0x2000c: ba db ad 56\n
#output: /rv: 0x20010: 78 9a fd e1\n
#output: /rv: 0x20014: 23 45 66 54\n
#output: /rv: 0x20018: 32 1a ac cb\n
#output: /rv: 0x2001c: be ed db ed\n
#output: /rv: 0x20020: aa da 00 aa\n
#output: /rv: IRQ 0x8\n
#output: /rv: REG R 0xd0000038\n
#output: /rv: := 0x76543212\n
#output: pass\n

# This is a slight variation of mem1.ms just to trig the "buffer needs
# to be malloced for large request size" for the DMA request.

# Locations of IRQ notifiers above depend on when the simulator is
# polled; adjustments may be needed (after checking that no poll is
# gone due to a bug!)

#r W,
#r r,a8836,76543210
#r s,e020,12233456798abddefbadbadbad56789afde123456654321aaccbbeeddbedaada00aabbcc
#r r,a8838,76543211
#r l,e000,aa5512233456798abddefbadbadbad56789afde123456654321aaccbbeeddbedaada00aa
#r I,8
#r r,a8838,76543212

 .include "testutils.inc"
 start
 test_h_mem 0x76543210 0xd0000036

 move.d 0x2003f,$r1
 move.d 0x10000,$r3
0:
 test.b [$r1]
 bne 1f
 subq 1,$r3
 bne 0b
 nop

1:
 test_h_mem 0x56342312 0x20020
 test_h_mem 0xdebd8a79 0x20024
 test_h_mem 0xdbbaadfb 0x20028
 test_h_mem 0x9a7856ad 0x2002c
 test_h_mem 0x4523e1fd 0x20030
 test_h_mem 0x1a325466 0x20034
 test_h_mem 0xedbecbac 0x20038
 test_h_mem 0xdaaaeddb 0x2003c
 test_h_mem 0xccbbaa00 0x20040

 move.d 0x20020,$r0
 move.d 0x20000,$r1
 move.w 0x55aa,$r2
 move.w $r2,[r1+]
 .rept 9
 move.d [$r0+],$r2
 move.d $r2,[$r1+]
 .endr

 test_h_mem 0x76543211 0xd0000038

 .if ..asm.arch.cris.v32
 move irqvec1,$ebp
 .else
 move irqvec1,$ibr
 .endif
 ei
 move.d 0x100000,$r9
0:
 subq 1,$r9
 bne 0b
 nop
killme:
 fail

irq0x34:
 test_h_mem 0x76543212 0xd0000038
 pass

 .fill 65536*2+128,1,0

 singlevec irqvec1,0x34,irq0x34
