# FIXME: wip, copied from ../testutils.inc
# r0-r3 are used as tmps, consider them call clobbered by these macros.
# This uses the angel rom monitor calls.
# ??? How do we use the \@ facility of .macros ???
# @ is the comment char!

	.macro a_mvi_h_gr reg, val
	ldr \reg,[pc]
	b . + 8
	.word \val
	.endm

	.macro mvaddr_h_gr reg, addr
	ldr \reg,[pc]
	b . + 8
	.word \val
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
	mov r1, #\rc
	mov r0, #0x2a @ decimal 42
	swi #1
	# If that returns, punt with a sigill.
	stc 0,cr0,[r0]
	.endm

# Other macros know this only clobbers r0.
	.macro test_h_gr reg, val
	mvaddr_h_gr r0, \val
	cmp \reg, r0
	beq . + 8
	fail
	.endm

	.macro mvi_h_cc c, n, v, z
	ldi8 r0, 0
	ldi8 r1, 1
	.if xxx
	cmp r0, r1
	.else
	cmp r1, r0
	.endif
	.endm

	.macro test_h_cc c, n, v, z
	.if xxx
	bc . + 8
	fail
	.else
	bnc . + 8
	fail
	.endif
	.endm
