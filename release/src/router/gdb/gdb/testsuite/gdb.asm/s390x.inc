### entry point code
        .macro gdbasm_startup

        # Align the stack pointer to an 8-byte boundary.
        lghi %r0,-16
        ngr %r15,%r0

        # Reserve space for the standard stack frame:
        # back chain, and space for the callee to save its registers.
        aghi %r15,-168
        
        # Zero this frame's back chain pointer.
        xc 0(8,%r15),0(%r15)
        .endm


### Call a function.
        .macro gdbasm_call subr
        brasl %r14, \subr
        .endm


### Exit with a zero status.
        .macro gdbasm_exit0
        lghi %r2, 0
        svc 1
        .endm

### Standard subroutine prologue.
        .macro gdbasm_enter

        # Save all the callee-saves registers.  What the heck.
        stmg %r6,%r15,48(%r15)

        # Allocate the stack frame, and write the back chain pointer.
        # Keep the original SP in %r11.
        lgr %r11,%r15
        aghi %r15,-168
        stg %r11,0(%r15)
        .endm


### Standard subroutine epilogue.
        .macro gdbasm_leave

        # Restore all our registers.  This also pops the frame, and
	# restores our return address.
        lmg %r6,%r15,216(%r15)

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
