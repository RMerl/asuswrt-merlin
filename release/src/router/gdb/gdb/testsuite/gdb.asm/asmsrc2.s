	.include "common.inc"
	.include "arch.inc"

	comment "Second file in assembly source debugging testcase."

	.global foo2
	gdbasm_declare foo2
	gdbasm_enter

	comment "Call someplace else (several times)."

	gdbasm_call foo3
	gdbasm_call foo3

	comment "All done, return."

	gdbasm_leave
	gdbasm_end foo2
