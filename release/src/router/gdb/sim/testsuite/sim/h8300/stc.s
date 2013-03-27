# Hitachi H8 testcase 'stc'
# mach(): all
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"
	.data
byte_dest1:
	.byte	0
	.byte	0
byte_dest2:
	.byte	0
	.byte	0
byte_dest3:
	.byte	0
	.byte	0
byte_dest4:
	.byte	0
	.byte	0
byte_dest5:
	.byte	0
	.byte	0
byte_dest6:
	.byte	0
	.byte	0
byte_dest7:
	.byte	0
	.byte	0
byte_dest8:
	.byte	0
	.byte	0
byte_dest9:
	.byte	0
	.byte	0
byte_dest10:
	.byte	0
	.byte	0
byte_dest11:
	.byte	0
	.byte	0
byte_dest12:
	.byte	0
	.byte	0
	
	start

stc_ccr_reg8:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0xff, ccr	; test value
	stc	ccr, r0h	; copy test value to r0h

	test_h_gr16  0xffa5 r0	; ff in r0h, a5 in r0l
.if (sim_cpu)			; h/s/sx
	test_h_gr32  0xa5a5ffa5 er0	; ff in r0h, a5 everywhere else
.endif
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu == h8300s || sim_cpu == h8sx)	; Earlier versions, no exr
stc_exr_reg8:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0x87, exr	; set exr to 0x87
	stc	exr, r0l	; retrieve and check exr value
	cmp.b	#0x87, r0l
	beq	.L21
	fail
.L21:
	test_h_gr32  0xa5a5a587 er0	; Register 0 modified by test procedure.
	test_gr_a5a5 1		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

stc_ccr_abs16:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0xff, ccr
	stc	ccr, @byte_dest1:16	; abs16 dest

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

stc_exr_abs16:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0x87, exr
	stc	exr, @byte_dest2:16	; abs16 dest

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

stc_ccr_abs32:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0xff, ccr
	stc	ccr, @byte_dest3:32	; abs32 dest

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

stc_exr_abs32:
	set_grs_a5a5
	set_ccr_zero

	ldc	#0x87, exr
	stc	exr, @byte_dest4:32	; abs32 dest

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

stc_ccr_disp16:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_dest5-1, er1
	ldc	#0xff, ccr
	stc	ccr, @(1:16,er1)	; disp16 dest (5)

	test_h_gr32 byte_dest5-1, er1	; er1 still contains address

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

stc_exr_disp16:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_dest6+1, er1
	ldc	#0x87, exr
	stc	exr, @(-1:16,er1)	; disp16 dest (6)

	test_h_gr32 byte_dest6+1, er1	; er1 still contains address

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

stc_ccr_disp32:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_dest7-1, er1
	ldc	#0xff, ccr
	stc	ccr, @(1:32,er1)	; disp32 dest (7)

	test_h_gr32 byte_dest7-1, er1	; er1 still contains address

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

stc_exr_disp32:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_dest8+1, er1
	ldc	#0x87, exr
	stc	exr, @(-1:32,er1)	; disp16 dest (8)

	test_h_gr32 byte_dest8+1, er1	; er1 still contains address

	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

stc_ccr_predecr:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_dest9+2, er1
	ldc	#0xff, ccr
	stc	ccr, @-er1	; predecr dest (9)

	test_h_gr32  byte_dest9 er1	; er1 still contains address

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
stc_exr_predecr:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_dest10+2, er1
	ldc	#0x87, exr
	stc	exr, @-er1	; predecr dest (10)

	test_h_gr32 byte_dest10, er1	; er1 still contains address

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
stc_ccr_ind:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_dest11, er1
	ldc	#0xff, ccr
	stc	ccr, @er1	; postinc dest (11)

	test_h_gr32 byte_dest11, er1	; er1 still contains address

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
stc_exr_ind:
	set_grs_a5a5
	set_ccr_zero

	mov	#byte_dest12, er1
	ldc	#0x87, exr
	stc	exr, @er1, exr	; postinc dest (12)

	test_h_gr32 byte_dest12, er1	; er1 still contains address

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	
.endif
	
.if (sim_cpu == h8sx)		; New vbr and sbr registers for h8sx
stc_sbr_reg:
	set_grs_a5a5
	set_ccr_zero

	mov	#0xaaaaaaaa, er0
	ldc	er0, sbr	; set sbr to 0xaaaaaaaa
 	stc	sbr, er1	; retreive and check sbr value

	test_h_gr32 0xaaaaaaaa er1
	test_h_gr32 0xaaaaaaaa er0 ; Register 0 modified by test procedure.
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

stc_vbr_reg:	
	set_grs_a5a5
	set_ccr_zero

	mov	#0xaaaaaaaa, er0
	ldc	er0, vbr	; set sbr to 0xaaaaaaaa
	stc	vbr, er1	; retreive and check sbr value

	test_h_gr32 0xaaaaaaaa er1
	test_h_gr32 0xaaaaaaaa er0 ; Register 0 modified by test procedure.
	test_gr_a5a5 2		; Make sure other general regs not disturbed
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

check_results:
	;; Now check results
	mov @byte_dest1, r0h
	cmp.b	#0xff, r0h
	beq .L1
	fail

.L1:	mov @byte_dest2, r0h
	cmp.b	#0x87, r0h
	beq .L2
	fail

.L2:	mov @byte_dest3, r0h
	cmp.b	#0xff, r0h
	beq .L3
	fail

.L3:	mov @byte_dest4, r0h
	cmp.b	#0x87, r0h
	beq .L4
	fail

.L4:	mov @byte_dest5, r0h
	cmp.b	#0xff, r0h
	beq .L5
	fail

.L5:	mov @byte_dest6, r0h
	cmp.b	#0x87, r0h
	beq .L6
	fail

.L6:	mov @byte_dest7, r0h
	cmp.b	#0xff, r0h
	beq .L7
	fail

.L7:	mov @byte_dest8, r0h
	cmp.b	#0x87, r0h
	beq .L8
	fail

.L8:	mov @byte_dest9, r0h
	cmp.b	#0xff, r0h
	beq .L9
	fail

.L9:	mov @byte_dest10, r0h
	cmp.b	#0x87, r0h
	beq .L10
	fail

.L10:	mov @byte_dest11, r0h
	cmp.b	#0xff, r0h
	beq .L11
	fail

.L11:	mov @byte_dest12, r0h
	cmp.b	#0x87, r0h
	beq .L12
	fail

.L12:	
.endif
	pass

	exit 0
