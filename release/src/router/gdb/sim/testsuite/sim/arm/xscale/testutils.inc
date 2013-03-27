# r0-r3 are used as tmps, consider them call clobbered by these macros.
# This uses the angel rom monitor calls.
# ??? How do we use the \@ facility of .macros ???
# @ is the comment char!

	.macro mvi_h_gr reg, val
	ldr \reg,[pc]
	b . + 8
	.word \val
	.endm

	.macro mvaddr_h_gr reg, addr
	ldr \reg,[pc]
	b . + 8
	.word \addr
	.endm

	.macro start
	.data
failmsg:
	.asciz "fail\n"
passmsg:
	.asciz "pass\n"
	.text

do_pass:
	ldr r1, passmsg_addr
	mov r0, #4
	swi #0x123456
	exit 0
passmsg_addr:
	.word passmsg

do_fail:
	ldr r1, failmsg_addr
	mov r0, #4
	swi #0x123456
	exit 1
failmsg_addr:
	.word failmsg

	.global _start
_start:
	.endm

# *** Other macros know pass/fail are 4 bytes in size!  Yuck.

	.macro pass
	b do_pass
	.endm

	.macro fail
	b do_fail
	.endm

	.macro exit rc
	# ??? This works with the ARMulator but maybe not others.
	#mov r0, #\rc
	#swi #1
	# This seems to be portable (though it ignores rc).
	mov r0,#0x18
	mvi_h_gr r1, 0x20026
	swi #0x123456
	# If that returns, punt with a sigill.
	stc 0,cr0,[r0]
	.endm

# Other macros know this only clobbers r0.
# WARNING: It also clobbers the condition codes (FIXME).
	.macro test_h_gr reg, val
	mvaddr_h_gr r0, \val
	cmp \reg, r0
	beq . + 8
	fail
	.endm

	.macro mvi_h_cnvz c, n, v, z
	mov r0, #0
	.if \c
	orr r0, r0, #0x20000000
	.endif
	.if \n
	orr r0, r0, #0x80000000
	.endif
	.if \v
	orr r0, r0, #0x10000000
	.endif
	.if \z
	orr r0, r0, #0x40000000
	.endif
	mrs r1, cpsr
	bic r1, r1, #0xf0000000
	orr r1, r1, r0
	msr cpsr, r1
	# ??? nops needed
	.endm

# ??? Preserve condition codes?
	.macro test_h_cnvz c, n, v, z
	mov r0, #0
	.if \c
	orr r0, r0, #0x20000000
	.endif
	.if \n
	orr r0, r0, #0x80000000
	.endif
	.if \v
	orr r0, r0, #0x10000000
	.endif
	.if \z
	orr r0, r0, #0x40000000
	.endif
	mrs r1, cpsr
	and r1, r1, #0xf0000000
	cmp r0, r1
	beq . + 8
	fail
	.endm
