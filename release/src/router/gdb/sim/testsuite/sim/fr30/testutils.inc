# r0, r4-r6 are used as tmps, consider them call clobbered by these macros.

	.macro start
	.data
failmsg:
	.ascii "fail\n"
passmsg:
	.ascii "pass\n"
	.text
	.global _start
_start:
	ldi32 0x7fffc,sp	; TODO -- what's a good value for this?
	ldi32 0xffc00,r0
	mov   r0,tbr		; defined in manual
	mov   sp,usp
	mov   sp,ssp
	.endm

; Exit with return code
	.macro exit rc
	ldi32 \rc,r4
	ldi32 #1,r0
	int   #10
	.endm

; Pass the test case
	.macro pass
	ldi32 #5,r6
	ldi32 #passmsg,r5
	ldi32 #1,r4
	ldi32 #5,r0
	int   #10
	exit  #0
	.endm

; Fail the testcase
	.macro fail
	ldi32 #5,r6
	ldi32 #failmsg,r5
	ldi32 #1,r4
	ldi32 #5,r0
	int   #10
	exit  #1
	.endm

; Load an immediate value into a general register
; TODO: use minimal sized insn
	.macro mvi_h_gr val reg
	ldi32 \val,\reg
	.endm

; Load an immediate value into a dedicated register
	.macro mvi_h_dr val reg
	ldi32 \val,r0
	mov r0,\reg
	.endm

; Load a general register into another general register
	.macro mvr_h_gr src targ
	mov \src,\targ
	.endm

; Store an immediate into a word in memory
	.macro mvi_h_mem val addr
	mvi_h_gr  \val r4
	mvr_h_mem r4,\addr
	.endm

; Store a register into a word in memory
	.macro mvr_h_mem reg addr
	st \reg,@\addr
	.endm

; Store the current ps on the stack
	.macro save_ps
	st ps,@-r15
	.endm

; Load a word value from memory
	.macro ldmem_h_gr addr reg
	ld @\addr,\reg
	.endm

; Add 2 general registers
	.macro add_h_gr reg1 reg2
	add \reg1,\reg2
	.endm

; Increment a register by and immediate
	.macro inci_h_gr inc reg
	mvi_h_gr \inc,r4
	add r4,\reg
	.endm

; Test the value of an immediate against a general register
	.macro test_h_gr val reg
	.if (\val >= 0) && (\val <= 15)
	cmp \val,\reg
	.else
	.if (\val < 0) && (\val >= -16)
	cmp2 \val,\reg
	.else
	ldi32 \val,r4
	cmp r4,\reg
	.endif
	.endif
	beq test_gr\@
	fail
test_gr\@:
	.endm

; compare two general registers
	.macro testr_h_gr reg1 reg2
	cmp \reg1,\reg2
	beq testr_gr\@
	fail
testr_gr\@:
	.endm

; Test the value of an immediate against a dedicated register
	.macro test_h_dr val reg
	mov \reg,r5
	test_h_gr \val r5
	.endm

; Test the value of an general register against a dedicated register
	.macro testr_h_dr gr dr
	mov \dr,r5
	testr_h_gr \gr r5
	.endm

; Compare an immediate with word in memory
	.macro test_h_mem val addr
	ldmem_h_gr \addr r5
	test_h_gr \val r5
	.endm

; Compare a general register with word in memory
	.macro testr_h_mem reg addr
	ldmem_h_gr \addr r5
	testr_h_gr \reg r5
	.endm

; Set the condition codes
	.macro set_cc mask
	andccr	0xf0
	orccr	\mask
	.endm

; Set the stack mode
	.macro set_s_user
	orccr	0x20
	.endm

	.macro set_s_system
	andccr	0x1f
	.endm

; Test the stack mode
	.macro test_s_user
	mvr_h_gr ps,r0
	mvi_h_gr 0x20,r4
	and      r4,r0
	test_h_gr 0x20,r0
	.endm

	.macro test_s_system
	mvr_h_gr ps,r0
	mvi_h_gr 0x20,r4
	and      r4,r0
	test_h_gr 0x0,r0
	.endm

; Set the interrupt bit
	.macro set_i val
	.if (\val == 1)
	orccr	0x10
	.else
	andccr	0x2f
	.endif
	.endm

; Test the stack mode
	.macro test_i val
	mvr_h_gr  ps,r0
	mvi_h_gr  0x10,r4
	and       r4,r0
	.if (\val == 1)
	test_h_gr 0x10,r0
	.else
	test_h_gr 0x0,r0
	.endif
	.endm

; Set the ilm
	.macro set_ilm val
	stilm \val
	.endm

; Test the ilm
	.macro test_ilm val
	mvr_h_gr   ps,r0
	mvi_h_gr   0x1f0000,r4
	and        r4,r0
	mvi_h_gr   \val,r5
	mvi_h_gr   0x1f,r4
	and	   r4,r5
	lsl        15,r5
	lsl        1,r5
	testr_h_gr r0,r5
	.endm

; Test the condition codes
	.macro test_cc N Z V C
	.if (\N == 1)
	bp fail\@
	.else
	bn fail\@
	.endif
	.if (\Z == 1)
	bne fail\@
	.else
	beq fail\@
	.endif
	.if (\V == 1)
	bnv fail\@
	.else
	bv fail\@
	.endif
	.if (\C == 1)
	bnc fail\@
	.else
	bc fail\@
	.endif
	bra test_cc\@
fail\@:
	fail
test_cc\@:
	.endm

; Set the division bits
	.macro set_dbits val
	mvr_h_gr ps,r5
	mvi_h_gr 0xfffff8ff,r4
	and r4,r5
	mvi_h_gr \val,r0
	mvi_h_gr 3,r4
	and r4,r0
	lsl 9,r0
	or r0,r5
	mvr_h_gr r5,ps
	.endm

; Test the division bits
	.macro test_dbits val
	mvr_h_gr ps,r0
	lsr 9,r0
	mvi_h_gr 3,r4
	and r4,r0
	test_h_gr \val,r0
	.endm

; Save the return pointer
	.macro save_rp
	st rp,@-R15
	.ENDM

; restore the return pointer
	.macro restore_rp
	ld @R15+,rp
	.endm

; Ensure branch taken
	.macro take_branch opcode
	\opcode take_br\@
	fail
take_br\@:
	.endm

	.macro take_branch_d opcode val
	\opcode take_brd\@
	ldi:8 \val,r0
	fail
take_brd\@:
	test_h_gr \val,r0
	.endm

; Ensure branch not taken
	.macro no_branch opcode
	\opcode no_brf\@
	bra	no_brs\@
no_brf\@:
	fail
no_brs\@:
	.endm

	.macro no_branch_d opcode val
	\opcode no_brdf\@
	ldi:8   \val,r0
	bra	no_brds\@
no_brdf\@:
	fail
no_brds\@:
	test_h_gr \val,r0
	.endm

