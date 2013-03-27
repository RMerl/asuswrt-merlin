# Hitachi H8 testcase 'bset', 'bclr'
# mach(): all
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf	
# ld(h8300s):	-m h8300self	
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"

	# Instructions tested:
	#
	# bset xx:3, rd8	;                   7 0 ?xxx rd8
	# bclr xx:3, rd8	;                   7 2 ?xxx rd8
	# bset xx:3, @erd	; 7 d 0rd ????      7 0 ?xxx ????
	# bclr xx:3, @erd	; 7 d 0rd ????      7 2 ?xxx ????
	# bset xx:3, @abs16	; 6 a 1 1??? aa:16  7 0 ?xxx ????
	# bclr xx:3, @abs16	; 6 a 1 1??? aa:16  7 2 ?xxx ???? 
	# bset reg8, rd8	;                   6 0 rs8  rd8
	# bclr reg8, rd8	;                   6 2 rs8  rd8
	# bset reg8, @erd	; 7 d 0rd ????      6 0 rs8  ????
	# bclr reg8, @erd	; 7 d 0rd ????      6 2 rs8  ????
	# bset reg8, @abs32	; 6 a 3 1??? aa:32  6 0 rs8  ????
	# bclr reg8, @abs32	; 6 a 3 1??? aa:32  6 2 rs8  ???? 
	#
	# bset/eq xx:3, rd8
	# bclr/eq xx:3, rd8
	# bset/ne xx:3, rd8
	# bclr/ne xx:3, rd8

	.data
byte_dst:	.byte 0

	start

bset_imm3_reg8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  bset xx:3, rd8
	mov	#0, r1l
	set_ccr_zero
	bset	#0, r1l
	test_cc_clear
	test_h_gr8 1 r1l

	set_ccr_zero
	bset	#1, r1l
	test_cc_clear
	test_h_gr8 3 r1l

	set_ccr_zero
	bset	#2, r1l
	test_cc_clear
	test_h_gr8 7 r1l

	set_ccr_zero
	bset	#3, r1l
	test_cc_clear
	test_h_gr8 15 r1l

	set_ccr_zero
	bset	#4, r1l
	test_cc_clear
	test_h_gr8 31 r1l

	set_ccr_zero
	bset	#5, r1l
	test_cc_clear
	test_h_gr8 63 r1l

	set_ccr_zero
	bset	#6, r1l
	test_cc_clear
	test_h_gr8 127 r1l

	set_ccr_zero
	bset	#7, r1l
	test_cc_clear
	test_h_gr8 255 r1l

.if (sim_cpu == h8300)
	test_h_gr16 0xa5ff, r1
.else
	test_h_gr32  0xa5a5a5ff er1
.endif

bclr_imm3_reg8:	
	set_ccr_zero
	bclr	#7, r1l
	test_cc_clear
	test_h_gr8 127 r1l

	set_ccr_zero
	bclr	#6, r1l
	test_cc_clear
	test_h_gr8 63 r1l

	set_ccr_zero
	bclr	#5, r1l
	test_cc_clear
	test_h_gr8 31 r1l

	set_ccr_zero
	bclr	#4, r1l
	test_cc_clear
	test_h_gr8 15 r1l

	set_ccr_zero
	bclr	#3, r1l
	test_cc_clear
	test_h_gr8 7 r1l

	set_ccr_zero
	bclr	#2, r1l
	test_cc_clear
	test_h_gr8 3 r1l

	set_ccr_zero
	bclr	#1, r1l
	test_cc_clear
	test_h_gr8 1 r1l

	set_ccr_zero
	bclr	#0, r1l
	test_cc_clear
	test_h_gr8 0 r1l

	test_gr_a5a5 0		; Make sure other general regs not disturbed
.if (sim_cpu == h8300)
	test_h_gr16 0xa500 r1
.else
	test_h_gr32  0xa5a5a500 er1
.endif
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu)
bset_imm3_ind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  bset xx:3, @erd
	mov	#byte_dst, er1
	set_ccr_zero
	bset	#0, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 1 r2l

	set_ccr_zero
	bset	#1, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 3 r2l

	set_ccr_zero
	bset	#2, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 7 r2l

	set_ccr_zero
	bset	#3, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 15 r2l

	set_ccr_zero
	bset	#4, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 31 r2l

	set_ccr_zero
	bset	#5, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 63 r2l

	set_ccr_zero
	bset	#6, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 127 r2l

	set_ccr_zero
	bset	#7, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 255 r2l

.if (sim_cpu == h8300)
	test_h_gr16  0xa5ff r2
.else
	test_h_gr32  0xa5a5a5ff er2
.endif

bclr_imm3_ind:	
	set_ccr_zero
	bclr	#7, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 127 r2l

	set_ccr_zero
	bclr	#6, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 63 r2l

	set_ccr_zero
	bclr	#5, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 31 r2l

	set_ccr_zero
	bclr	#4, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 15 r2l

	set_ccr_zero
	bclr	#3, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 7  r2l

	set_ccr_zero
	bclr	#2, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 3  r2l

	set_ccr_zero
	bclr	#1, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 1  r2l

	set_ccr_zero
	bclr	#0, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 0  r2l

	test_gr_a5a5 0		; Make sure other general regs not disturbed
.if (sim_cpu == h8300)
	test_h_gr16  byte_dst r1
	test_h_gr16  0xa500   r2
.else
	test_h_gr32  byte_dst   er1
	test_h_gr32  0xa5a5a500 er2
.endif
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu > h8300h)
bset_imm3_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  bset xx:3, @aa:16
	set_ccr_zero
	bset	#0, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 1 r2l

	set_ccr_zero
	bset	#1, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 3 r2l

	set_ccr_zero
	bset	#2, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 7 r2l

	set_ccr_zero
	bset	#3, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 15 r2l

	set_ccr_zero
	bset	#4, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 31 r2l

	set_ccr_zero
	bset	#5, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 63 r2l

	set_ccr_zero
	bset	#6, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 127 r2l

	set_ccr_zero
	bset	#7, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 255 r2l

.if (sim_cpu == h8300)
	test_h_gr16  0xa5ff r2
.else
	test_h_gr32  0xa5a5a5ff er2
.endif

bclr_imm3_abs16:	
	set_ccr_zero
	bclr	#7, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 127 r2l

	set_ccr_zero
	bclr	#6, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 63 r2l

	set_ccr_zero
	bclr	#5, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 31 r2l

	set_ccr_zero
	bclr	#4, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 15 r2l

	set_ccr_zero
	bclr	#3, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 7  r2l

	set_ccr_zero
	bclr	#2, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 3  r2l

	set_ccr_zero
	bclr	#1, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 1  r2l

	set_ccr_zero
	bclr	#0, @byte_dst:16
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 0  r2l

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
.if (sim_cpu == h8300)
	test_h_gr16  0xa500   r2
.else
	test_h_gr32  0xa5a5a500 er2
.endif
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif
.endif

bset_rs8_rd8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  bset rs8, rd8
	mov	#0, r1h
	mov	#0, r1l
	set_ccr_zero
	bset	r1h, r1l
	test_cc_clear
	test_h_gr8 1 r1l

	mov	#1, r1h
	set_ccr_zero
	bset	r1h, r1l
	test_cc_clear
	test_h_gr8 3 r1l

	mov	#2, r1h
	set_ccr_zero
	bset	r1h, r1l
	test_cc_clear
	test_h_gr8 7 r1l

	mov	#3, r1h
	set_ccr_zero
	bset	r1h, r1l
	test_cc_clear
	test_h_gr8 15 r1l

	mov	#4, r1h
	set_ccr_zero
	bset	r1h, r1l
	test_cc_clear
	test_h_gr8 31 r1l

	mov	#5, r1h
	set_ccr_zero
	bset	r1h, r1l
	test_cc_clear
	test_h_gr8 63 r1l

	mov	#6, r1h
	set_ccr_zero
	bset	r1h, r1l
	test_cc_clear
	test_h_gr8 127 r1l

	mov	#7, r1h
	set_ccr_zero
	bset	r1h, r1l
	test_cc_clear
	test_h_gr8 255 r1l

.if (sim_cpu == h8300)
	test_h_gr16 0x07ff, r1
.else
	test_h_gr32  0xa5a507ff er1
.endif

bclr_rs8_rd8:	
	mov	#7, r1h
	set_ccr_zero
	bclr	r1h, r1l
	test_cc_clear
	test_h_gr8 127 r1l

	mov	#6, r1h
	set_ccr_zero
	bclr	r1h, r1l
	test_cc_clear
	test_h_gr8 63 r1l

	mov	#5, r1h
	set_ccr_zero
	bclr	r1h, r1l
	test_cc_clear
	test_h_gr8 31 r1l

	mov	#4, r1h
	set_ccr_zero
	bclr	r1h, r1l
	test_cc_clear
	test_h_gr8 15 r1l

	mov	#3, r1h
	set_ccr_zero
	bclr	r1h, r1l
	test_cc_clear
	test_h_gr8 7 r1l

	mov	#2, r1h
	set_ccr_zero
	bclr	r1h, r1l
	test_cc_clear
	test_h_gr8 3 r1l

	mov	#1, r1h
	set_ccr_zero
	bclr	r1h, r1l
	test_cc_clear
	test_h_gr8 1 r1l

	mov	#0, r1h
	set_ccr_zero
	bclr	r1h, r1l
	test_cc_clear
	test_h_gr8 0 r1l

	test_gr_a5a5 0		; Make sure other general regs not disturbed
.if (sim_cpu == h8300)
	test_h_gr16 0x0000 r1
.else
	test_h_gr32  0xa5a50000 er1
.endif
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu)
bset_rs8_ind:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  bset rs8, @erd
	mov	#byte_dst, er1
	mov	#0, r2h
	set_ccr_zero
	bset	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 1 r2l

	mov	#1, r2h
	set_ccr_zero
	bset	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 3 r2l

	mov	#2, r2h
	set_ccr_zero
	bset	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 7 r2l

	mov	#3, r2h
	set_ccr_zero
	bset	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 15 r2l

	mov	#4, r2h
	set_ccr_zero
	bset	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 31 r2l

	mov	#5, r2h
	set_ccr_zero
	bset	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 63 r2l

	mov	#6, r2h
	set_ccr_zero
	bset	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 127 r2l

	mov	#7, r2h
	set_ccr_zero
	bset	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 255 r2l

.if (sim_cpu == h8300)
	test_h_gr16  0x07ff r2
.else
	test_h_gr32  0xa5a507ff er2
.endif

bclr_rs8_ind:	
	mov	#7, r2h
	set_ccr_zero
	bclr	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 127 r2l

	mov	#6, r2h
	set_ccr_zero
	bclr	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 63 r2l

	mov	#5, r2h
	set_ccr_zero
	bclr	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 31 r2l

	mov	#4, r2h
	set_ccr_zero
	bclr	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 15 r2l

	mov	#3, r2h
	set_ccr_zero
	bclr	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 7  r2l

	mov	#2, r2h
	set_ccr_zero
	bclr	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 3  r2l

	mov	#1, r2h
	set_ccr_zero
	bclr	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 1  r2l

	mov	#0, r2h
	set_ccr_zero
	bclr	r2h, @er1
	test_cc_clear
	mov	@er1, r2l
	test_h_gr8 0  r2l

	test_gr_a5a5 0		; Make sure other general regs not disturbed
.if (sim_cpu == h8300)
	test_h_gr16  byte_dst r1
	test_h_gr16  0x0000   r2
.else
	test_h_gr32  byte_dst   er1
	test_h_gr32  0xa5a50000 er2
.endif
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu > h8300h)
bset_rs8_abs32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  bset rs8, @aa:32
	mov	#0, r2h
	set_ccr_zero
	bset	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 1 r2l

	mov	#1, r2h
	set_ccr_zero
	bset	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 3 r2l

	mov	#2, r2h
	set_ccr_zero
	bset	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 7 r2l

	mov	#3, r2h
	set_ccr_zero
	bset	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 15 r2l

	mov	#4, r2h
	set_ccr_zero
	bset	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 31 r2l

	mov	#5, r2h
	set_ccr_zero
	bset	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 63 r2l

	mov	#6, r2h
	set_ccr_zero
	bset	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 127 r2l

	mov	#7, r2h
	set_ccr_zero
	bset	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 255 r2l

.if (sim_cpu == h8300)
	test_h_gr16  0x07ff r2
.else
	test_h_gr32  0xa5a507ff er2
.endif

bclr_rs8_abs32:	
	mov	#7, r2h
	set_ccr_zero
	bclr	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 127 r2l

	mov	#6, r2h
	set_ccr_zero
	bclr	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 63 r2l

	mov	#5, r2h
	set_ccr_zero
	bclr	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 31 r2l

	mov	#4, r2h
	set_ccr_zero
	bclr	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 15 r2l

	mov	#3, r2h
	set_ccr_zero
	bclr	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 7  r2l

	mov	#2, r2h
	set_ccr_zero
	bclr	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 3  r2l

	mov	#1, r2h
	set_ccr_zero
	bclr	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 1  r2l

	mov	#0, r2h
	set_ccr_zero
	bclr	r2h, @byte_dst:32
	test_cc_clear
	mov	@byte_dst, r2l
	test_h_gr8 0  r2l

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
.if (sim_cpu == h8300)
	test_h_gr16  0x0000   r2
.else
	test_h_gr32  0xa5a50000 er2
.endif
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif
.endif

.if (sim_cpu == h8sx)
bset_eq_imm3_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  bset/eq xx:3, rd8
	mov	#0, @byte_dst
	set_ccr_zero
	bset/eq	#0, @byte_dst:16 ; Zero is clear, should have no effect.
	test_cc_clear
	mov	@byte_dst, r1l
	test_h_gr8 0 r1l

	set_ccr_zero
	orc	#4, ccr		; Set zero flag
	bset/eq	#0, @byte_dst:16 ; Zero is set: operation should succeed.

	test_neg_clear
	test_zero_set
	test_ovf_clear
	test_carry_clear

	mov	@byte_dst, r1l
	test_h_gr8 1 r1l

bclr_eq_imm3_abs32:
	mov	#1, @byte_dst
	set_ccr_zero
	bclr/eq	#0, @byte_dst:32 ; Zero is clear, should have no effect.
	test_cc_clear
	mov	@byte_dst, r1l
	test_h_gr8 1 r1l

	set_ccr_zero
	orc	#4, ccr		; Set zero flag
	bclr/eq	#0, @byte_dst:32 ; Zero is set: operation should succeed.
	test_neg_clear
	test_zero_set
	test_ovf_clear
	test_carry_clear
	mov	@byte_dst, r1l
	test_h_gr8 0 r1l

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32  0xa5a5a500 er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

bset_ne_imm3_abs16:
	set_grs_a5a5		; Fill all general regs with a fixed pattern

	;;  bset/ne xx:3, aa:16
	mov	#0, @byte_dst
	set_ccr_zero
	orc	#4, ccr		; Set zero flag
	bset/ne	#0, @byte_dst:16 ; Zero is set; should have no effect.
	test_zero_set
	test_neg_clear
	test_ovf_clear
	test_carry_clear
	mov	@byte_dst, r1l
	test_h_gr8 0 r1l

	set_ccr_zero
	bset/ne	#0, @byte_dst:16 ; Zero is clear: operation should succeed.
	test_cc_clear
	mov	@byte_dst, r1l
	test_h_gr8 1 r1l

bclr_ne_imm3_abs32:
	mov	#1, @byte_dst
	set_ccr_zero
	orc	#4, ccr		; Set zero flag
	;; bclr/ne xx:3, aa:16
	bclr/ne	#0, @byte_dst:32 ; Zero is set, should have no effect.
	test_neg_clear
	test_zero_set
	test_ovf_clear
	test_carry_clear
	mov	@byte_dst, r1l
	test_h_gr8 1 r1l

	set_ccr_zero
	bclr/ne	#0, @byte_dst:32 ; Zero is clear: operation should succeed.
	test_cc_clear
	mov	@byte_dst, r1l
	test_h_gr8 0 r1l

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_h_gr32  0xa5a5a500 er1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif

	pass
	exit 0
