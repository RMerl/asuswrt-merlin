; Copied from fr30 and modified.
; r9, r11-r13 are used as tmps, consider them call clobbered by these macros.
;
; Do not use the macro counter \@ in macros, there's a bug in
; gas 2.9.1 when it is also a line-separator.
;

	; Don't require the $-prefix on registers.
	.syntax no_register_prefix

	.macro startnostack
	.data
	.space 64,0 ; Simple stack
stackhi:
failmsg:
	.ascii "fail\n"
passmsg:
	.ascii "pass\n"
	.text
	break 11
	.global _start
_start:
	.endm

	.macro start
	startnostack
	move.d stackhi,sp
	.endm

; Exit with return code
	.macro exit rc
	move.d \rc,r10
	moveq 1,r9	; == __NR_exit
	break 13
	break 15
	.endm

; Pass the test case
	.macro pass
	moveq 5,r12
	move.d passmsg,r11
	move.d 1,r10
	moveq 4,r9	; == __NR_write
	break 13
	exit 0
	.endm

; Fail the testcase
	.macro fail
;	moveq 5,r12
;	move.d failmsg,r11
;	move.d 1,r10
;	moveq 4,r1
;	break 13
;	exit 1
	break 15
	.endm

	.macro quit
	break 15
	.endm

	.macro dumpr3
	break 14
	.endm

; Load an immediate value into a general register
; TODO: use minimal sized insn
	.macro mvi_h_gr val reg
	move.d \val,\reg
	.endm

; Load an immediate value into a dedicated register
	.macro mvi_h_dr val reg
	move.d \val,r9
	move.d r9,\reg
	.endm

; Load a general register into another general register
	.macro mvr_h_gr src targ
	move.d \src,\targ
	.endm

; Store an immediate into a word in memory
	.macro mvi_h_mem val addr
	mvi_h_gr  \val r11
	mvr_h_mem r11,\addr
	.endm

; Store a register into a word in memory
	.macro mvr_h_mem reg addr
	move.d \addr,$r13
	move.d \reg,[$r13]
	.endm

; Store the current ps on the stack
	.macro save_ps
	.if ..asm.arch.cris.v32
	move ccs,acr ; Push will do a "subq" first.
	push acr
	.else
	push dccr
	.endif
	.endm

; Load a word value from memory
	.macro ldmem_h_gr addr reg
	move.d \addr,$r13
	move.d [$r13],\reg
	.endm

; Add 2 general registers
	.macro add_h_gr reg1 reg2
	add.d \reg1,\reg2
	.endm

; Increment a register by and immediate
	.macro inci_h_gr inc reg
	mvi_h_gr \inc,r11
	add.d r11,\reg
	.endm

; Test the value of an immediate against a general register
	.macro test_h_gr val reg
	cmp.d \val,\reg
	beq 9f
	nop
	fail
9:
	.endm

; compare two general registers
	.macro testr_h_gr reg1 reg2
	cmp.d \reg1,\reg2
	beq 9f
	fail
9:
	.endm

; Test the value of an immediate against a dedicated register
	.macro test_h_dr val reg
	move \reg,$r12
	test_h_gr \val $r12
	.endm

; Test the value of an general register against a dedicated register
	.macro testr_h_dr gr dr
	move \dr,$r12
	testr_h_gr \gr $r12
	.endm

; Compare an immediate with word in memory
	.macro test_h_mem val addr
	ldmem_h_gr \addr $r12
	test_h_gr \val $r12
	.endm

; Compare a general register with word in memory
	.macro testr_h_mem reg addr
	ldmem_h_gr \addr r12
	testr_h_gr \reg r12
	.endm

; Set the condition codes
; The lower bits of the mask *are* nzvc, so we don't
; have to do anything strange.
	.macro set_cc mask
	move.w \mask,r13
	.if ..asm.arch.cris.v32
	move r13,ccs
	.else
	move r13,ccr
	.endif
	.endm

; Set the stack mode
;	 .macro set_s_user
;	 orccr	0x20
;	 .endm
;
;	 .macro set_s_system
;	 andccr	0x1f
;	 .endm
;
;; Test the stack mode
;	 .macro test_s_user
;	 mvr_h_gr ps,r9
;	 mvi_h_gr 0x20,r11
;	 and	  r11,r9
;	 test_h_gr 0x20,r9
;	 .endm
;
;	 .macro test_s_system
;	 mvr_h_gr ps,r9
;	 mvi_h_gr 0x20,r11
;	 and	  r11,r9
;	 test_h_gr 0x0,r9
;	 .endm

; Set the interrupt bit
; ??? Do they mean "enable interrupts" or "disable interrupts"?
; Assuming enable here.
	.macro set_i val
	.if (\val == 1)
	ei
	.else
	di
	.endif
	.endm

; Test the stack mode
;	 .macro test_i val
;	 mvr_h_gr  ps,r9
;	 mvi_h_gr  0x10,r11
;	 and	   r11,r9
;	 .if (\val == 1)
;	 test_h_gr 0x10,r9
;	 .else
;	 test_h_gr 0x0,r9
;	 .endif
;	 .endm
;
;; Set the ilm
;	 .macro set_ilm val
;	 stilm \val
;	 .endm
;
;; Test the ilm
;	 .macro test_ilm val
;	 mvr_h_gr   ps,r9
;	 mvi_h_gr   0x1f0000,r11
;	 and	    r11,r9
;	 mvi_h_gr   \val,r12
;	 mvi_h_gr   0x1f,r11
;	 and	   r11,r12
;	 lsl	    15,r12
;	 lsl	    1,r12
;	 testr_h_gr r9,r12
;	 .endm
;
; Test the condition codes
	.macro test_cc N Z V C
	.if \N
	bpl 9f
	nop
	.else
	bmi 9f
	nop
	.endif
	.if \Z
	bne 9f
	nop
	.else
	beq 9f
	nop
	.endif
	.if \V
	bvc 9f
	nop
	.else
	bvs 9f
	nop
	.endif
	.if \C
	bcc 9f
	nop
	.else
	bcs 9f
	nop
	.endif
	ba 8f
	nop
9:
	fail
8:
	.endm

	.macro test_move_cc N Z V C
	.if ..asm.arch.cris.v32
	clearf vc
	.endif
	.endm

; Set the division bits
;	 .macro set_dbits val
;	 mvr_h_gr ps,r12
;	 mvi_h_gr 0xfffff8ff,r11
;	 and r11,r12
;	 mvi_h_gr \val,r9
;	 mvi_h_gr 3,r11
;	 and r11,r9
;	 lsl 9,r9
;	 or r9,r12
;	 mvr_h_gr r12,ps
;	 .endm
;
;; Test the division bits
;	 .macro test_dbits val
;	 mvr_h_gr ps,r9
;	 lsr 9,r9
;	 mvi_h_gr 3,r11
;	 and r11,r9
;	 test_h_gr \val,r9
;	 .endm
;
; Save the return pointer
	.macro save_rp
	push srp
	.ENDM

; restore the return pointer
	.macro restore_rp
	pop srp
	.endm

; Ensure branch taken
	.macro take_branch opcode
	\opcode 9f
	nop
	fail
9:
	.endm

	.macro take_branch_d opcode val
	\opcode 9f
	nop
	move.d \val,r9
	fail
9:
	test_h_gr \val,r9
	.endm

; Ensure branch not taken
	.macro no_branch opcode
	\opcode 9f
	nop
	ba	8f
	nop
9:
	fail
8:
	.endm

	.macro no_branch_d opcode val
	\opcode 9f
	move.d	 \val,r9
	nop
	ba	8f
	nop
9:
	fail
8:
	test_h_gr \val,r9
	.endm

