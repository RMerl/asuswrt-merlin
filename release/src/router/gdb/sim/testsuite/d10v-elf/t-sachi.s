.include "t-macros.i"

	start

test_sachi_1:
	loadacc2 a0 0x00 0xAFFF 0x0000
	sachi r4, a0
	check 1 r4 0x7FFF


test_sachi_2:
	loadacc2 a0 0xFF 0x8000 0x1000
	sachi r4, a0
	check 2 r4 0x8000


test_sachi_3:
	loadacc2 a0 0x00 0x1000 0xA000
	sachi r4, a0
	check 3 r4 0x1000

	exit0
