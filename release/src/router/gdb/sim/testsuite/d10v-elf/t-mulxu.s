.include "t-macros.i"

	start

	;; clear FX
	ldi r2, #0x8005
	mvtc r2, cr0

	loadacc2 a1 0x7f 0xffff 0xffff
	ldi r8, 0xffff
	ldi r9, 0x8001
test_mulxu1:
	MULXU a1, r9, r8
	checkacc2 1 a1 0x00 0x8000 0x7FFF

	
	;; set FX
	ldi r2, #0x8085
	mvtc r2, cr0

	loadacc2 a1 0x7f 0xffff 0xffff
	ldi r8, 0xffff
	ldi r9, 0x8001
test_mulxu2:
	MULXU a1, r9, r8
	checkacc2 2 a1 0x01 0x0000 0xFFFE

	exit0
