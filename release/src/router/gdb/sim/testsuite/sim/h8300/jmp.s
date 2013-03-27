# Hitachi H8 testcase 'jmp'
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
vector_area:
	.fill	0x400, 1, 0

	start
	
.if (sim_cpu == h8sx)
jmp_8:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	mov.l	#.Ltgt_8:32, @0x20
	set_ccr_zero
	;;  jmp @@aa:8		; 8-bit displacement
	jmp @@0x20
	fail

.Ltgt_8:	
	test_cc_clear
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

jmp_7:				; vector jump
	mov.l	#vector_area, er0
	ldc.l	er0, vbr
	set_grs_a5a5
	mov.l	#.Ltgt_7:32, @vector_area+0x300
	set_ccr_zero

	jmp	@@0x300
	fail
.Ltgt_7:
	test_cc_clear
	test_grs_a5a5
	stc.l	vbr, er0
	test_h_gr32 vector_area, er0
	
.endif				; h8sx

jmp_24:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  jmp @aa:24		; 24-bit address
	jmp @.Ltgt_24:24
	fail

.Ltgt_24:	
	test_cc_clear
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7

.if (sim_cpu)			; Non-zero means h8300h, h8300s, or h8sx
jmp_reg:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  jmp @ern		; register indirect
	mov.l	#.Ltgt_reg, er5
	jmp	@er5
	fail
	
.Ltgt_reg:
	test_cc_clear
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_h_gr32 .Ltgt_reg er5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif				; not h8300

.if (sim_cpu == h8sx)
jmp_32:
	set_grs_a5a5		; Fill all general regs with a fixed pattern
	set_ccr_zero

	;;  jmp @aa:32		; 32-bit address
;	jmp @.Ltgt_32:32	; NOTE:	hard-coded to avoid relaxing
	.word	0x5908
	.long	.Ltgt_32
	fail

.Ltgt_32:	
	test_cc_clear
	test_gr_a5a5 0		; Make sure other general regs not disturbed
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
.endif				; h8sx

	pass
	exit 0

	