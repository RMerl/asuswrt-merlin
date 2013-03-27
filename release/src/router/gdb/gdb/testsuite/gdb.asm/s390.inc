### entry point code
        .macro gdbasm_startup

        # Align the stack pointer to an 8-byte boundary.
        lhi %r0,-8
        nr %r15,%r0

        # Reserve space for the standard stack frame:
        # back chain, and space for the callee to save its registers.
        ahi %r15,-104
        
        # Zero this frame's back chain pointer.
        xc 0(4,%r15),0(%r15)
        .endm


### Call a function.
        .macro gdbasm_call subr

        # Put the address of the constant in %r1, load the constant
        # (SUBR's address), and jump to it.
        bras %r1, .Lafterconst\@
        .long \subr
.Lafterconst\@:
        l %r1,0(%r1)
        basr %r14,%r1
        .endm


### Exit with a zero status.
        .macro gdbasm_exit0
        lhi %r2, 0
        svc 1
        .endm

### Standard subroutine prologue.
        .macro gdbasm_enter

        # Save all the callee-saves registers.  What the heck.
        stm %r6,%r15,24(%r15)

        # Allocate the stack frame, and write the back chain pointer.
        # Keep the original SP in %r11.
        lr %r1,%r15
        ahi %r15,-96
        st %r1,0(%r15)
        .endm


### Standard subroutine epilogue.
        .macro gdbasm_leave

        # Restore all our registers.  This also pops the frame, and
	# restores our return address.
        lm %r6,%r15,120(%r15)

        # Jump to the return address.
        br %r14

        .endm

### Several nops.
        .macro gdbasm_several_nops
        lr %r0, %r0
        lr %r0, %r0
        lr %r0, %r0
        lr %r0, %r0
        .endm

### Declare an `int' variable.
	.purgem gdbasm_datavar
        .macro gdbasm_datavar name value
        .data
\name:
        .long \value
        .endm
