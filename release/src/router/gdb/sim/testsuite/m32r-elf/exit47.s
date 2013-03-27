	;; Return with exit code 47.

	.globl _start
_start:
	ldi8 r1,#47
	ldi8 r0,#1
	trap #0
