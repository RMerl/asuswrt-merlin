.include "t-macros.i"

	start

	;; clear FX
	loadpsw2 0x8004
	loadacc2 a0 0x80 0x0000 0x0000
	loadacc2 a1 0x00 0x0000 0x5000
	load r10 0x0123
	load r11 0x4567
test_rac1:
	RAC	r10, a0, #-2
	checkpsw2 1 0x8008
	check2w2 2 r10 0x8000 0x0000

	exit0
