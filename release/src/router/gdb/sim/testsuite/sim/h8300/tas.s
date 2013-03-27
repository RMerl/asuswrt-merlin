# Hitachi H8 testcase 'tas'
# mach(): h8300h h8300s h8sx
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf	
# ld(h8300s):	-m h8300self	
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"

	.data
byte_dst:	.byte 0

	start

tas_ind:			; test and set instruction
	set_grs_a5a5
	mov	#byte_dst, er4
	set_ccr_zero
	;; tas @erd
	tas	@er4		; should set zero flag
	test_carry_clear
	test_neg_clear
	test_ovf_clear
	test_zero_set

	tas	@er4		; should clear zero, set neg
	test_carry_clear
	test_neg_set
	test_ovf_clear
	test_zero_clear

	test_gr_a5a5 0		; general regs have not been modified
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_h_gr32  byte_dst, er4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	mov.b	@byte_dst, r0l	; test variable has MSB set?
	test_h_gr8 0x80 r0l

.if (sim_cpu == h8sx)		; h8sx can use any register for tas
tas_h8sx:			; test and set instruction
	mov.b	#0, @byte_dst
	set_grs_a5a5
	mov	#byte_dst, er3
	set_ccr_zero
	;; tas @erd
	tas	@er3		; should set zero flag
	test_carry_clear
	test_neg_clear
	test_ovf_clear
	test_zero_set

	tas	@er3		; should clear zero, set neg
	test_carry_clear
	test_neg_set
	test_ovf_clear
	test_zero_clear

	test_gr_a5a5 0		; general regs have not been modified
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_h_gr32  byte_dst, er3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

	mov.b	@byte_dst, r0l	; test variable has MSB set?
	test_h_gr8 0x80 r0l
.endif				; h8sx

	pass
	exit 0
