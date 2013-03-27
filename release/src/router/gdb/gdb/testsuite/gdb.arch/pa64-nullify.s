	.LEVEL 2.0w
	.text
	.align 8
	.globl	foo
	.type	foo, @function
foo:
	.PROC
	.CALLINFO FRAME=64,NO_CALLS,SAVE_SP,ENTRY_GR=3
	.ENTRY
	copy %r3,%r1
	copy %r30,%r3
	std,ma %r1,64(%r30)
	std %r3,-8(%r30)
	ldo -64(%r29),%r28
	stw %r26,0(%r28)
	ldw 0(%r28),%r28
	extrd,s %r28,63,32,%r28
	ldo 64(%r3),%r30
	ldd,mb -64(%r30),%r3
	nop
	bve,n (%r2)
	.EXIT
	.PROCEND

	.align 8
	.globl	bar
	.type	bar, @function
bar:
	.PROC
	.CALLINFO FRAME=64,NO_CALLS,SAVE_SP,ENTRY_GR=3
	.ENTRY
	copy %r3,%r1
	copy %r30,%r3
	std,ma %r1,64(%r30)
	std %r3,-8(%r30)
	ldo 64(%r3),%r30
	ldd,mb -64(%r30),%r3
	bve,n (%r2)
	.EXIT
	.PROCEND

	.align 8
	.globl	main
	.type	main, @function
main:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP,SAVE_SP,ENTRY_GR=4
	.ENTRY
	std %r2,-16(%r30)
	copy %r3,%r1
	copy %r30,%r3
	std,ma %r1,128(%r30)
	std %r3,-8(%r30)
	std %r4,8(%r3)
	ldo -64(%r29),%r28
	stw %r26,0(%r28)
	std %r25,8(%r28)
	ldw 0(%r28),%r26
	ldo -48(%r30),%r29
	copy %r27,%r4
	b,l foo,%r2
	nop
	copy %r4,%r27
	ldd -16(%r3),%r2
	ldd 8(%r3),%r4
	ldo 64(%r3),%r30
	ldd,mb -64(%r30),%r3
	bve,n (%r2)
	.EXIT
	.PROCEND
