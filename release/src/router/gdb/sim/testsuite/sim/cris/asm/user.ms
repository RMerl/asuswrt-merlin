# mach: crisv32
# output: 40\n40\n140\nabadefb0\n6543789c\n0\n0\n0\n0\n0\n0\n0\n0\n

; Check for protected operations being NOP in user mode, for the
; parts implemented in this simulator.

 .include "testutils.inc"
 start
 move 0,ccs
 move 0,usp
 move 0,pid
 move 0,srs
 move 0,ebp
 move 0,spc
 setf u

; Flag settings, besides what's tested in rfn.ms, rfe.ms and
; sfe.ms.
 setf i
 move ccs,r3
 dumpr3		; 0x40

 clearf u
 move ccs,r3
 dumpr3		; 0x40

 move 0xc0000300,ccs
 move ccs,r3
 dumpr3		; 0x140

; R14==USP
 move.d 0xabadefb0,r14
 nop
 nop
 nop
 move usp,r3
 dumpr3		; 0xabadefb0
 move 0x6543789c,usp
 nop
 nop
 nop
 move.d r14,r3
 dumpr3		; 0x6543789c

; We can't go back to kernel mode, so we can't check that R14 in
; kernel mode wasn't affected.

; Moves to protected special registers.
 .macro testsr reg,val=-1
 move \val,\reg
 ; Registers shorter than dword will not affect the rest of the
 ; general register when copied using a move insn.
 clear.d r3
; Three cycles are needed between move to protected register and
; read from it, to avoid reading undefined contents due to
; incomplete forwarding.
 nop
 nop
 move \reg,r3
 dumpr3
 moveq \val,r3
 move r3,\reg
 clear.d r3
 nop
 nop
 move \reg,r3
 dumpr3
 .endm

 testsr pid	; 0 0
 testsr srs,3	; 0 0
 testsr ebp	; 0 0
 testsr spc	; 0 0

 quit
