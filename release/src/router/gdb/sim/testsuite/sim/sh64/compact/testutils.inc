# Support macros for the assembly test cases.

	.macro start
	.text
	.global start
start:
	.endm

	# Perform a single to double precision floating point conversion.
	.macro _s2d fpr dpr
	flds \fpr, fpul
	_setpr
	fcnvsd fpul, \dpr
	_clrpr
	.endm

	# Set the PR (PRecision) bit in the FPSCR.
	.macro _setpr
	sts fpscr, r7
	mov #8, r8
	shll16 r8
	or r8, r7
	lds r7, fpscr
	.endm

	# Clear the PR bit.
	.macro _clrpr
	sts fpscr, r7
	mov #8, r8
	shll16 r8
	not r8, r8
	and r8, r7
	lds r7, fpscr
	.endm

	# nb: this macro clobbers R7. 
	.macro assert reg value
	mov \value, r7
	cmp/eq \reg, r7
	bf wrong
	.endm

	.macro pass
	trapa #253
	.endm

	.macro fail
	trapa #254
	.endm
