	.include "common.inc"
	.include "arch.inc"

	comment "WARNING: asm-source.exp checks for line numbers printed by gdb."
	comment "Be careful about changing this file without also changing"
	comment "asm-source.exp."

	
	comment	"This file is not linked with crt0."
	comment	"Provide very simplistic equivalent."
	
	.global _start
	gdbasm_declare _start
	gdbasm_startup
	gdbasm_call main
	gdbasm_exit0
	gdbasm_end _start

	comment "main routine for assembly source debugging test"
	comment "This particular testcase uses macros in <arch>.inc to achieve"
	comment "machine independence."

	.global main
	gdbasm_declare main
	gdbasm_enter

	comment "Call a macro that consists of several lines of assembler code."

	gdbasm_several_nops

	comment "Call a subroutine in another file."

	gdbasm_call foo2

	comment "All done."

	gdbasm_exit0
	gdbasm_end main

	comment "A routine for foo2 to call."

	.global foo3
	gdbasm_declare foo3
	gdbasm_enter
	gdbasm_leave
	gdbasm_end foo3

	.global exit
	gdbasm_declare exit
	gdbasm_exit0
	gdbasm_end exit

	comment "A static function"

	gdbasm_declare foostatic
	gdbasm_enter
	gdbasm_leave
	gdbasm_end foostatic

	comment "A global variable"

	.global globalvar
	gdbasm_datavar	globalvar	11

	comment "A static variable"

	gdbasm_datavar	staticvar	5

	.include "note.inc"
