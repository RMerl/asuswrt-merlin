# mach: crisv32
# output: 4000c800\nc3221800\nc8606400\n48606400\n419d8260\n

; Check that SFE affects CCS the right way.

 .include "testutils.inc"
 start

; Set SPC to 1 to disable single step exceptions when S flag is set.
 move 1,spc

; CCS:
;  31            24 23           16 15            8 7             0
;  +---+-----------+-------+-------+-----------+---+---------------+
;  |Q M|S R P U I X N Z V C|S R P U I X N Z V C|S R P U I X N Z V C|
;  |   |2 2 2 2 2 2 2 2 2 2|1 1 1 1 1 1 1 1 1 1|                   |
;  +---+-----------+-------+-------+-----------+---+---------------+

 move 0x40000000,ccs
 setf ixv
 sfe
 move ccs,r3
 dumpr3			; 0x4000c800
 or.d 0x80000000,r3
 move r3,ccs

 setf pzv
 sfe
 move ccs,r3
 dumpr3			; 0xc3221800

 setf xnc
 sfe
 move ccs,r3
 dumpr3			; 0xc8606400

; Clear Q, so we don't get S and Q at the same time when we set S.
 lslq 1,r3
 lsrq 1,r3
 move r3,ccs
 move ccs,r3
 dumpr3			; 0x48606400

 or.w 0x300,r3
 move r3,ccs
 setf ui
 sfe
 move ccs,r3
 dumpr3			; 0x419d8260

 quit
