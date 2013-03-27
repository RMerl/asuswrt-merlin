
	.globl _start
_start:

; write (hello world)
	ldi8 r3,#14
	ld24 r2,#hello
	ldi8 r1,#1
	ldi8 r0,#5
	trap #0
; exit (0)
	ldi8 r1,#0
	ldi8 r0,#1
	trap #0

length:	.long 14
hello:	.ascii "Hello World!\r\n"
