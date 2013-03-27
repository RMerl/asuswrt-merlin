	.global _start
_start:

; write (hello world)
	setlos	#14,gr10
	sethi	%hi(hello),gr9
	setlo	%lo(hello),gr9
	setlos	#1,gr8
	setlos	#5,gr7
	tira	gr0,#0
; exit (0)
	setlos	#0,gr8
	setlos	#1,gr7
	tira	gr0,#0

hello:	.ascii "Hello World!\r\n"
