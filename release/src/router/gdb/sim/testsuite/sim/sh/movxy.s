# sh testcase for movxy
# mach:	 shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	.align	2
src1:	.word	1
src2:	.word	2
src3:	.word	3
src4:	.word	4
src5:	.word	5
src6:	.word	6
src7:	.word	7
src8:	.word	8
src9:	.word	9
	.word	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

dst1:	.word	0
dst2:	.word	0
dst3:	.word	0
dst4:	.word	0
dst5:	.word	0
dst6:	.word	0
dst7:	.word	0
dst8:	.word	0
dst9:	.word	0
	.word	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

	start
movxw_nopy:
	set_grs_a5a5
	# load up pointers
	mov.l	srcp1, r4
	mov.l	dstp1, r5

	# perform moves
	movx.w	@r4, x0
	pcopy	x0, a0
	movx.w	a0, @r5

	# verify pointers unchanged
	mov.l	srcp1, r0
	cmp/eq	r0, r4
	bt	.L0
	fail
.L0:
	mov.l	dstp1, r1
	cmp/eq	r1, r5
	bt	.L1
	fail
.L1:
	# verify copied values
	mov.w	@r0, r0
	mov.w	@r1, r1
	cmp/eq	r0, r1
	bt	.L2
	fail
.L2:
	test_gr_a5a5 r2
	test_gr_a5a5 r3
	test_gr_a5a5 r6
	test_gr_a5a5 r7
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

movyw_nopx:
	set_grs_a5a5
	# load up pointers
	mov.l	srcp2, r6
	mov.l	dstp2, r7

	# perform moves
	movy.w	@r6, y0
	pcopy	y0, a0
	movy.w	a0, @r7

	# verify pointers unchanged
	mov.l	srcp2, r2
	cmp/eq	r2, r6
	bt	.L3
	fail
.L3:
	mov.l	dstp2, r3
	cmp/eq	r3, r7
	bt	.L4
	fail
.L4:
	# verify copied values
	mov.w	@r2, r2
	mov.w	@r3, r3
	cmp/eq	r2, r3
	bt	.L5
	fail
.L5:
	test_gr_a5a5 r0
	test_gr_a5a5 r1
	test_gr_a5a5 r4
	test_gr_a5a5 r5
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

movxw_movyw:
	set_grs_a5a5
	# load up pointers
	mov.l	srcp3, r4
	mov.l	dstp3, r5
	mov.l	srcp4, r6
	mov.l	dstp4, r7

	# perform moves
	movx.w	@r4, x1	movy.w	@r6, y1
	pcopy	x1, a0	
	pcopy	y1, a1
	movx.w	a0, @r5 movy.w	a1, @r7

	# verify pointers unchanged
	mov.l	srcp3, r0
	cmp/eq	r0, r4
	bt	.L6
	fail
.L6:
	mov.l	dstp3, r1
	cmp/eq	r1, r5
	bt	.L7
	fail
.L7:
	mov.l	srcp4, r2
	cmp/eq	r2, r6
	bt	.L8
	fail
.L8:
	mov.l	dstp4, r3
	cmp/eq	r3, r7
	bt	.L9
	fail
.L9:
	# verify copied values
	mov.w	@r0, r0
	mov.w	@r1, r1
	cmp/eq	r0, r1
	bt	.L10
	fail
.L10:
	mov.w	@r2, r2
	mov.w	@r3, r3
	cmp/eq	r2, r3
	bt	.L11
	fail
.L11:
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

	bra	movxw_movyw_new
	nop

	.align	2
srcp1:	.long	src1
srcp2:	.long	src2
srcp3:	.long	src3
srcp4:	.long	src4
srcp5:	.long	src5
srcp6:	.long	src6
srcp7:	.long	src7
srcp8:	.long	src8
srcp9:	.long	src9

dstp1:	.long	dst1
dstp2:	.long	dst2
dstp3:	.long	dst3
dstp4:	.long	dst4
dstp5:	.long	dst5
dstp6:	.long	dst6
dstp7:	.long	dst7
dstp8:	.long	dst8
dstp9:	.long	dst9

movxw_movyw_new:
	set_grs_a5a5
	# load up pointers
	mov.l	srcp5b, r0
	mov.l	dstp5b, r1
	mov.l	srcp6b, r2
	mov.l	dstp6b, r3

	# perform moves
	movx.w	@r0, x1	
	movy.w	@r2, y1
	movx.w	x1, @r1 
	movy.w	y1, @r3

	# verify pointers unchanged
	mov.l	srcp5b, r4
	cmp/eq	r0, r4
	bt	.L12
	fail

.L12:
	mov.l	dstp5b, r5
	cmp/eq	r1, r5
	bt	.L13
	fail
.L13:
	mov.l	srcp6b, r6
	cmp/eq	r2, r6
	bt	.L14
	fail
.L14:
	mov.l	dstp6b, r7
	cmp/eq	r3, r7
	bt	.L15
	fail
.L15:
	# verify copied values
	mov.w	@r0, r0
	mov.w	@r1, r1
	cmp/eq	r0, r1
	bt	.L16
	fail
.L16:
	mov.w	@r2, r2
	mov.w	@r3, r3
	cmp/eq	r2, r3
	bt	.L17
	fail
.L17:
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

	mov.l	srcp1b, r0
	mov.l	dstp1b, r1
	mov.l	srcp2b, r2
	mov.l	dstp2b, r3
	mov.l	srcp1b, r4
	mov.l	dstp1b, r5
	mov.l	srcp2b, r6
	mov.l	dstp2b, r7
	mov	#4, r8
	mov	#4, r9
	bra	.L18
	nop

	.align	2
srcp1b:	.long	src1
srcp2b:	.long	src2
srcp3b:	.long	src3
srcp4b:	.long	src4
srcp5b:	.long	src5
srcp6b:	.long	src6
srcp7b:	.long	src7
srcp8b:	.long	src8
srcp9b:	.long	src9

dstp1b:	.long	dst1
dstp2b:	.long	dst2
dstp3b:	.long	dst3
dstp4b:	.long	dst4
dstp5b:	.long	dst5
dstp6b:	.long	dst6
dstp7b:	.long	dst7
dstp8b:	.long	dst8
dstp9b:	.long	dst9

.L18:

	# movx.w @Ax{}, Dx | nopy 
movxwaxdx_nopy:
	movx.w	@r4,x0		! .word	0xf004
	movx.w	@r4,x1		! .word	0xf084
	movx.w	@r5,x0		! .word	0xf204
	movx.w	@r5,x1		! .word	0xf284
	movx.w	@r4+,x0		! .word	0xf008
	movx.w	@r4+,x1		! .word	0xf088
	movx.w	@r5+,x0		! .word	0xf208
	movx.w	@r5+,x1		! .word	0xf288
	movx.w	@r4+r8,x0	! .word	0xf00c
	movx.w	@r4+r8,x1	! .word	0xf08c
	movx.w	@r5+r8,x0	! .word	0xf20c
	movx.w	@r5+r8,x1	! .word	0xf28c
	# movx.w Da, @Ax{} | nopy
movxwdaax_nopy:
	movx.w	a0,@r4		! .word	0xf024
	movx.w	a1,@r4		! .word	0xf0a4
	movx.w	a0,@r5		! .word	0xf224
	movx.w	a1,@r5		! .word	0xf2a4
	movx.w	a0,@r4+		! .word	0xf028
	movx.w	a1,@r4+		! .word	0xf0a8
	movx.w	a0,@r5+		! .word	0xf228
	movx.w	a1,@r5+		! .word	0xf2a8
	movx.w	a0,@r4+r8	! .word	0xf02c
	movx.w	a1,@r4+r8	! .word	0xf0ac
	movx.w	a0,@r5+r8	! .word	0xf22c
	movx.w	a1,@r5+r8	! .word	0xf2ac
	# movy.w @Ay{}, Dy | nopx 
movywaydy_nopx:
	movy.w	@r6,y0		! .word	0xf001
	movy.w	@r6,y1		! .word	0xf041
	movy.w	@r7,y0		! .word	0xf101
	movy.w	@r7,y1		! .word	0xf141
	movy.w	@r6+,y0		! .word	0xf002
	movy.w	@r6+,y1		! .word	0xf042
	movy.w	@r7+,y0		! .word	0xf102
	movy.w	@r7+,y1		! .word	0xf142
	movy.w	@r6+r9,y0	! .word	0xf003
	movy.w	@r6+r9,y1	! .word	0xf043
	movy.w	@r7+r9,y0	! .word	0xf103
	movy.w	@r7+r9,y1	! .word	0xf143
	# movy.w Da, @Ay{} | nopx
movywdaay_nopx:
	movy.w	a0,@r6		! .word	0xf011
	movy.w	a1,@r6		! .word	0xf051
	movy.w	a0,@r7		! .word	0xf111
	movy.w	a1,@r7		! .word	0xf151
	movy.w	a0,@r6+		! .word	0xf012
	movy.w	a1,@r6+		! .word	0xf052
	movy.w	a0,@r7+		! .word	0xf112
	movy.w	a1,@r7+		! .word	0xf152
	movy.w	a0,@r6+r9	! .word	0xf013
	movy.w	a1,@r6+r9	! .word	0xf053
	movy.w	a0,@r7+r9	! .word	0xf113
	movy.w	a1,@r7+r9	! .word	0xf153
	# movx {} || movy {} 
movx_movy:
	movx.w	@r4,x0	movy.w	@r6,y0	! .word	0xf005
	movx.w	@r4,x0	movy.w	@r6,y1	! .word	0xf045
	movx.w	@r4,x1	movy.w	@r6,y0	! .word	0xf085
	movx.w	@r4,x1	movy.w	@r6,y1	! .word	0xf0c5
	movx.w	@r4,x0	movy.w	@r7,y0	! .word	0xf105
	movx.w	@r4,x0	movy.w	@r7,y1	! .word	0xf145
	movx.w	@r4,x1	movy.w	@r7,y0	! .word	0xf185
	movx.w	@r4,x1	movy.w	@r7,y1	! .word	0xf1c5
	movx.w	@r5,x0	movy.w	@r6,y0	! .word	0xf205
	movx.w	@r5,x0	movy.w	@r6,y1	! .word	0xf245
	movx.w	@r5,x1	movy.w	@r6,y0	! .word	0xf285
	movx.w	@r5,x1	movy.w	@r6,y1	! .word	0xf2c5
	movx.w	@r5,x0	movy.w	@r7,y0	! .word	0xf305
	movx.w	@r5,x0	movy.w	@r7,y1	! .word	0xf345
	movx.w	@r5,x1	movy.w	@r7,y0	! .word	0xf385
	movx.w	@r5,x1	movy.w	@r7,y1	! .word	0xf3c5
	movx.w	@r4,x0	movy.w	@r6+,y0	! .word	0xf006
	movx.w	@r4,x0	movy.w	@r6+,y1	! .word	0xf046
	movx.w	@r4,x1	movy.w	@r6+,y0	! .word	0xf086
	movx.w	@r4,x1	movy.w	@r6+,y1	! .word	0xf0c6
	movx.w	@r4,x0	movy.w	@r7+,y0	! .word	0xf106
	movx.w	@r4,x0	movy.w	@r7+,y1	! .word	0xf146
	movx.w	@r4,x1	movy.w	@r7+,y0	! .word	0xf186
	movx.w	@r4,x1	movy.w	@r7+,y1	! .word	0xf1c6
	movx.w	@r5,x0	movy.w	@r6+,y0	! .word	0xf206
	movx.w	@r5,x0	movy.w	@r6+,y1	! .word	0xf246
	movx.w	@r5,x1	movy.w	@r6+,y0	! .word	0xf286
	movx.w	@r5,x1	movy.w	@r6+,y1	! .word	0xf2c6
	movx.w	@r5,x0	movy.w	@r7+,y0	! .word	0xf306
	movx.w	@r5,x0	movy.w	@r7+,y1	! .word	0xf346
	movx.w	@r5,x1	movy.w	@r7+,y0	! .word	0xf386
	movx.w	@r5,x1	movy.w	@r7+,y1	! .word	0xf3c6
	movx.w	@r4,x0	movy.w	@r6+r9,y0	! .word	0xf007
	movx.w	@r4,x0	movy.w	@r6+r9,y1	! .word	0xf047
	movx.w	@r4,x1	movy.w	@r6+r9,y0	! .word	0xf087
	movx.w	@r4,x1	movy.w	@r6+r9,y1	! .word	0xf0c7
	movx.w	@r4,x0	movy.w	@r7+r9,y0	! .word	0xf107
	movx.w	@r4,x0	movy.w	@r7+r9,y1	! .word	0xf147
	movx.w	@r4,x1	movy.w	@r7+r9,y0	! .word	0xf187
	movx.w	@r4,x1	movy.w	@r7+r9,y1	! .word	0xf1c7
	movx.w	@r5,x0	movy.w	@r6+r9,y0	! .word	0xf207
	movx.w	@r5,x0	movy.w	@r6+r9,y1	! .word	0xf247
	movx.w	@r5,x1	movy.w	@r6+r9,y0	! .word	0xf287
	movx.w	@r5,x1	movy.w	@r6+r9,y1	! .word	0xf2c7
	movx.w	@r5,x0	movy.w	@r7+r9,y0	! .word	0xf307
	movx.w	@r5,x0	movy.w	@r7+r9,y1	! .word	0xf347
	movx.w	@r5,x1	movy.w	@r7+r9,y0	! .word	0xf387
	movx.w	@r5,x1	movy.w	@r7+r9,y1	! .word	0xf3c7
	movx.w	@r4+,x0	movy.w	@r6,y0	! .word	0xf009
	movx.w	@r4+,x0	movy.w	@r6,y1	! .word	0xf049
	movx.w	@r4+,x1	movy.w	@r6,y0	! .word	0xf089
	movx.w	@r4+,x1	movy.w	@r6,y1	! .word	0xf0c9
	movx.w	@r4+,x0	movy.w	@r7,y0	! .word	0xf109
	movx.w	@r4+,x0	movy.w	@r7,y1	! .word	0xf149
	movx.w	@r4+,x1	movy.w	@r7,y0	! .word	0xf189
	movx.w	@r4+,x1	movy.w	@r7,y1	! .word	0xf1c9
	movx.w	@r5+,x0	movy.w	@r6,y0	! .word	0xf209
	movx.w	@r5+,x0	movy.w	@r6,y1	! .word	0xf249
	movx.w	@r5+,x1	movy.w	@r6,y0	! .word	0xf289
	movx.w	@r5+,x1	movy.w	@r6,y1	! .word	0xf2c9
	movx.w	@r5+,x0	movy.w	@r7,y0	! .word	0xf309
	movx.w	@r5+,x0	movy.w	@r7,y1	! .word	0xf349
	movx.w	@r5+,x1	movy.w	@r7,y0	! .word	0xf389
	movx.w	@r5+,x1	movy.w	@r7,y1	! .word	0xf3c9
	movx.w	@r4+,x0	movy.w	@r6+,y0	! .word	0xf00a
	movx.w	@r4+,x0	movy.w	@r6+,y1	! .word	0xf04a
	movx.w	@r4+,x1	movy.w	@r6+,y0	! .word	0xf08a
	movx.w	@r4+,x1	movy.w	@r6+,y1	! .word	0xf0ca
	movx.w	@r4+,x0	movy.w	@r7+,y0	! .word	0xf10a
	movx.w	@r4+,x0	movy.w	@r7+,y1	! .word	0xf14a
	movx.w	@r4+,x1	movy.w	@r7+,y0	! .word	0xf18a
	movx.w	@r4+,x1	movy.w	@r7+,y1	! .word	0xf1ca
	movx.w	@r5+,x0	movy.w	@r6+,y0	! .word	0xf20a
	movx.w	@r5+,x0	movy.w	@r6+,y1	! .word	0xf24a
	movx.w	@r5+,x1	movy.w	@r6+,y0	! .word	0xf28a
	movx.w	@r5+,x1	movy.w	@r6+,y1	! .word	0xf2ca
	movx.w	@r5+,x0	movy.w	@r7+,y0	! .word	0xf30a
	movx.w	@r5+,x0	movy.w	@r7+,y1	! .word	0xf34a
	movx.w	@r5+,x1	movy.w	@r7+,y0	! .word	0xf38a
	movx.w	@r5+,x1	movy.w	@r7+,y1	! .word	0xf3ca
	movx.w	@r4+,x0	movy.w	@r6+r9,y0	! .word	0xf00b
	movx.w	@r4+,x0	movy.w	@r6+r9,y1	! .word	0xf04b
	movx.w	@r4+,x1	movy.w	@r6+r9,y0	! .word	0xf08b
	movx.w	@r4+,x1	movy.w	@r6+r9,y1	! .word	0xf0cb
	movx.w	@r4+,x0	movy.w	@r7+r9,y0	! .word	0xf10b
	movx.w	@r4+,x0	movy.w	@r7+r9,y1	! .word	0xf14b
	movx.w	@r4+,x1	movy.w	@r7+r9,y0	! .word	0xf18b
	movx.w	@r4+,x1	movy.w	@r7+r9,y1	! .word	0xf1cb
	movx.w	@r5+,x0	movy.w	@r6+r9,y0	! .word	0xf20b
	movx.w	@r5+,x0	movy.w	@r6+r9,y1	! .word	0xf24b
	movx.w	@r5+,x1	movy.w	@r6+r9,y0	! .word	0xf28b
	movx.w	@r5+,x1	movy.w	@r6+r9,y1	! .word	0xf2cb
	movx.w	@r5+,x0	movy.w	@r7+r9,y0	! .word	0xf30b
	movx.w	@r5+,x0	movy.w	@r7+r9,y1	! .word	0xf34b
	movx.w	@r5+,x1	movy.w	@r7+r9,y0	! .word	0xf38b
	movx.w	@r5+,x1	movy.w	@r7+r9,y1	! .word	0xf3cb
	movx.w	@r4+r8,x0	movy.w	@r6,y0	! .word	0xf00d
	movx.w	@r4+r8,x0	movy.w	@r6,y1	! .word	0xf04d
	movx.w	@r4+r8,x1	movy.w	@r6,y0	! .word	0xf08d
	movx.w	@r4+r8,x1	movy.w	@r6,y1	! .word	0xf0cd
	movx.w	@r4+r8,x0	movy.w	@r7,y0	! .word	0xf10d
	movx.w	@r4+r8,x0	movy.w	@r7,y1	! .word	0xf14d
	movx.w	@r4+r8,x1	movy.w	@r7,y0	! .word	0xf18d
	movx.w	@r4+r8,x1	movy.w	@r7,y1	! .word	0xf1cd
	movx.w	@r5+r8,x0	movy.w	@r6,y0	! .word	0xf20d
	movx.w	@r5+r8,x0	movy.w	@r6,y1	! .word	0xf24d
	movx.w	@r5+r8,x1	movy.w	@r6,y0	! .word	0xf28d
	movx.w	@r5+r8,x1	movy.w	@r6,y1	! .word	0xf2cd
	movx.w	@r5+r8,x0	movy.w	@r7,y0	! .word	0xf30d
	movx.w	@r5+r8,x0	movy.w	@r7,y1	! .word	0xf34d
	movx.w	@r5+r8,x1	movy.w	@r7,y0	! .word	0xf38d
	movx.w	@r5+r8,x1	movy.w	@r7,y1	! .word	0xf3cd
	movx.w	@r4+r8,x0	movy.w	@r6+,y0	! .word	0xf00e
	movx.w	@r4+r8,x0	movy.w	@r6+,y1	! .word	0xf04e
	movx.w	@r4+r8,x1	movy.w	@r6+,y0	! .word	0xf08e
	movx.w	@r4+r8,x1	movy.w	@r6+,y1	! .word	0xf0ce
	movx.w	@r4+r8,x0	movy.w	@r7+,y0	! .word	0xf10e
	movx.w	@r4+r8,x0	movy.w	@r7+,y1	! .word	0xf14e
	movx.w	@r4+r8,x1	movy.w	@r7+,y0	! .word	0xf18e
	movx.w	@r4+r8,x1	movy.w	@r7+,y1	! .word	0xf1ce
	movx.w	@r5+r8,x0	movy.w	@r6+,y0	! .word	0xf20e
	movx.w	@r5+r8,x0	movy.w	@r6+,y1	! .word	0xf24e
	movx.w	@r5+r8,x1	movy.w	@r6+,y0	! .word	0xf28e
	movx.w	@r5+r8,x1	movy.w	@r6+,y1	! .word	0xf2ce
	movx.w	@r5+r8,x0	movy.w	@r7+,y0	! .word	0xf30e
	movx.w	@r5+r8,x0	movy.w	@r7+,y1	! .word	0xf34e
	movx.w	@r5+r8,x1	movy.w	@r7+,y0	! .word	0xf38e
	movx.w	@r5+r8,x1	movy.w	@r7+,y1	! .word	0xf3ce
	movx.w	@r4+r8,x0	movy.w	@r6+r9,y0	! .word	0xf00f
	movx.w	@r4+r8,x0	movy.w	@r6+r9,y1	! .word	0xf04f
	movx.w	@r4+r8,x1	movy.w	@r6+r9,y0	! .word	0xf08f
	movx.w	@r4+r8,x1	movy.w	@r6+r9,y1	! .word	0xf0cf
	movx.w	@r4+r8,x0	movy.w	@r7+r9,y0	! .word	0xf10f
	movx.w	@r4+r8,x0	movy.w	@r7+r9,y1	! .word	0xf14f
	movx.w	@r4+r8,x1	movy.w	@r7+r9,y0	! .word	0xf18f
	movx.w	@r4+r8,x1	movy.w	@r7+r9,y1	! .word	0xf1cf
	movx.w	@r5+r8,x0	movy.w	@r6+r9,y0	! .word	0xf20f
	movx.w	@r5+r8,x0	movy.w	@r6+r9,y1	! .word	0xf24f
	movx.w	@r5+r8,x1	movy.w	@r6+r9,y0	! .word	0xf28f
	movx.w	@r5+r8,x1	movy.w	@r6+r9,y1	! .word	0xf2cf
	movx.w	@r5+r8,x0	movy.w	@r7+r9,y0	! .word	0xf30f
	movx.w	@r5+r8,x0	movy.w	@r7+r9,y1	! .word	0xf34f
	movx.w	@r5+r8,x1	movy.w	@r7+r9,y0	! .word	0xf38f
	movx.w	@r5+r8,x1	movy.w	@r7+r9,y1	! .word	0xf3cf
	movx.w	@r4,x0	movy.w	a0,@r6	! .word	0xf015
	movx.w	@r4,x0	movy.w	a1,@r6	! .word	0xf055
	movx.w	@r4,x1	movy.w	a0,@r6	! .word	0xf095
	movx.w	@r4,x1	movy.w	a1,@r6	! .word	0xf0d5
	movx.w	@r4,x0	movy.w	a0,@r7	! .word	0xf115
	movx.w	@r4,x0	movy.w	a1,@r7	! .word	0xf155
	movx.w	@r4,x1	movy.w	a0,@r7	! .word	0xf195
	movx.w	@r4,x1	movy.w	a1,@r7	! .word	0xf1d5
	movx.w	@r5,x0	movy.w	a0,@r6	! .word	0xf215
	movx.w	@r5,x0	movy.w	a1,@r6	! .word	0xf255
	movx.w	@r5,x1	movy.w	a0,@r6	! .word	0xf295
	movx.w	@r5,x1	movy.w	a1,@r6	! .word	0xf2d5
	movx.w	@r5,x0	movy.w	a0,@r7	! .word	0xf315
	movx.w	@r5,x0	movy.w	a1,@r7	! .word	0xf355
	movx.w	@r5,x1	movy.w	a0,@r7	! .word	0xf395
	movx.w	@r5,x1	movy.w	a1,@r7	! .word	0xf3d5
	movx.w	@r4,x0	movy.w	a0,@r6+	! .word	0xf016
	movx.w	@r4,x0	movy.w	a1,@r6+	! .word	0xf056
	movx.w	@r4,x1	movy.w	a0,@r6+	! .word	0xf096
	movx.w	@r4,x1	movy.w	a1,@r6+	! .word	0xf0d6
	movx.w	@r4,x0	movy.w	a0,@r7+	! .word	0xf116
	movx.w	@r4,x0	movy.w	a1,@r7+	! .word	0xf156
	movx.w	@r4,x1	movy.w	a0,@r7+	! .word	0xf196
	movx.w	@r4,x1	movy.w	a1,@r7+	! .word	0xf1d6
	movx.w	@r5,x0	movy.w	a0,@r6+	! .word	0xf216
	movx.w	@r5,x0	movy.w	a1,@r6+	! .word	0xf256
	movx.w	@r5,x1	movy.w	a0,@r6+	! .word	0xf296
	movx.w	@r5,x1	movy.w	a1,@r6+	! .word	0xf2d6
	movx.w	@r5,x0	movy.w	a0,@r7+	! .word	0xf316
	movx.w	@r5,x0	movy.w	a1,@r7+	! .word	0xf356
	movx.w	@r5,x1	movy.w	a0,@r7+	! .word	0xf396
	movx.w	@r5,x1	movy.w	a1,@r7+	! .word	0xf3d6
	movx.w	@r4,x0	movy.w	a0,@r6+r9	! .word	0xf017
	movx.w	@r4,x0	movy.w	a1,@r6+r9	! .word	0xf057
	movx.w	@r4,x1	movy.w	a0,@r6+r9	! .word	0xf097
	movx.w	@r4,x1	movy.w	a1,@r6+r9	! .word	0xf0d7
	movx.w	@r4,x0	movy.w	a0,@r7+r9	! .word	0xf117
	movx.w	@r4,x0	movy.w	a1,@r7+r9	! .word	0xf157
	movx.w	@r4,x1	movy.w	a0,@r7+r9	! .word	0xf197
	movx.w	@r4,x1	movy.w	a1,@r7+r9	! .word	0xf1d7
	movx.w	@r5,x0	movy.w	a0,@r6+r9	! .word	0xf217
	movx.w	@r5,x0	movy.w	a1,@r6+r9	! .word	0xf257
	movx.w	@r5,x1	movy.w	a0,@r6+r9	! .word	0xf297
	movx.w	@r5,x1	movy.w	a1,@r6+r9	! .word	0xf2d7
	movx.w	@r5,x0	movy.w	a0,@r7+r9	! .word	0xf317
	movx.w	@r5,x0	movy.w	a1,@r7+r9	! .word	0xf357
	movx.w	@r5,x1	movy.w	a0,@r7+r9	! .word	0xf397
	movx.w	@r5,x1	movy.w	a1,@r7+r9	! .word	0xf3d7
	movx.w	@r4+,x0	movy.w	a0,@r6	! .word	0xf019
	movx.w	@r4+,x0	movy.w	a1,@r6	! .word	0xf059
	movx.w	@r4+,x1	movy.w	a0,@r6	! .word	0xf099
	movx.w	@r4+,x1	movy.w	a1,@r6	! .word	0xf0d9
	movx.w	@r4+,x0	movy.w	a0,@r7	! .word	0xf119
	movx.w	@r4+,x0	movy.w	a1,@r7	! .word	0xf159
	movx.w	@r4+,x1	movy.w	a0,@r7	! .word	0xf199
	movx.w	@r4+,x1	movy.w	a1,@r7	! .word	0xf1d9
	movx.w	@r5+,x0	movy.w	a0,@r6	! .word	0xf219
	movx.w	@r5+,x0	movy.w	a1,@r6	! .word	0xf259
	movx.w	@r5+,x1	movy.w	a0,@r6	! .word	0xf299
	movx.w	@r5+,x1	movy.w	a1,@r6	! .word	0xf2d9
	movx.w	@r5+,x0	movy.w	a0,@r7	! .word	0xf319
	movx.w	@r5+,x0	movy.w	a1,@r7	! .word	0xf359
	movx.w	@r5+,x1	movy.w	a0,@r7	! .word	0xf399
	movx.w	@r5+,x1	movy.w	a1,@r7	! .word	0xf3d9
	movx.w	@r4+,x0	movy.w	a0,@r6+	! .word	0xf01a
	movx.w	@r4+,x0	movy.w	a1,@r6+	! .word	0xf05a
	movx.w	@r4+,x1	movy.w	a0,@r6+	! .word	0xf09a
	movx.w	@r4+,x1	movy.w	a1,@r6+	! .word	0xf0da
	movx.w	@r4+,x0	movy.w	a0,@r7+	! .word	0xf11a
	movx.w	@r4+,x0	movy.w	a1,@r7+	! .word	0xf15a
	movx.w	@r4+,x1	movy.w	a0,@r7+	! .word	0xf19a
	movx.w	@r4+,x1	movy.w	a1,@r7+	! .word	0xf1da
	movx.w	@r5+,x0	movy.w	a0,@r6+	! .word	0xf21a
	movx.w	@r5+,x0	movy.w	a1,@r6+	! .word	0xf25a
	movx.w	@r5+,x1	movy.w	a0,@r6+	! .word	0xf29a
	movx.w	@r5+,x1	movy.w	a1,@r6+	! .word	0xf2da
	movx.w	@r5+,x0	movy.w	a0,@r7+	! .word	0xf31a
	movx.w	@r5+,x0	movy.w	a1,@r7+	! .word	0xf35a
	movx.w	@r5+,x1	movy.w	a0,@r7+	! .word	0xf39a
	movx.w	@r5+,x1	movy.w	a1,@r7+	! .word	0xf3da
	movx.w	@r4+,x0	movy.w	a0,@r6+r9	! .word	0xf01b
	movx.w	@r4+,x0	movy.w	a1,@r6+r9	! .word	0xf05b
	movx.w	@r4+,x1	movy.w	a0,@r6+r9	! .word	0xf09b
	movx.w	@r4+,x1	movy.w	a1,@r6+r9	! .word	0xf0db
	movx.w	@r4+,x0	movy.w	a0,@r7+r9	! .word	0xf11b
	movx.w	@r4+,x0	movy.w	a1,@r7+r9	! .word	0xf15b
	movx.w	@r4+,x1	movy.w	a0,@r7+r9	! .word	0xf19b
	movx.w	@r4+,x1	movy.w	a1,@r7+r9	! .word	0xf1db
	movx.w	@r5+,x0	movy.w	a0,@r6+r9	! .word	0xf21b
	movx.w	@r5+,x0	movy.w	a1,@r6+r9	! .word	0xf25b
	movx.w	@r5+,x1	movy.w	a0,@r6+r9	! .word	0xf29b
	movx.w	@r5+,x1	movy.w	a1,@r6+r9	! .word	0xf2db
	movx.w	@r5+,x0	movy.w	a0,@r7+r9	! .word	0xf31b
	movx.w	@r5+,x0	movy.w	a1,@r7+r9	! .word	0xf35b
	movx.w	@r5+,x1	movy.w	a0,@r7+r9	! .word	0xf39b
	movx.w	@r5+,x1	movy.w	a1,@r7+r9	! .word	0xf3db
	movx.w	@r4+r8,x0	movy.w	a0,@r6	! .word	0xf01d
	movx.w	@r4+r8,x0	movy.w	a1,@r6	! .word	0xf05d
	movx.w	@r4+r8,x1	movy.w	a0,@r6	! .word	0xf09d
	movx.w	@r4+r8,x1	movy.w	a1,@r6	! .word	0xf0dd
	movx.w	@r4+r8,x0	movy.w	a0,@r7	! .word	0xf11d
	movx.w	@r4+r8,x0	movy.w	a1,@r7	! .word	0xf15d
	movx.w	@r4+r8,x1	movy.w	a0,@r7	! .word	0xf19d
	movx.w	@r4+r8,x1	movy.w	a1,@r7	! .word	0xf1dd
	movx.w	@r5+r8,x0	movy.w	a0,@r6	! .word	0xf21d
	movx.w	@r5+r8,x0	movy.w	a1,@r6	! .word	0xf25d
	movx.w	@r5+r8,x1	movy.w	a0,@r6	! .word	0xf29d
	movx.w	@r5+r8,x1	movy.w	a1,@r6	! .word	0xf2dd
	movx.w	@r5+r8,x0	movy.w	a0,@r7	! .word	0xf31d
	movx.w	@r5+r8,x0	movy.w	a1,@r7	! .word	0xf35d
	movx.w	@r5+r8,x1	movy.w	a0,@r7	! .word	0xf39d
	movx.w	@r5+r8,x1	movy.w	a1,@r7	! .word	0xf3dd
	movx.w	@r4+r8,x0	movy.w	a0,@r6+	! .word	0xf01e
	movx.w	@r4+r8,x0	movy.w	a1,@r6+	! .word	0xf05e
	movx.w	@r4+r8,x1	movy.w	a0,@r6+	! .word	0xf09e
	movx.w	@r4+r8,x1	movy.w	a1,@r6+	! .word	0xf0de
	movx.w	@r4+r8,x0	movy.w	a0,@r7+	! .word	0xf11e
	movx.w	@r4+r8,x0	movy.w	a1,@r7+	! .word	0xf15e
	movx.w	@r4+r8,x1	movy.w	a0,@r7+	! .word	0xf19e
	movx.w	@r4+r8,x1	movy.w	a1,@r7+	! .word	0xf1de
	movx.w	@r5+r8,x0	movy.w	a0,@r6+	! .word	0xf21e
	movx.w	@r5+r8,x0	movy.w	a1,@r6+	! .word	0xf25e
	movx.w	@r5+r8,x1	movy.w	a0,@r6+	! .word	0xf29e
	movx.w	@r5+r8,x1	movy.w	a1,@r6+	! .word	0xf2de
	movx.w	@r5+r8,x0	movy.w	a0,@r7+	! .word	0xf31e
	movx.w	@r5+r8,x0	movy.w	a1,@r7+	! .word	0xf35e
	movx.w	@r5+r8,x1	movy.w	a0,@r7+	! .word	0xf39e
	movx.w	@r5+r8,x1	movy.w	a1,@r7+	! .word	0xf3de
	movx.w	@r4+r8,x0	movy.w	a0,@r6+r9	! .word	0xf01f
	movx.w	@r4+r8,x0	movy.w	a1,@r6+r9	! .word	0xf05f
	movx.w	@r4+r8,x1	movy.w	a0,@r6+r9	! .word	0xf09f
	movx.w	@r4+r8,x1	movy.w	a1,@r6+r9	! .word	0xf0df
	movx.w	@r4+r8,x0	movy.w	a0,@r7+r9	! .word	0xf11f
	movx.w	@r4+r8,x0	movy.w	a1,@r7+r9	! .word	0xf15f
	movx.w	@r4+r8,x1	movy.w	a0,@r7+r9	! .word	0xf19f
	movx.w	@r4+r8,x1	movy.w	a1,@r7+r9	! .word	0xf1df
	movx.w	@r5+r8,x0	movy.w	a0,@r6+r9	! .word	0xf21f
	movx.w	@r5+r8,x0	movy.w	a1,@r6+r9	! .word	0xf25f
	movx.w	@r5+r8,x1	movy.w	a0,@r6+r9	! .word	0xf29f
	movx.w	@r5+r8,x1	movy.w	a1,@r6+r9	! .word	0xf2df
	movx.w	@r5+r8,x0	movy.w	a0,@r7+r9	! .word	0xf31f
	movx.w	@r5+r8,x0	movy.w	a1,@r7+r9	! .word	0xf35f
	movx.w	@r5+r8,x1	movy.w	a0,@r7+r9	! .word	0xf39f
	movx.w	@r5+r8,x1	movy.w	a1,@r7+r9	! .word	0xf3df
	movx.w	a0,@r4	movy.w	@r6,y0	! .word	0xf025
	movx.w	a0,@r4	movy.w	@r6,y1	! .word	0xf065
	movx.w	a1,@r4	movy.w	@r6,y0	! .word	0xf0a5
	movx.w	a1,@r4	movy.w	@r6,y1	! .word	0xf0e5
	movx.w	a0,@r4	movy.w	@r7,y0	! .word	0xf125
	movx.w	a0,@r4	movy.w	@r7,y1	! .word	0xf165
	movx.w	a1,@r4	movy.w	@r7,y0	! .word	0xf1a5
	movx.w	a1,@r4	movy.w	@r7,y1	! .word	0xf1e5
	movx.w	a0,@r5	movy.w	@r6,y0	! .word	0xf225
	movx.w	a0,@r5	movy.w	@r6,y1	! .word	0xf265
	movx.w	a1,@r5	movy.w	@r6,y0	! .word	0xf2a5
	movx.w	a1,@r5	movy.w	@r6,y1	! .word	0xf2e5
	movx.w	a0,@r5	movy.w	@r7,y0	! .word	0xf325
	movx.w	a0,@r5	movy.w	@r7,y1	! .word	0xf365
	movx.w	a0,@r5	movy.w	@r7,y1	! .word	0xf3a5
	movx.w	a1,@r5	movy.w	@r7,y1	! .word	0xf3e5
	movx.w	a0,@r4	movy.w	@r6+,y0	! .word	0xf026
	movx.w	a0,@r4	movy.w	@r6+,y1	! .word	0xf066
	movx.w	a1,@r4	movy.w	@r6+,y0	! .word	0xf0a6
	movx.w	a1,@r4	movy.w	@r6+,y1	! .word	0xf0e6
	movx.w	a0,@r4	movy.w	@r7+,y0	! .word	0xf126
	movx.w	a0,@r4	movy.w	@r7+,y1	! .word	0xf166
	movx.w	a1,@r4	movy.w	@r7+,y0	! .word	0xf1a6
	movx.w	a1,@r4	movy.w	@r7+,y1	! .word	0xf1e6
	movx.w	a0,@r5	movy.w	@r6+,y0	! .word	0xf226
	movx.w	a0,@r5	movy.w	@r6+,y1	! .word	0xf266
	movx.w	a1,@r5	movy.w	@r6+,y0	! .word	0xf2a6
	movx.w	a1,@r5	movy.w	@r6+,y1	! .word	0xf2e6
	movx.w	a0,@r5	movy.w	@r7+,y0	! .word	0xf326
	movx.w	a0,@r5	movy.w	@r7+,y1	! .word	0xf366
	movx.w	a1,@r5	movy.w	@r7+,y0	! .word	0xf3a6
	movx.w	a1,@r5	movy.w	@r7+,y1	! .word	0xf3e6
	movx.w	a0,@r4	movy.w	@r6+r9,y0	! .word	0xf027
	movx.w	a0,@r4	movy.w	@r6+r9,y1	! .word	0xf067
	movx.w	a1,@r4	movy.w	@r6+r9,y0	! .word	0xf0a7
	movx.w	a1,@r4	movy.w	@r6+r9,y1	! .word	0xf0e7
	movx.w	a0,@r4	movy.w	@r7+r9,y0	! .word	0xf127
	movx.w	a0,@r4	movy.w	@r7+r9,y1	! .word	0xf167
	movx.w	a1,@r4	movy.w	@r7+r9,y0	! .word	0xf1a7
	movx.w	a1,@r4	movy.w	@r7+r9,y1	! .word	0xf1e7
	movx.w	a0,@r5	movy.w	@r6+r9,y0	! .word	0xf227
	movx.w	a0,@r5	movy.w	@r6+r9,y1	! .word	0xf267
	movx.w	a1,@r5	movy.w	@r6+r9,y0	! .word	0xf2a7
	movx.w	a1,@r5	movy.w	@r6+r9,y1	! .word	0xf2e7
	movx.w	a0,@r5	movy.w	@r7+r9,y0	! .word	0xf327
	movx.w	a0,@r5	movy.w	@r7+r9,y1	! .word	0xf367
	movx.w	a1,@r5	movy.w	@r7+r9,y0	! .word	0xf3a7
	movx.w	a1,@r5	movy.w	@r7+r9,y1	! .word	0xf3e7
	movx.w	a0,@r4+	movy.w	@r6,y0	! .word	0xf029
	movx.w	a0,@r4+	movy.w	@r6,y1	! .word	0xf069
	movx.w	a1,@r4+	movy.w	@r6,y0	! .word	0xf0a9
	movx.w	a1,@r4+	movy.w	@r6,y1	! .word	0xf0e9
	movx.w	a0,@r4+	movy.w	@r7,y0	! .word	0xf129
	movx.w	a0,@r4+	movy.w	@r7,y1	! .word	0xf169
	movx.w	a1,@r4+	movy.w	@r7,y0	! .word	0xf1a9
	movx.w	a1,@r4+	movy.w	@r7,y1	! .word	0xf1e9
	movx.w	a0,@r5+	movy.w	@r6,y0	! .word	0xf229
	movx.w	a0,@r5+	movy.w	@r6,y1	! .word	0xf269
	movx.w	a1,@r5+	movy.w	@r6,y0	! .word	0xf2a9
	movx.w	a1,@r5+	movy.w	@r6,y1	! .word	0xf2e9
	movx.w	a0,@r5+	movy.w	@r7,y0	! .word	0xf329
	movx.w	a0,@r5+	movy.w	@r7,y1	! .word	0xf369
	movx.w	a1,@r5+	movy.w	@r7,y0	! .word	0xf3a9
	movx.w	a1,@r5+	movy.w	@r7,y1	! .word	0xf3e9
	movx.w	a0,@r4+	movy.w	@r6+,y0	! .word	0xf02a
	movx.w	a0,@r4+	movy.w	@r6+,y1	! .word	0xf06a
	movx.w	a1,@r4+	movy.w	@r6+,y0	! .word	0xf0aa
	movx.w	a1,@r4+	movy.w	@r6+,y1	! .word	0xf0ea
	movx.w	a0,@r4+	movy.w	@r7+,y0	! .word	0xf12a
	movx.w	a0,@r4+	movy.w	@r7+,y1	! .word	0xf16a
	movx.w	a1,@r4+	movy.w	@r7+,y0	! .word	0xf1aa
	movx.w	a1,@r4+	movy.w	@r7+,y1	! .word	0xf1ea
	movx.w	a0,@r5+	movy.w	@r6+,y0	! .word	0xf22a
	movx.w	a0,@r5+	movy.w	@r6+,y1	! .word	0xf26a
	movx.w	a1,@r5+	movy.w	@r6+,y0	! .word	0xf2aa
	movx.w	a1,@r5+	movy.w	@r6+,y1	! .word	0xf2ea
	movx.w	a0,@r5+	movy.w	@r7+,y0	! .word	0xf32a
	movx.w	a0,@r5+	movy.w	@r7+,y1	! .word	0xf36a
	movx.w	a1,@r5+	movy.w	@r7+,y0	! .word	0xf3aa
	movx.w	a1,@r5+	movy.w	@r7+,y1	! .word	0xf3ea
	movx.w	a0,@r4+	movy.w	@r6+r9,y0	! .word	0xf02b
	movx.w	a0,@r4+	movy.w	@r6+r9,y1	! .word	0xf06b
	movx.w	a1,@r4+	movy.w	@r6+r9,y0	! .word	0xf0ab
	movx.w	a1,@r4+	movy.w	@r6+r9,y1	! .word	0xf0eb
	movx.w	a0,@r4+	movy.w	@r7+r9,y0	! .word	0xf12b
	movx.w	a0,@r4+	movy.w	@r7+r9,y1	! .word	0xf16b
	movx.w	a1,@r4+	movy.w	@r7+r9,y0	! .word	0xf1ab
	movx.w	a1,@r4+	movy.w	@r7+r9,y1	! .word	0xf1eb
	movx.w	a0,@r5+	movy.w	@r6+r9,y0	! .word	0xf22b
	movx.w	a0,@r5+	movy.w	@r6+r9,y1	! .word	0xf26b
	movx.w	a1,@r5+	movy.w	@r6+r9,y0	! .word	0xf2ab
	movx.w	a1,@r5+	movy.w	@r6+r9,y1	! .word	0xf2eb
	movx.w	a0,@r5+	movy.w	@r7+r9,y0	! .word	0xf32b
	movx.w	a0,@r5+	movy.w	@r7+r9,y1	! .word	0xf36b
	movx.w	a1,@r5+	movy.w	@r7+r9,y0	! .word	0xf3ab
	movx.w	a1,@r5+	movy.w	@r7+r9,y1	! .word	0xf3eb
	movx.w	a0,@r4+r8	movy.w	@r6,y0	! .word	0xf02d
	movx.w	a0,@r4+r8	movy.w	@r6,y1	! .word	0xf06d
	movx.w	a1,@r4+r8	movy.w	@r6,y0	! .word	0xf0ad
	movx.w	a1,@r4+r8	movy.w	@r6,y1	! .word	0xf0ed
	movx.w	a0,@r4+r8	movy.w	@r7,y0	! .word	0xf12d
	movx.w	a0,@r4+r8	movy.w	@r7,y1	! .word	0xf16d
	movx.w	a1,@r4+r8	movy.w	@r7,y0	! .word	0xf1ad
	movx.w	a1,@r4+r8	movy.w	@r7,y1	! .word	0xf1ed
	movx.w	a0,@r5+r8	movy.w	@r6,y0	! .word	0xf22d
	movx.w	a0,@r5+r8	movy.w	@r6,y1	! .word	0xf26d
	movx.w	a1,@r5+r8	movy.w	@r6,y0	! .word	0xf2ad
	movx.w	a1,@r5+r8	movy.w	@r6,y1	! .word	0xf2ed
	movx.w	a0,@r5+r8	movy.w	@r7,y0	! .word	0xf32d
	movx.w	a0,@r5+r8	movy.w	@r7,y1	! .word	0xf36d
	movx.w	a1,@r5+r8	movy.w	@r7,y0	! .word	0xf3ad
	movx.w	a1,@r5+r8	movy.w	@r7,y1	! .word	0xf3ed
	movx.w	a0,@r4+r8	movy.w	@r6+,y0	! .word	0xf02e
	movx.w	a0,@r4+r8	movy.w	@r6+,y1	! .word	0xf06e
	movx.w	a1,@r4+r8	movy.w	@r6+,y0	! .word	0xf0ae
	movx.w	a1,@r4+r8	movy.w	@r6+,y1	! .word	0xf0ee
	movx.w	a0,@r4+r8	movy.w	@r7+,y0	! .word	0xf12e
	movx.w	a0,@r4+r8	movy.w	@r7+,y1	! .word	0xf16e
	movx.w	a1,@r4+r8	movy.w	@r7+,y0	! .word	0xf1ae
	movx.w	a1,@r4+r8	movy.w	@r7+,y1	! .word	0xf1ee
	movx.w	a0,@r5+r8	movy.w	@r6+,y0	! .word	0xf22e
	movx.w	a0,@r5+r8	movy.w	@r6+,y1	! .word	0xf26e
	movx.w	a1,@r5+r8	movy.w	@r6+,y0	! .word	0xf2ae
	movx.w	a1,@r5+r8	movy.w	@r6+,y1	! .word	0xf2ee
	movx.w	a0,@r5+r8	movy.w	@r7+,y0	! .word	0xf32e
	movx.w	a0,@r5+r8	movy.w	@r7+,y1	! .word	0xf36e
	movx.w	a1,@r5+r8	movy.w	@r7+,y0	! .word	0xf3ae
	movx.w	a1,@r5+r8	movy.w	@r7+,y1	! .word	0xf3ee
	movx.w	a0,@r4+r8	movy.w	@r6+r9,y0	! .word	0xf02f
	movx.w	a0,@r4+r8	movy.w	@r6+r9,y1	! .word	0xf06f
	movx.w	a1,@r4+r8	movy.w	@r6+r9,y0	! .word	0xf0af
	movx.w	a1,@r4+r8	movy.w	@r6+r9,y1	! .word	0xf0ef
	movx.w	a0,@r4+r8	movy.w	@r7+r9,y0	! .word	0xf12f
	movx.w	a0,@r4+r8	movy.w	@r7+r9,y1	! .word	0xf16f
	movx.w	a1,@r4+r8	movy.w	@r7+r9,y0	! .word	0xf1af
	movx.w	a1,@r4+r8	movy.w	@r7+r9,y1	! .word	0xf1ef
	movx.w	a0,@r5+r8	movy.w	@r6+r9,y0	! .word	0xf22f
	movx.w	a0,@r5+r8	movy.w	@r6+r9,y1	! .word	0xf26f
	movx.w	a1,@r5+r8	movy.w	@r6+r9,y0	! .word	0xf2af
	movx.w	a1,@r5+r8	movy.w	@r6+r9,y1	! .word	0xf2ef
	movx.w	a0,@r5+r8	movy.w	@r7+r9,y0	! .word	0xf32f
	movx.w	a0,@r5+r8	movy.w	@r7+r9,y1	! .word	0xf36f
	movx.w	a1,@r5+r8	movy.w	@r7+r9,y0	! .word	0xf3af
	movx.w	a1,@r5+r8	movy.w	@r7+r9,y1	! .word	0xf3ef

movxwaxydxy:
	movx.w	@r4,x0	! 
	movx.w	@r4,y0	! 
	movx.w	@r4,x1	! 
	movx.w	@r4,y1	! 
	movx.w	@r0,x0	! 
	movx.w	@r0,y0	! 
	movx.w	@r0,x1	! 
	movx.w	@r0,y1	! 
	movx.w	@r5,x0	! 
	movx.w	@r5,y0	! 
	movx.w	@r5,x1	! 
	movx.w	@r5,y1	! 
	movx.w	@r1,x0	! 
	movx.w	@r1,y0	! 
	movx.w	@r1,x1	! 
	movx.w	@r1,y1	! 
	movx.w	@r4+,x0	! 
	movx.w	@r4+,y0	! 
	movx.w	@r4+,x1	! 
	movx.w	@r4+,y1	! 
	movx.w	@r0+,x0	! 
	movx.w	@r0+,y0	! 
	movx.w	@r0+,x1	! 
	movx.w	@r0+,y1	! 
	movx.w	@r5+,x0	! 
	movx.w	@r5+,y0	! 
	movx.w	@r5+,x1	! 
	movx.w	@r5+,y1	! 
	movx.w	@r1+,x0	! 
	movx.w	@r1+,y0	! 
	movx.w	@r1+,x1	! 
	movx.w	@r1+,y1	! 
	movx.w	@r4+r8,x0	! 
	movx.w	@r4+r8,y0	! 
	movx.w	@r4+r8,x1	! 
	movx.w	@r4+r8,y1	! 
	movx.w	@r0+r8,x0	! 
	movx.w	@r0+r8,y0	! 
	movx.w	@r0+r8,x1	! 
	movx.w	@r0+r8,y1	! 
	movx.w	@r5+r8,x0	! 
	movx.w	@r5+r8,y0	! 
	movx.w	@r5+r8,x1	! 
	movx.w	@r5+r8,y1	! 
	movx.w	@r1+r8,x0	! 
	movx.w	@r1+r8,y0	! 
	movx.w	@r1+r8,x1	! 
	movx.w	@r1+r8,y1	! 
	
movxwdaxaxy:	! 
	movx.w	a0,@r4	! 
	movx.w	x0,@r4	! 
	movx.w	a1,@r4	! 
	movx.w	x1,@r4	! 
	movx.w	a0,@r0	! 
	movx.w	x0,@r0	! 
	movx.w	a1,@r0	! 
	movx.w	x1,@r0	! 
	movx.w	a0,@r5	! 
	movx.w	x0,@r5	! 
	movx.w	a1,@r5	! 
	movx.w	x1,@r5	! 
	movx.w	a0,@r1	! 
	movx.w	x0,@r1	! 
	movx.w	a1,@r1	! 
	movx.w	x1,@r1	! 
	movx.w	a0,@r4+	! 
	movx.w	x0,@r4+	! 
	movx.w	a1,@r4+	! 
	movx.w	x1,@r4+	! 
	movx.w	a0,@r0+	! 
	movx.w	x0,@r0+	! 
	movx.w	a1,@r0+	! 
	movx.w	x1,@r0+	! 
	movx.w	a0,@r5+	! 
	movx.w	x0,@r5+	! 
	movx.w	a1,@r5+	! 
	movx.w	x1,@r5+	! 
	movx.w	a0,@r1+	! 
	movx.w	x0,@r1+	! 
	movx.w	a1,@r1+	! 
	movx.w	x1,@r1+	! 
	movx.w	a0,@r4+r8	! 
	movx.w	x0,@r4+r8	! 
	movx.w	a1,@r4+r8	! 
	movx.w	x1,@r4+r8	! 
	movx.w	a0,@r0+r8	! 
	movx.w	x0,@r0+r8	! 
	movx.w	a1,@r0+r8	! 
	movx.w	x1,@r0+r8	! 
	movx.w	a0,@r5+r8	! 
	movx.w	x0,@r5+r8	! 
	movx.w	a1,@r5+r8	! 
	movx.w	x1,@r5+r8	! 
	movx.w	a0,@r1+r8	! 
	movx.w	x0,@r1+r8	! 
	movx.w	a1,@r1+r8	! 
	movx.w	x1,@r1+r8	! 

movywayxdyx:	! 
	movy.w	@r6,y0	! 
	movy.w	@r6,y1	! 
	movy.w	@r6,x0	! 
	movy.w	@r6,x1	! 
	movy.w	@r7,y0	! 
	movy.w	@r7,y1	! 
	movy.w	@r7,x0	! 
	movy.w	@r7,x1	! 
	movy.w	@r2,y0	! 
	movy.w	@r2,y1	! 
	movy.w	@r2,x0	! 
	movy.w	@r2,x1	! 
	movy.w	@r3,y0	! 
	movy.w	@r3,y1	! 
	movy.w	@r3,x0	! 
	movy.w	@r3,x1	! 
	movy.w	@r6+,y0	! 
	movy.w	@r6+,y1	! 
	movy.w	@r6+,x0	! 
	movy.w	@r6+,x1	! 
	movy.w	@r7+,y0	! 
	movy.w	@r7+,y1	! 
	movy.w	@r7+,x0	! 
	movy.w	@r7+,x1	! 
	movy.w	@r2+,y0	! 
	movy.w	@r2+,y1	! 
	movy.w	@r2+,x0	! 
	movy.w	@r2+,x1	! 
	movy.w	@r3+,y0	! 
	movy.w	@r3+,y1	! 
	movy.w	@r3+,x0	! 
	movy.w	@r3+,x1	! 
	movy.w	@r6+r9,y0	! 
	movy.w	@r6+r9,y1	! 
	movy.w	@r6+r9,x0	! 
	movy.w	@r6+r9,x1	! 
	movy.w	@r7+r9,y0	! 
	movy.w	@r7+r9,y1	! 
	movy.w	@r7+r9,x0	! 
	movy.w	@r7+r9,x1	! 
	movy.w	@r2+r9,y0	! 
	movy.w	@r2+r9,y1	! 
	movy.w	@r2+r9,x0	! 
	movy.w	@r2+r9,x1	! 
	movy.w	@r3+r9,y0	! 
	movy.w	@r3+r9,y1	! 
	movy.w	@r3+r9,x0	! 
	movy.w	@r3+r9,x1	! 

movywdayayx:
	movy.w	a0,@r6
	movy.w	a1,@r6
	movy.w	y0,@r6
	movy.w	y1,@r6
	movy.w	a0,@r7
	movy.w	a1,@r7
	movy.w	y0,@r7
	movy.w	y1,@r7
	movy.w	a0,@r2
	movy.w	a1,@r2
	movy.w	y0,@r2
	movy.w	y1,@r2
	movy.w	a0,@r3
	movy.w	a1,@r3
	movy.w	y0,@r3
	movy.w	y1,@r3
	movy.w	a0,@r6+
	movy.w	a1,@r6+
	movy.w	y0,@r6+
	movy.w	y1,@r6+
	movy.w	a0,@r7+
	movy.w	a1,@r7+
	movy.w	y0,@r7+
	movy.w	y1,@r7+
	movy.w	a0,@r2+
	movy.w	a1,@r2+
	movy.w	y0,@r2+
	movy.w	y1,@r2+
	movy.w	a0,@r3+
	movy.w	a1,@r3+
	movy.w	y0,@r3+
	movy.w	y1,@r3+
	movy.w	a0,@r6+r9
	movy.w	a1,@r6+r9
	movy.w	y0,@r6+r9
	movy.w	y1,@r6+r9
	movy.w	a0,@r7+r9
	movy.w	a1,@r7+r9
	movy.w	y0,@r7+r9
	movy.w	y1,@r7+r9
	movy.w	a0,@r2+r9
	movy.w	a1,@r2+r9
	movy.w	y0,@r2+r9
	movy.w	y1,@r2+r9
	movy.w	a0,@r3+r9
	movy.w	a1,@r3+r9
	movy.w	y0,@r3+r9
	movy.w	y1,@r3+r9

	mov	r4, r0
	mov	r4, r1
	mov	r4, r2
	mov	r4, r3
	mov	r4, r5
	mov	r4, r6
	mov	r5, r7

movxlaxydxy:
	movx.l	@r4,x0
	movx.l	@r4,y0
	movx.l	@r4,x1
	movx.l	@r4,y1
	movx.l	@r0,x0
	movx.l	@r0,y0
	movx.l	@r0,x1
	movx.l	@r0,y1
	movx.l	@r5,x0
	movx.l	@r5,y0
	movx.l	@r5,x1
	movx.l	@r5,y1
	movx.l	@r1,x0
	movx.l	@r1,y0
	movx.l	@r1,x1
	movx.l	@r1,y1
	movx.l	@r4+,x0
	movx.l	@r4+,y0
	movx.l	@r4+,x1
	movx.l	@r4+,y1
	movx.l	@r0+,x0
	movx.l	@r0+,y0
	movx.l	@r0+,x1
	movx.l	@r0+,y1
	movx.l	@r5+,x0
	movx.l	@r5+,y0
	movx.l	@r5+,x1
	movx.l	@r5+,y1
	movx.l	@r1+,x0
	movx.l	@r1+,y0
	movx.l	@r1+,x1
	movx.l	@r1+,y1
	movx.l	@r4+r8,x0
	movx.l	@r4+r8,y0
	movx.l	@r4+r8,x1
	movx.l	@r4+r8,y1
	movx.l	@r0+r8,x0
	movx.l	@r0+r8,y0
	movx.l	@r0+r8,x1
	movx.l	@r0+r8,y1
	movx.l	@r5+r8,x0
	movx.l	@r5+r8,y0
	movx.l	@r5+r8,x1
	movx.l	@r5+r8,y1
	movx.l	@r1+r8,x0
	movx.l	@r1+r8,y0
	movx.l	@r1+r8,x1
	movx.l	@r1+r8,y1

movxldaxaxy:
	movx.l	a0,@r4
	movx.l	x0,@r4
	movx.l	a1,@r4
	movx.l	x1,@r4
	movx.l	a0,@r0
	movx.l	x0,@r0
	movx.l	a1,@r0
	movx.l	x1,@r0
	movx.l	a0,@r5
	movx.l	x0,@r5
	movx.l	a1,@r5
	movx.l	x1,@r5
	movx.l	a0,@r1
	movx.l	x0,@r1
	movx.l	a1,@r1
	movx.l	x1,@r1
	movx.l	a0,@r4+
	movx.l	x0,@r4+
	movx.l	a1,@r4+
	movx.l	x1,@r4+
	movx.l	a0,@r0+
	movx.l	x0,@r0+
	movx.l	a1,@r0+
	movx.l	x1,@r0+
	movx.l	a0,@r5+
	movx.l	x0,@r5+
	movx.l	a1,@r5+
	movx.l	x1,@r5+
	movx.l	a0,@r1+
	movx.l	x0,@r1+
	movx.l	a1,@r1+
	movx.l	x1,@r1+
	movx.l	a0,@r4+r8
	movx.l	x0,@r4+r8
	movx.l	a1,@r4+r8
	movx.l	x1,@r4+r8
	movx.l	a0,@r0+r8
	movx.l	x0,@r0+r8
	movx.l	a1,@r0+r8
	movx.l	x1,@r0+r8
	movx.l	a0,@r5+r8
	movx.l	x0,@r5+r8
	movx.l	a1,@r5+r8
	movx.l	x1,@r5+r8
	movx.l	a0,@r1+r8
	movx.l	x0,@r1+r8
	movx.l	a1,@r1+r8
	movx.l	x1,@r1+r8

movylayxdyx:
	movy.l	@r6,y0
	movy.l	@r6,y1
	movy.l	@r6,x0
	movy.l	@r6,x1
	movy.l	@r7,y0
	movy.l	@r7,y1
	movy.l	@r7,x0
	movy.l	@r7,x1
	movy.l	@r2,y0
	movy.l	@r2,y1
	movy.l	@r2,x0
	movy.l	@r2,x1
	movy.l	@r3,y0
	movy.l	@r3,y1
	movy.l	@r3,x0
	movy.l	@r3,x1
	movy.l	@r6+,y0
	movy.l	@r6+,y1
	movy.l	@r6+,x0
	movy.l	@r6+,x1
	movy.l	@r7+,y0
	movy.l	@r7+,y1
	movy.l	@r7+,x0
	movy.l	@r7+,x1
	movy.l	@r2+,y0
	movy.l	@r2+,y1
	movy.l	@r2+,x0
	movy.l	@r2+,x1
	movy.l	@r3+,y0
	movy.l	@r3+,y1
	movy.l	@r3+,x0
	movy.l	@r3+,x1
	movy.l	@r6+r9,y0
	movy.l	@r6+r9,y1
	movy.l	@r6+r9,x0
	movy.l	@r6+r9,x1
	movy.l	@r7+r9,y0
	movy.l	@r7+r9,y1
	movy.l	@r7+r9,x0
	movy.l	@r7+r9,x1
	movy.l	@r2+r9,y0
	movy.l	@r2+r9,y1
	movy.l	@r2+r9,x0
	movy.l	@r2+r9,x1
	movy.l	@r3+r9,y0
	movy.l	@r3+r9,y1
	movy.l	@r3+r9,x0
	movy.l	@r3+r9,x1

movyldayayx:
	movy.l	a0,@r6
	movy.l	a1,@r6
	movy.l	y0,@r6
	movy.l	y1,@r6
	movy.l	a0,@r7
	movy.l	a1,@r7
	movy.l	y0,@r7
	movy.l	y1,@r7
	movy.l	a0,@r2
	movy.l	a1,@r2
	movy.l	y0,@r2
	movy.l	y1,@r2
	movy.l	a0,@r3
	movy.l	a1,@r3
	movy.l	y0,@r3
	movy.l	y1,@r3
	movy.l	a0,@r6+
	movy.l	a1,@r6+
	movy.l	y0,@r6+
	movy.l	y1,@r6+
	movy.l	a0,@r7+
	movy.l	a1,@r7+
	movy.l	y0,@r7+
	movy.l	y1,@r7+
	movy.l	a0,@r2+
	movy.l	a1,@r2+
	movy.l	y0,@r2+
	movy.l	y1,@r2+
	movy.l	a0,@r3+
	movy.l	a1,@r3+
	movy.l	y0,@r3+
	movy.l	y1,@r3+
	movy.l	a0,@r6+r9
	movy.l	a1,@r6+r9
	movy.l	y0,@r6+r9
	movy.l	y1,@r6+r9
	movy.l	a0,@r7+r9
	movy.l	a1,@r7+r9
	movy.l	y0,@r7+r9
	movy.l	y1,@r7+r9
	movy.l	a0,@r2+r9
	movy.l	a1,@r2+r9
	movy.l	y0,@r2+r9
	movy.l	y1,@r2+r9
	movy.l	a0,@r3+r9
	movy.l	a1,@r3+r9
	movy.l	y0,@r3+r9
	movy.l	y1,@r3+r9

	pass
	exit 0
