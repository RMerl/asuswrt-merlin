.include "t-macros.i"

	start

test_slae_1:
	loadpsw2 PSW_ST|PSW_FX
	loadacc2 a0 0x00 0x0AFF 0xF000
	ldi r0, 4
	slae a0, r0
	checkacc2 1 a0 0x00 0x7FFF 0xFFFF

test_slae_2:
	loadpsw2 PSW_ST|PSW_FX
	loadacc2 a0 0xFF 0xF700 0x1000
	ldi r0, 4
	slae a0, r0
	checkacc2 2 a0 0xFF 0x8000 0x0000

test_slae_3:
	loadpsw2 PSW_ST|PSW_FX
	loadacc2 a0 0x00 0x0010 0xA000
	ldi r0, 4
	slae a0, r0
	checkacc2 3 a0 0x00 0x010A 0x0000

test_slae_4:
	loadpsw2 0
	loadacc2 a0 0x00 0x0010 0xA000
	ldi r0, 4
	slae a0, r0
	checkacc2 4 a0 0x00 0x010A 0x0000

test_slae_5:
	loadacc2 a0 0x00 0x0010 0xA000
	ldi r0, -4
	slae a0, r0
	checkacc2 4 a0 0x00 0x0001 0x0A00

	exit0
