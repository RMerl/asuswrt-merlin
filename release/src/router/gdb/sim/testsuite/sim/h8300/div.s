# Hitachi H8 testcase 'divs', 'divu', 'divxs', 'divxu'
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
divs_w_reg_reg:	
	set_grs_a5a5

	;; divs.w rs, rd
	mov.w	#32, r1
	mov.w	#-2, r2
	set_ccr_zero
	divs.w	r2, r1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	test_gr_a5a5	0
	test_h_gr16	0xfff0	r1
	test_h_gr32	0xa5a5fffe	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

divs_w_imm4_reg:
	set_grs_a5a5

	;; divs.w xx:4, rd
	mov.w	#-32, r1
	set_ccr_zero
	divs.w	#2:4, r1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_h_gr16	-16	r1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

divs_l_reg_reg:	
	set_grs_a5a5

	;; divs.l ers, erd
	mov.l	#320000, er1
	mov.l	#-2, er2
	set_ccr_zero
	divs.l	er2, er1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	test_gr_a5a5	0
	test_h_gr32	-160000	er1
	test_h_gr32	-2	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

divs_l_imm4_reg:
	set_grs_a5a5

	;; divs.l xx:4, rd
	mov.l	#-320000, er1
	set_ccr_zero
	divs.l	#2:4, er1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_h_gr32	-160000	er1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

divu_w_reg_reg:	
	set_grs_a5a5

	;; divu.w rs, rd
	mov.w	#32, r1
	mov.w	#2, r2
	set_ccr_zero
	divu.w	r2, r1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5	0
	test_h_gr16	16	r1
	test_h_gr32	0xa5a50002	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

divu_w_imm4_reg:
	set_grs_a5a5

	;; divu.w xx:4, rd
	mov.w	#32, r1
	set_ccr_zero
	divu.w	#2:4, r1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5	0
	test_h_gr16	16	r1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

divu_l_reg_reg:	
	set_grs_a5a5

	;; divu.l ers, erd
	mov.l	#320000, er1
	mov.l	#2, er2
	set_ccr_zero
	divu.l	er2, er1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear
	
	test_gr_a5a5	0
	test_h_gr32	160000	er1
	test_h_gr32	2	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

divu_l_imm4_reg:
	set_grs_a5a5

	;; divu.l xx:4, rd
	mov.l	#320000, er1
	set_ccr_zero
	divu.l	#2:4, er1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5	0
	test_h_gr32	160000	er1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

.endif

.if (sim_cpu)			; not equal to zero ie. not h8
divxs_b_reg_reg:
	set_grs_a5a5

	;; divxs.b rs, rd
	mov.w	#32, r1
	mov.b	#-2, r2l
	set_ccr_zero
	divxs.b	r2l, r1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	test_gr_a5a5	0
	test_h_gr16	0x00f0	r1
	test_h_gr32	0xa5a5a5fe	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

.if (sim_cpu == h8sx)	
divxs_b_imm4_reg:
	set_grs_a5a5

	;; divxs.b xx:4, rd
	mov.w	#-32, r1
	set_ccr_zero
	divxs.b	#2:4, r1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_h_gr16	0x00f0	r1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
.endif				; h8sx

divxs_w_reg_reg:	
	set_grs_a5a5

	;; divxs.w ers, erd
	mov.l	#0x1000,  er1
	mov.w	#-0x1000, r2
	set_ccr_zero
	divxs.w	r2, er1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear
	
	test_gr_a5a5	0
	test_h_gr32	0x0000ffff	er1
	test_h_gr32	0xa5a5f000	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

.if (sim_cpu == h8sx)
divxs_w_imm4_reg:
	set_grs_a5a5

	;; divxs.w xx:4, rd
	mov.l	#-4, er1
	set_ccr_zero
	divxs.w	#2:4, er1

	;; test ccr		; H=0 N=1 Z=0 V=0 C=0
	test_neg_set
	test_carry_clear
	test_zero_clear
	test_ovf_clear

	test_gr_a5a5	0
	test_h_gr32	0x0000fffe	er1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
.endif				; h8sx
.endif				; not h8

divxu_b_reg_reg:
	set_grs_a5a5

	;; divxu.b rs, rd
	mov.w	#32, r1
	mov.b	#2, r2l
	set_ccr_zero
	divxu.b	r2l, r1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear
	
	test_gr_a5a5	0
	test_h_gr16	0x0010	r1
	test_h_gr16	0xa502  r2
.if (sim_cpu)
	test_h_gr32	0xa5a5a502	er2
.endif
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

.if (sim_cpu)			; not h8
.if (sim_cpu == h8sx)
divxu_b_imm4_reg:
	set_grs_a5a5

	;; divxu.b xx:4, rd
	mov.w	#32, r1
	set_ccr_zero
	divxu.b	#2:4, r1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5	0
	test_h_gr16	0x0010	r1
	test_gr_a5a5	2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7
.endif				; h8sx

divxu_w_reg_reg:	
	set_grs_a5a5

	;; divxu.w ers, erd
	mov.l	#0x1000, er1
	mov.w	#0x1000, r2
	set_ccr_zero
	divxu.w	r2, er1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear
	
	test_gr_a5a5	0
	test_h_gr32	0x00000001	er1
	test_h_gr32	0xa5a51000	er2
	test_gr_a5a5	3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

.if (sim_cpu == h8sx)
divxu_w_imm4_reg:
	set_grs_a5a5

	;; divxu.w xx:4, rd
	mov.l	#0xffff, er1
	set_ccr_zero
	divxu.w	#2:4, er1

	;; test ccr		; H=0 N=0 Z=0 V=0 C=0
	test_cc_clear

	test_gr_a5a5	0
	test_h_gr32	0x00017fff	er1
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
