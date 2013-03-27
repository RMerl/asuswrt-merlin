# mach: crisv32
# output: c008c1af\n40000220\n40000080\n40000000\n

; Check that RFN affects CCS the right way.

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

; Clear S R P U I X N Z V C, set R1 P1 (not U1) I1 X1 N1 Z1 V1 C1,
; clear S1 R2 P2 U2 N2 Z2 V2 C2, set S2 I2 X2 Q, clear M:
;   1 1 0 0 0 0 1 1 0 0 0 0 1 1 1 0 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
 move 0xa306fc00,ccs

 test_cc 0 0 0 0

 rfn
 test_cc 1 1 1 1
 move ccs,r3
 dumpr3			; 0xc008c1af

 and.d 0x3fffffff,r3
 move r3,ccs
 rfn
 test_cc 0 0 0 0
 move ccs,r3
 dumpr3			; 0x40000220

 and.d 0x3fffffff,r3
 move r3,ccs
 rfn
 test_cc 0 0 0 0
 move ccs,r3
 dumpr3			; 0x40000080

 and.d 0x3fffffff,r3
 move r3,ccs
 or.w 0x100,r3
 move r3,ccs
 rfn
 move ccs,r3
 dumpr3			; 0x40000000

 quit
