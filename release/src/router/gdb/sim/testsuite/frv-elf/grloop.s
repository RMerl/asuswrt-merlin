	.global _start
_start:
	setlo	0x0400,gr10
loop:	
	addicc	gr10,-1,gr10,icc0
	bne	icc0,0,loop
; exit (0)
	setlos	#0,gr8
	setlos	#1,gr7
	tira	gr0,#0
