.include "t-macros.i"

	start

	loadacc2 a0 0x00 0x7FFF 0x8000
test_rachi_1:
	rachi r4, a0, 0
	check 1 r4 0x7FFF


	loadacc2 a0 0xFF 0x8000 0x1000
test_rachi_2:
	rachi r4, a0, 0
	check 2 r4 0x8000


	loadacc2 a0 0x00 0x1000 0xA000
test_rachi_3:
	rachi r4, a0, 0
	check 3 r4 0x1001


	loadacc2 a0 0xFF 0xA000 0x7FFF
test_rachi_4:
	rachi r4, a0, 0
	check 4 r4 0xa000

	exit0
