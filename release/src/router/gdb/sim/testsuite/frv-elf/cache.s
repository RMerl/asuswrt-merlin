# run with --memory-region 0xff000000,4 --memory-region 0xfe000000,00404000
; Exit with return code
	.macro exit rc
	setlos.p	#1,gr7
	setlos          \rc,gr8
	tira		gr0,#0
	.endm

; Pass the test case
	.macro pass
pass:
	setlos.p	#5,gr10
	setlos		#1,gr8
	setlos		#5,gr7
	sethi.p		%hi(passmsg),gr9
	setlo		%lo(passmsg),gr9
	tira		gr0,#0
	exit		#0
	.endm

; Fail the testcase
	.macro fail
fail\@:
	setlos.p	#5,gr10
	setlos		#1,gr8
	setlos		#5,gr7
	sethi.p		%hi(failmsg),gr9
	setlo		%lo(failmsg),gr9
	tira		gr0,#0
	exit		#1
	.endm

	.data
failmsg:
	.ascii "fail\n"
passmsg:
	.ascii "pass\n"

	.text
	.global _start
_start:
	movsg		hsr0,gr10	; enable insn and data caches
	sethi.p		0xc800,gr11	; in copy-back mode
	setlo		0x0000,gr11
	or		gr10,gr11,gr10
	movgs		gr10,hsr0
	
	sethi.p		0x7,sp
	setlo		0x0000,sp

	; fill the cache
	sethi.p		%hi(done1),gr10
	setlo		%lo(done1),gr10
	movgs		gr10,lr
	setlos.p	0x1000,gr10
	setlos		0x0,gr11
	movgs		gr10,lcr
write1:	st.p		gr11,@(sp,gr11)
	addi.p		gr11,4,gr11
	bctrlr.p	1,0
	bra		write1
done1:
	; read it back
	sethi.p		%hi(done2),gr10
	setlo		%lo(done2),gr10
	movgs		gr10,lr
	setlos.p	0x1000,gr10
	setlos		0x0,gr11
	movgs		gr10,lcr
read1:	ld		@(sp,gr11),gr12
	cmp		gr11,gr12,icc0
	bne		icc0,1,fail
	addi.p		gr11,4,gr11
	bctrlr.p	1,0
	bra		read1
done2:	
	
	; fill the cache twice
	sethi.p		%hi(done3),gr10
	setlo		%lo(done3),gr10
	movgs		gr10,lr
	setlos.p	0x2000,gr10
	setlos		0x0,gr11
	movgs		gr10,lcr
write3:	st.p		gr11,@(sp,gr11)
	addi.p		gr11,4,gr11
	bctrlr.p	1,0
	bra		write3
done3:
	; read it back
	sethi.p		%hi(done4),gr10
	setlo		%lo(done4),gr10
	movgs		gr10,lr
	setlos.p	0x2000,gr10
	setlos		0x0,gr11
	movgs		gr10,lcr
read4:	ld		@(sp,gr11),gr12
	cmp		gr11,gr12,icc0
	bne		icc0,1,fail
	addi.p		gr11,4,gr11
	bctrlr.p	1,0
	bra		read4
done4:	
	; read it back in reverse
	sethi.p		%hi(done5),gr10
	setlo		%lo(done5),gr10
	movgs		gr10,lr
	setlos.p	0x2000,gr10
	setlos		0x7ffc,gr11
	movgs		gr10,lcr
read5:	ld		@(sp,gr11),gr12
	cmp		gr11,gr12,icc0
	bne		icc0,1,fail
	subi.p		gr11,4,gr11
	bctrlr.p	1,0
	bra		read5
done5:	

	; access data and insns in non-cache areas
	sethi.p		0x8038,gr11		; bctrlr 0,0
	setlo		0x2000,gr11

	sethi.p		0xff00,gr10		; documented area
	setlo		0x0000,gr10
	sti		gr11,@(gr10,0)
	jmpl		@(gr10,gr0)

	;  enable RAM mode
	movsg		hsr0,gr10
	sethi.p		0x0040,gr12
	setlo		0x0000,gr12
	or		gr10,gr12,gr10
	movgs		gr10,hsr0

	sethi.p		0xfe00,gr10		; documented area
	setlo		0x0400,gr10
	sti		gr11,@(gr10,0)
	jmpl		@(gr10,gr0)

	sethi.p		0xfe40,gr10		; documented area
	setlo		0x0400,gr10
	sti		gr11,@(gr10,0)
	dcf		@(gr10,gr0)
	jmpl		@(gr10,gr0)

	sethi.p		0x0007,gr10		; non RAM area
	setlo		0x0000,gr10
	sti		gr11,@(gr10,0)
	jmpl		@(gr10,gr0)

	sethi.p		0xfe00,gr10		; insn RAM area
	setlo		0x0000,gr10
	sti		gr11,@(gr10,0)
	jmpl		@(gr10,gr0)

	sethi.p		0xfe40,gr10		; data RAM area
	setlo		0x0000,gr10
	sti		gr11,@(gr10,0)
	dcf		@(gr10,gr0)
	jmpl		@(gr10,gr0)
	
	pass
fail:
	fail
