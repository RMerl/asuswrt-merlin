# Hitachi H8 testcase 'bra/bc'
# mach(): h8sx
# as(h8300):	--defsym sim_cpu=0
# as(h8300h):	--defsym sim_cpu=1
# as(h8300s):	--defsym sim_cpu=2
# as(h8sx):	--defsym sim_cpu=3
# ld(h8300h):	-m h8300helf
# ld(h8300s):	-m h8300self
# ld(h8sx):	-m h8300sxelf

	.include "testutils.inc"

	.data
byte_src:	.byte	0xa5

	start

.if (sim_cpu == h8sx)
brabc_ind_disp8:
	set_grs_a5a5
	mov	#byte_src, er1
	set_ccr_zero
	;; bra/bc xx:3, @erd, disp8
	bra/bc	#1, @er1, .Lpass1:8
;;; 	.word	0x7c10
;;; 	.word	0x4110
	fail
.Lpass1:
	bra/bc	#2, @er1, .Lfail1:8
;;; 	.word	0x7c10
;;; 	.word	0x4202
	bra	.Lpass2
.Lfail1:
	fail
.Lpass2:	
	test_cc_clear
	test_h_gr32 0xa5a5a5a5 er0
	test_h_gr32 byte_src   er1
	test_h_gr32 0xa5a5a5a5 er2
	test_h_gr32 0xa5a5a5a5 er3
	test_h_gr32 0xa5a5a5a5 er4
	test_h_gr32 0xa5a5a5a5 er5
	test_h_gr32 0xa5a5a5a5 er6
	test_h_gr32 0xa5a5a5a5 er7

brabc_abs8_disp16:
	set_grs_a5a5
	mov.b	#0xa5, @0x20:32
	set_ccr_zero
	;; bra/bc xx:3, @aa:8, disp16
	bra/bc	#1, @0x20:8, .Lpass3:16
	fail
.Lpass3:
	bra/bc	#2, @0x20:8, Lfail:16

	test_cc_clear
	test_grs_a5a5

brabc_abs16_disp16:
	set_grs_a5a5
	set_ccr_zero
	;; bra/bc xx:3, @aa:16, disp16
	bra/bc	#1, @byte_src:16, .Lpass5:16
	fail
.Lpass5:
	bra/bc	#2, @byte_src:16, Lfail:16

	test_cc_clear
	test_grs_a5a5

brabs_ind_disp8:
	set_grs_a5a5
	mov	#byte_src, er1
	set_ccr_zero
	;; bra/bs xx:3, @erd, disp8
	bra/bs	#2, @er1, .Lpass7:8
;;; 	.word	0x7c10
;;; 	.word	0x4a10
	fail
.Lpass7:
	bra/bs	#1, @er1, .Lfail3:8
;;; 	.word	0x7c10
;;; 	.word	0x4902
	bra	.Lpass8
.Lfail3:
	fail
.Lpass8:
	test_cc_clear
	test_h_gr32 0xa5a5a5a5 er0
	test_h_gr32 byte_src   er1
	test_h_gr32 0xa5a5a5a5 er2
	test_h_gr32 0xa5a5a5a5 er3
	test_h_gr32 0xa5a5a5a5 er4
	test_h_gr32 0xa5a5a5a5 er5
	test_h_gr32 0xa5a5a5a5 er6
	test_h_gr32 0xa5a5a5a5 er7

brabs_abs32_disp16:
	set_grs_a5a5
	set_ccr_zero
	;; bra/bs xx:3, @aa:32, disp16
	bra/bs	#2, @byte_src:32, .Lpass9:16
	fail
.Lpass9:
	bra/bs	#1, @byte_src:32, Lfail:16

	test_cc_clear
	test_grs_a5a5

.endif

	pass

	exit 0

Lfail:	fail
