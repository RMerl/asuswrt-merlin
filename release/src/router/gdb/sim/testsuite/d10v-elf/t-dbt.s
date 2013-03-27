.include "t-macros.i"

	start

	PSW_BITS = PSW_DM
	
;;; Blat our DMAP registers so that they point at on-chip imem

	ldi r2, MAP_INSN | 0xf
	st r2, @(DMAP_REG,r0)
	ldi r2, MAP_INSN
	st r2, @(IMAP1_REG,r0)

;;; Patch the interrupt vector's dbt entry with a jmp to success

	ldi r4, #trap
	ldi r5, (VEC_DBT & DMAP_MASK) + DMAP_BASE
	ld2w r2, @(0,r4)
	st2w r2, @(0,r5)
	ld2w r2, @(4,r4)
	st2w r2, @(4,r5)

test_dbt:
	dbt -> nop
	exit47

success:
	checkpsw2 1 PSW_BITS
	exit0

	.data
trap:	ldi r1, success@word
	jmp r1
