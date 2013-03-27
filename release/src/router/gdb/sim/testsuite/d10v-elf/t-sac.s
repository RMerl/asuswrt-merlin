.include "t-macros.i"

	start

test_sac_1:
	loadacc2 a0 0x00 0xAFFF 0x0000
	sac r4, a0
	check 1 r4 0x7FFF
	check 2 r5 0xFFFF

test_sac_2:
	loadacc2 a0 0xFF 0x7000 0x0000
	sac r4, a0
	check 3 r4 0x8000
	check 4 r5 0x0000

test_sac_3:
	loadacc2 a0 0x00 0x1000 0xA000
	sac r4, a0
	check 5 r4 0x1000
	check 6 r5 0xA000

	exit0
