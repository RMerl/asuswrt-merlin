# Hitachi H8 testcase 'muls', 'muls/u', mulu', 'mulu/u', 'mulxs', 'mulxu'
# mach(): all
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf	

	.include "testutils.inc"

	start

.if (sim_cpu == h8sx)	
muls_w_reg_reg:	
	set_grs_a5a5

	;; muls.w rs, rd
	mov.w	#32, r1
	mov.w	#-2, r2
	set_ccr_zero
	muls.w	r2, r1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	test_gr_a5a5	0
	test_h_gr16	-64	r1
	test_h_gr32	0xa5a5fffe	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

muls_w_imm4_reg:
	set_grs_a5a5

	;; muls.w xx:4, rd
	mov.w	#-32, r1
	set_ccr_zero
	muls.w	#2:4, r1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_h_gr16	-64	r1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

muls_l_reg_reg:	
	set_grs_a5a5

	;; muls.l ers, erd
	mov.l	#320000, er1
	mov.l	#-2, er2
	set_ccr_zero
	muls.l	er2, er1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	test_gr_a5a5	0
	test_h_gr32	-640000	er1
	test_h_gr32	-2	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

muls_l_imm4_reg:
	set_grs_a5a5

	;; muls.l xx:4, rd
	mov.l	#-320000, er1
	set_ccr_zero
	muls.l	#2:4, er1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_h_gr32	-640000	er1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

muls_u_l_reg_reg:
	set_grs_a5a5

	;; muls/u.l ers, erd
	mov.l	#0x10000000, er1
	mov.l	#-16, er2
	set_ccr_zero
	muls/u.l	er2, er1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	test_gr_a5a5	0
	test_h_gr32	-1	er1
	test_h_gr32	-16	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

muls_u_l_imm4_reg:
	set_grs_a5a5

	;; muls/u.l xx:4, rd
	mov.l	#0xffffffff, er1
	set_ccr_zero
	muls/u.l	#2:4, er1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_h_gr32	-1	er1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

mulu_w_reg_reg:	
	set_grs_a5a5

	;; mulu.w rs, rd
	mov.w	#32, r1
	mov.w	#-2, r2
	set_ccr_zero
	mulu.w	r2, r1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear
	
	test_gr_a5a5	0
	test_h_gr16	-64	r1
	test_h_gr32	0xa5a5fffe	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

mulu_w_imm4_reg:
	set_grs_a5a5

	;; mulu.w xx:4, rd
	mov.w	#32, r1
	set_ccr_zero
	mulu.w	#-2:4, r1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5	0
	test_h_gr16	0x1c0	r1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

mulu_l_reg_reg:	
	set_grs_a5a5

	;; mulu.l ers, erd
	mov.l	#320000, er1
	mov.l	#-2, er2
	set_ccr_zero
	mulu.l	er2, er1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear
	
	test_gr_a5a5	0
	test_h_gr32	-640000	er1
	test_h_gr32	-2	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

mulu_l_imm4_reg:
	set_grs_a5a5

	;; mulu.l xx:4, rd
	mov.l	#320000, er1
	set_ccr_zero
	mulu.l	#-2:4, er1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5	0
	test_h_gr32	0x445c00	er1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

mulu_u_l_reg_reg:
	set_grs_a5a5

	;; mulu/u.l ers, erd
	mov.l	#0x10000000, er1
	mov.l	#16, er2
	set_ccr_zero
	mulu/u.l	er2, er1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear
	
	test_gr_a5a5	0
	test_h_gr32	1	er1
	test_h_gr32	16	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

mulu_u_l_imm4_reg:
	set_grs_a5a5

	;; mulu/u.l xx:4, rd
	mov.l	#0xffffffff, er1
	set_ccr_zero
	mulu/u.l	#2:4, er1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5	0
	test_h_gr32	0x1	er1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
.endif

.if (sim_cpu)			; not equal to zero ie. not h8
mulxs_b_reg_reg:
	set_grs_a5a5

	;; mulxs.b rs, rd
	mov.b	#32, r1l
	mov.b	#-2, r2l
	set_ccr_zero
	mulxs.b	r2l, r1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	test_gr_a5a5	0
	test_h_gr16	-64	r1
	test_h_gr32	0xa5a5a5fe	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

.if (sim_cpu == h8sx)	
mulxs_b_imm4_reg:
	set_grs_a5a5

	;; mulxs.b xx:4, rd
	mov.w	#-32, r1
	set_ccr_zero
	mulxs.b	#2:4, r1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_h_gr16	-64	r1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
.endif				; h8sx

mulxs_w_reg_reg:	
	set_grs_a5a5

	;; mulxs.w ers, erd
	mov.w	#0x1000,  r1
	mov.w	#-0x1000, r2
	set_ccr_zero
	mulxs.w	r2, er1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	test_gr_a5a5	0
	test_h_gr32	0xff000000	er1
	test_h_gr32	0xa5a5f000	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

.if (sim_cpu == h8sx)
mulxs_w_imm4_reg:
	set_grs_a5a5

	;; mulxs.w xx:4, rd
	mov.w	#-1, r1
	set_ccr_zero
	mulxs.w	#2:4, er1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_h_gr32	-2	er1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
.endif				; h8sx
.endif				; not h8

mulxu_b_reg_reg:
	set_grs_a5a5

	;; mulxu.b rs, rd
	mov.b	#32, r1l
	mov.b	#-2, r2l
	set_ccr_zero
	mulxu.b	r2l, r1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear
	
	test_gr_a5a5	0
	test_h_gr16	0x1fc0	r1
	test_h_gr16	0xa5fe  r2
.if (sim_cpu)
	test_h_gr32	0xa5a5a5fe	er2
.endif
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

.if (sim_cpu)			; not h8
.if (sim_cpu == h8sx)
mulxu_b_imm4_reg:
	set_grs_a5a5

	;; mulxu.b xx:4, rd
	mov.b	#-32, r1l
	set_ccr_zero
	mulxu.b	#2:4, r1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5	0
	test_h_gr16	0x1c0	r1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
.endif				; h8sx

mulxu_w_reg_reg:	
	set_grs_a5a5

	;; mulxu.w ers, erd
	mov.w	#0x1000,  r1
	mov.w	#-0x1000, r2
	set_ccr_zero
	mulxu.w	r2, er1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear
	
	test_gr_a5a5	0
	test_h_gr32	0x0f000000	er1
	test_h_gr32	0xa5a5f000	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

.if (sim_cpu == h8sx)
mulxu_w_imm4_reg:
	set_grs_a5a5

	;; mulxu.w xx:4, rd
	mov.w	#-1, r1
	set_ccr_zero
	mulxu.w	#2:4, er1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5	0
	test_h_gr32	0x1fffe	er1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
.endif				; h8sx
.endif				; not h8

	pass

	exit 0
