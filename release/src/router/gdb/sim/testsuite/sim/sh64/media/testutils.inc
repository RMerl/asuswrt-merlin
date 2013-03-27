# Support macros for the assembly test cases.

	.macro start
	.text
	.global start
start:
	.endm

	.macro pass
	movi 253, r0
	trapa r0
	.endm

	.macro fail
	movi 254, r0
	trapa r0
	.endm

	.macro _packb v1 v2 v3 v4 v5 v6 v7 v8 reg
	movi \v1, \reg
	shlli \reg, 8, \reg
	addi \reg, \v2, \reg
	shlli \reg, 8, \reg
	addi \reg, \v3, \reg
	shlli \reg, 8, \reg
	addi \reg, \v4, \reg
	shlli \reg, 8, \reg
	addi \reg, \v5, \reg
	shlli \reg, 8, \reg
	addi \reg, \v6, \reg
	shlli \reg, 8, \reg
	addi \reg, \v7, \reg
	shlli \reg, 8, \reg
	addi \reg, \v8, \reg
	.endm

	.macro _packw v1 v2 v3 v4 reg
	movi \v1, \reg
	shlli \reg, 16, \reg
	addi \reg, \v2, \reg
	shlli \reg, 16, \reg
	addi \reg, \v3, \reg
	shlli \reg, 16, \reg
	addi \reg, \v4, \reg
	.endm

	.macro _packl v1 v2 reg
	movi \v1, \reg
	shlli \reg, 32, \reg
	addi \reg, \v2, \reg 
	.endm
