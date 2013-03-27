	.macro comment text
	.endm

	comment "Can't rely on assembler comment character so do this."

	comment "On some platforms (like HPUX), only labels are allowed"
	comment "to start at the first column.  Beware of this when editing"
	comment "the assembly files for this test."

	.macro include arch file
	.include "\arch\file"
	.endm

	comment "Declare a data variable"
	.macro gdbasm_datavar name value
	.data
\name:
	.word \value
	.endm

	comment "Declare a subroutine"
	.macro gdbasm_declare name
\name:
	.endm

	comment "End a subroutine"
	.macro gdbasm_end name
	.endm

	comment "arch.inc is responsible for defining the following macros:"
	comment "enter - subroutine prologue"
	comment "leave - subroutine epilogue"
	comment "call - call a named subroutine"
	comment "several_nops - execute several (typically 4) nops"
	comment "exit0 - exit (0)"

	comment "arch.inc may also override the default definitions of:"
	comment "datavar - define a data variable"
	comment "declare - declare the start of a subroutine"
	comment "end - end a subroutine"
