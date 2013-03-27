# mach: crisv32
# output: 4000c3af\n40000020\n40000080\n40000000\n

; Check that RFE affects CCS the right way.

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

; Clear S R P U I X N Z V C, set S1 R1 P1 (not U1) I1 X1 N1 Z1 V1 C1,
; clear S2 R2 P2 U2 N2 Z2 V2 C2, Q; set I2 X2 M:
;   1 1 0 0 0 0 1 1 0 0 0 0 1 1 1 0 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
 move 0x430efc00,ccs

 test_cc 0 0 0 0

 rfe
 test_cc 1 1 1 1
 move ccs,r3
 dumpr3			; 0x4000c3af

 rfe
 test_cc 0 0 0 0
 move ccs,r3
 dumpr3			; 0x40000020

 rfe
 test_cc 0 0 0 0
 move ccs,r3
 dumpr3			; 0x40000080

 or.w 0x100,r3
 move $r3,ccs
 rfe
 move ccs,r3
 dumpr3			; 0x40000000

 quit
