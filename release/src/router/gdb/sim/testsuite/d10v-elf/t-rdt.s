.include "t-macros.i"

	start

	PSW_BITS = PSW_C|PSW_F0|PSW_F1
	
	ldi	r6, #success@word
	mvtc	r6, dpc
	ldi	r6, #PSW_BITS
	mvtc	r6, dpsw

test_rdt:
	RTD
	exit47

success:
	checkpsw2 1 PSW_BITS
	exit0
