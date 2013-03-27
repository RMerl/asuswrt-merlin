# Hitachi H8 testcase 'mac'
# mach(): h8300s h8sx
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"

	.data
src1:	.word	0
src2:	.word	0

array:	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff
	.word	0x7fff

	start

.if (sim_cpu)
_clrmac:
	set_grs_a5a5
	set_ccr_zero
	clrmac
	test_cc_clear
	test_grs_a5a5
	;; Now see if the mac is actually clear...
	stmac	mach, er0
	test_zero_set
	test_neg_clear
	test_ovf_clear
	test_h_gr32 0 er0
	stmac	macl, er1
	test_zero_set
	test_neg_clear
	test_ovf_clear
	test_h_gr32 0 er1

ld_stmac:
	set_grs_a5a5
	sub.l	er2, er2
	set_ccr_zero
	ldmac	er1, macl
	stmac	macl, er2
	test_ovf_clear
	test_carry_clear
	;; neg and zero are undefined
	test_h_gr32 0xa5a5a5a5 er2

	sub.l	er2, er2
	set_ccr_zero
	ldmac	er1, mach
	stmac	mach, er2
	test_ovf_clear
	test_carry_clear
	;; neg and zero are undefined
	test_h_gr32 0x0001a5 er2

	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

mac_2x2:
	set_grs_a5a5
	mov.w	#2, r1
	mov.w	r1, @src1
	mov.w	#2, r2
	mov.w	r2, @src2
	mov	#src1, er1
	mov	#src2, er2
	set_ccr_zero
	clrmac
	mac	@er1+, @er2+
	test_cc_clear
	
	test_h_gr32 0xa5a5a5a5 er0
	test_h_gr32 src1+2     er1
	test_h_gr32 src2+2     er2
	test_h_gr32 0xa5a5a5a5 er3
	test_h_gr32 0xa5a5a5a5 er4
	test_h_gr32 0xa5a5a5a5 er5
	test_h_gr32 0xa5a5a5a5 er6
	test_h_gr32 0xa5a5a5a5 er7

	stmac	macl, er0
	test_zero_clear
	test_neg_clear
	test_ovf_clear
	test_h_gr32 4 er0

	stmac	mach, er0
	test_zero_clear
	test_neg_clear
	test_ovf_clear
	test_h_gr32 0 er0

mac_same_reg_2x4:
	;; Use same reg for src and dst.  Should be incremented twice,
	;; and fetch values from consecutive locations.
	set_grs_a5a5
	mov.w	#2, r1
	mov.w	r1, @src1
	mov.w	#4, r2
	mov.w	r2, @src2
	mov	#src1, er1

	set_ccr_zero
	clrmac
	mac	@er1+, @er1+	; same register for src and dst
	test_cc_clear
	
	test_h_gr32 0xa5a5a5a5 er0
	test_h_gr32 src1+4     er1
	test_h_gr32 0xa5a50004 er2
	test_h_gr32 0xa5a5a5a5 er3
	test_h_gr32 0xa5a5a5a5 er4
	test_h_gr32 0xa5a5a5a5 er5
	test_h_gr32 0xa5a5a5a5 er6
	test_h_gr32 0xa5a5a5a5 er7

	stmac	macl, er0
	test_zero_clear
	test_neg_clear
	test_ovf_clear
	test_h_gr32 8 er0

	stmac	mach, er0
	test_zero_clear
	test_neg_clear
	test_ovf_clear
	test_h_gr32 0 er0

mac_0x0:
	set_grs_a5a5
	mov.w	#0, r1
	mov.w	r1, @src1
	mov.w	#0, r2
	mov.w	r2, @src2
	mov	#src1, er1
	mov	#src2, er2
	set_ccr_zero
	clrmac
	mac	@er1+, @er2+
	test_cc_clear
	
	test_h_gr32 0xa5a5a5a5 er0
	test_h_gr32 src1+2     er1
	test_h_gr32 src2+2     er2
	test_h_gr32 0xa5a5a5a5 er3
	test_h_gr32 0xa5a5a5a5 er4
	test_h_gr32 0xa5a5a5a5 er5
	test_h_gr32 0xa5a5a5a5 er6
	test_h_gr32 0xa5a5a5a5 er7

	stmac	macl, er0
	test_zero_set		; zero flag is set
	test_neg_clear
	test_ovf_clear
	test_h_gr32 0 er0	; result is zero

	stmac	mach, er0
	test_zero_set
	test_neg_clear
	test_ovf_clear
	test_h_gr32 0 er0

mac_neg2x2:
	set_grs_a5a5
	mov.w	#-2, r1
	mov.w	r1, @src1
	mov.w	#2, r2
	mov.w	r2, @src2
	mov	#src1, er1
	mov	#src2, er2
	set_ccr_zero
	clrmac
	mac	@er1+, @er2+
	test_cc_clear
	
	test_h_gr32 0xa5a5a5a5 er0
	test_h_gr32 src1+2     er1
	test_h_gr32 src2+2     er2
	test_h_gr32 0xa5a5a5a5 er3
	test_h_gr32 0xa5a5a5a5 er4
	test_h_gr32 0xa5a5a5a5 er5
	test_h_gr32 0xa5a5a5a5 er6
	test_h_gr32 0xa5a5a5a5 er7

	stmac	macl, er0
	test_zero_clear
	test_neg_set		; neg flag is set
	test_ovf_clear
	test_h_gr32 -4 er0	; result is negative

	stmac	mach, er0
	test_zero_clear
	test_neg_set
	test_ovf_clear
	test_h_gr32 -1 er0	; negative sign extend

mac_array:
	;; Use same reg for src and dst, pointing to an array of shorts
	set_grs_a5a5
	mov	#array, er1

	set_ccr_zero
	clrmac
	mac	@er1+, @er1+	; same register for src and dst
	mac	@er1+, @er1+	; repeat 8 times
	mac	@er1+, @er1+
	mac	@er1+, @er1+
	mac	@er1+, @er1+
	mac	@er1+, @er1+
	mac	@er1+, @er1+
	mac	@er1+, @er1+
	test_cc_clear
	
	test_h_gr32 0xa5a5a5a5 er0
	test_h_gr32 array+32     er1
	test_h_gr32 0xa5a5a5a5 er2
	test_h_gr32 0xa5a5a5a5 er3
	test_h_gr32 0xa5a5a5a5 er4
	test_h_gr32 0xa5a5a5a5 er5
	test_h_gr32 0xa5a5a5a5 er6
	test_h_gr32 0xa5a5a5a5 er7

	stmac	macl, er0
	test_zero_clear
	test_neg_clear
	test_ovf_clear
	test_h_gr32 0xfff80008 er0

	stmac	mach, er0
	test_zero_clear
	test_neg_clear
	test_ovf_clear
	test_h_gr32 1 er0	; result is greater than 32 bits

.endif

	pass

	exit 0
