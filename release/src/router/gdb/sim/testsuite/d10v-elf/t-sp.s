.include "t-macros.i"

	start

;;; Read/Write values to SPU/SPI

	loadpsw2 0
	ldi sp, 0xdead
	loadpsw2 PSW_SM
	ldi sp, 0xbeef
	
	loadpsw2 0
	check 1 sp 0xdead
	loadpsw2 PSW_SM
	check 2 sp 0xbeef

	exit0
