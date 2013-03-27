	comment "subroutine prologue"
	.macro gdbasm_enter
        ldx     _.frame
	pshx
	sts     _.frame
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
        pulx
        stx     _.frame
	rts
	.endm

	.macro gdbasm_call subr
	jsr	\subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
        clra
        clrb
        wai
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
        .sect .data
        .globl _.frame
_.frame: .word 0
        .previous
        lds     #0x2000
        ; the linker script maps the data section in ROM (LMA) for its initial
        ; content and in RAM (VMA) for its runtime value.  We have to do
        ; what the default crt0 does: copy the ROM part in RAM.
        ; (otherwise any 'globalvar' appears uninitialized)
	ldx	#__data_image
	ldy	#__data_section_start
	bra	Start_map
Loop:
	ldaa	0,x
	staa	0,y
	inx
	iny
Start_map:
	cpx	#__data_image_end
	blo	Loop
Done:
        clr     _.frame
        clr     _.frame+1
	.endm

	comment "Declare a data variable"
	.purgem gdbasm_datavar
	.macro gdbasm_datavar name value
	.data
\name:
	.word	\value
	.endm
