
	.globl _main
	.globl call_tests
	.globl movm_tests
	.globl misc_tests
	.globl mov_tests
	.globl ext_tests
	.globl add_tests
	.globl sub_tests
	.globl cmp_tests
	.globl logical_tests
	.globl shift_tests
	.globl muldiv_tests
	.globl movbu_tests
	.globl movhu_tests
	.globl mac_tests
	.globl bit_tests
	.globl dsp_add_tests
	.globl dsp_cmp_tests
	.globl dsp_sub_tests
	.globl dsp_mov_tests
	.globl dsp_logical_tests
	.globl dsp_misc_tests
	.globl autoincrement_tests
	.globl dsp_autoincrement_tests

	.text
	.am33
_main:
call_tests:
	call 256,[a2,a3,exreg0],9
	call 256,[a2,a3,exreg1],9
	call 256,[a2,a3,exother],9
	call 256,[a2,a3,all],9
	call 131071,[a2,a3,exreg0],9
	call 131071,[a2,a3,exreg1],9
	call 131071,[a2,a3,exother],9
	call 131071,[a2,a3,all],9

movm_tests:
	movm (sp),[a2,a3,exreg0]
	movm (sp),[a2,a3,exreg1]
	movm (sp),[a2,a3,exother]
	movm (sp),[a2,a3,all]
	movm [a2,a3,exreg0],(sp)
	movm [a2,a3,exreg1],(sp)
	movm [a2,a3,exother],(sp)
	movm [a2,a3,all],(sp)
	movm (usp),[a2,a3,exreg0]
	movm (usp),[a2,a3,exreg1]
	movm (usp),[a2,a3,exother]
	movm (usp),[a2,a3,all]
	movm [a2,a3,exreg0],(usp)
	movm [a2,a3,exreg1],(usp)
	movm [a2,a3,exother],(usp)
	movm [a2,a3,all],(usp)

misc_tests:
	syscall 0x4
	mcst9 d0
	mcst48 d1
	getchx d0
	getclx d1
	clr r9
	sat16 r9,r8
	mcste r7,r6
	swap r5,r4
	swaph r3,r2
	swhw  r1,r0


mov_tests:
	mov r0,r1
	mov xr0, r1
	mov r1, xr2
	mov (r1),r2
	mov r3,(r4)
	mov (sp),r5
	mov r6,(sp)
	mov 16,r1
	mov 16,xr1
	mov (16,r1),r2
	mov r2,(16,r1)
	mov (16,sp),r2
	mov r2,(16,sp)
	mov 0x1ffeff,r2
	mov 0x1ffeff,xr2
	mov (0x1ffeff,r1),r2
	mov r2,(0x1ffeff,r1)
	mov (0x1ffeff,sp),r2
	mov r2,(0x1ffeff,sp)
	mov (0x1ffeff),r2
	mov r2,(0x1ffeff)
	mov 0x7ffefdfc,r2
	mov 0x7ffefdfc,xr2
	mov (0x7ffefdfc,r1),r2
	mov r2,(0x7ffefdfc,r1)
	mov (0x7ffefdfc,sp),r2
	mov r2,(0x7ffefdfc,sp)
	mov (0x7ffefdfc),r2
	mov r2,(0x7ffefdfc)
	movu 16,r1
	movu 0x1ffeff,r2
	movu 0x7ffefdfc,r2
	mov usp,a0
	mov ssp,a1
	mov msp,a2
	mov pc,a3
	mov a0,usp
	mov a1,ssp
	mov a2,msp
	mov epsw,d0
	mov d1,epsw
	mov a0,r1
	mov d2,r3
	mov r5,a1
	mov r7,d3

ext_tests:
	ext r2
	extb r3,r4
	extbu r4,r5
	exth r6,r7
	exthu r7,r8

add_tests:
	add r10,r11
	add 16,r1
	add 0x1ffeff,r2
	add 0x7ffefdfc,r2
	add r1,r2,r3
	addc r12,r13
	addc 16,r1
	addc 0x1ffeff,r2
	addc 0x7ffefdfc,r2
	inc r13
	inc4 r12


sub_tests:
	sub r14,r15
	sub 16,r1
	sub 0x1ffeff,r2
	sub 0x7ffefdfc,r2
	subc r15,r14
	subc 16,r1
	subc 0x1ffeff,r2
	subc 0x7ffefdfc,r2

cmp_tests:
	cmp r11,r10
	cmp 16,r1
	cmp 0x1ffeff,r2
	cmp 0x7ffefdfc,r2

logical_tests:
	and r0,r1
	or r2,r3
	xor r4,r5
	not r6
	and 16,r1
	or 16,r1
	xor 16,r1
	and 0x1ffeff,r2
	or 0x1ffeff,r2
	xor 0x1ffeff,r2
	and 0x7ffefdfc,r2
	or 0x7ffefdfc,r2
	xor 0x7ffefdfc,r2
	and 131072,epsw
	or 65535,epsw

shift_tests:
	asr r7,r8
	lsr r9,r10
	asl r11,r12
	asl2 r13
	ror r14
	rol r15
	asr 16,r1
	lsr 16,r1
	asl 16,r1
	asr 0x1ffeff,r2
	lsr 0x1ffeff,r2
	asl 0x1ffeff,r2
	asr 0x7ffefdfc,r2
	lsr 0x7ffefdfc,r2
	asl 0x7ffefdfc,r2

muldiv_tests:
	mul r1,r2
	mulu r3,r4
	mul 16,r1
	mulu 16,r1
	mul 0x1ffeff,r2
	mulu 0x1ffeff,r2
	mul 0x7ffefdfc,r2
	mulu 0x7ffefdfc,r2
	div r5,r6
	divu r7,r8
	dmulh r13,r12
	dmulhu r11,r10
	dmulh 0x7ffefdfc,r2
	dmulhu 0x7ffefdfc,r2
	mul r1,r2,r3,r4
	mulu r1,r2,r3,r4

movbu_tests:
	movbu (r5),r6
	movbu r7,(r8)
	movbu (sp),r7
	movbu r8,(sp)
	movbu (16,r1),r2
	movbu r2,(16,r1)
	movbu (16,sp),r2
	movbu r2,(16,sp)
	movbu (0x1ffeff,r1),r2
	movbu r2,(0x1ffeff,r1)
	movbu (0x1ffeff,sp),r2
	movbu r2,(0x1ffeff,sp)
	movbu (0x1ffeff),r2
	movbu r2,(0x1ffeff)
	movbu (0x7ffefdfc,r1),r2
	movbu r2,(0x7ffefdfc,r1)
	movbu (0x7ffefdfc,sp),r2
	movbu r2,(0x7ffefdfc,sp)
	movbu (0x7ffefdfc),r2
	movbu r2,(0x7ffefdfc)

movhu_tests:
	movhu (r9),r10
	movhu r11,(r12)
	movhu (sp),r9
	movhu r10,(sp)
	movhu (16,r1),r2
	movhu r2,(16,r1)
	movhu (16,sp),r2
	movhu r2,(16,sp)
	movhu (0x1ffeff,r1),r2
	movhu r2,(0x1ffeff,r1)
	movhu (0x1ffeff,sp),r2
	movhu r2,(0x1ffeff,sp)
	movhu (0x1ffeff),r2
	movhu r2,(0x1ffeff)
	movhu (0x7ffefdfc,r1),r2
	movhu r2,(0x7ffefdfc,r1)
	movhu (0x7ffefdfc,sp),r2
	movhu r2,(0x7ffefdfc,sp)
	movhu (0x7ffefdfc),r2
	movhu r2,(0x7ffefdfc)


mac_tests:
	mac r1,r2
	macu r3,r4
	macb r5,r6
	macbu r7,r8
	mach r9,r10
	machu r11,r12
	dmach r13,r14
	dmachu r15,r14
	mac 16,r1
	macu 16,r1
	macb 16,r1
	macbu 16,r1
	mach 16,r1
	machu 16,r1
	mac 0x1ffeff,r2
	macu 0x1ffeff,r2
	macb 0x1ffeff,r2
	macbu 0x1ffeff,r2
	mach 0x1ffeff,r2
	machu 0x1ffeff,r2
	mac 0x7ffefdfc,r2
	macu 0x7ffefdfc,r2
	macb 0x7ffefdfc,r2
	macbu 0x7ffefdfc,r2
	mach 0x7ffefdfc,r2
	machu 0x7ffefdfc,r2
	dmach 0x7ffefdfc,r2
	dmachu 0x7ffefdfc,r2

bit_tests:
	bsch r1,r2
	btst 16,r1
	btst 0x1ffeff,r2
	btst 0x7ffefdfc,r2


	
dsp_add_tests:
	add_add r4,r1,r2,r3
	add_add r4,r1,2,r3
	add_sub r4,r1,r2,r3
	add_sub r4,r1,2,r3
	add_cmp r4,r1,r2,r3
	add_cmp r4,r1,2,r3
	add_mov r4,r1,r2,r3
	add_mov r4,r1,2,r3
	add_asr r4,r1,r2,r3
	add_asr r4,r1,2,r3
	add_lsr r4,r1,r2,r3
	add_lsr r4,r1,2,r3
	add_asl r4,r1,r2,r3
	add_asl r4,r1,2,r3
	add_add 4,r1,r2,r3
	add_add 4,r1,2,r3
	add_sub 4,r1,r2,r3
	add_sub 4,r1,2,r3
	add_cmp 4,r1,r2,r3
	add_cmp 4,r1,2,r3
	add_mov 4,r1,r2,r3
	add_mov 4,r1,2,r3
	add_asr 4,r1,r2,r3
	add_asr 4,r1,2,r3
	add_lsr 4,r1,r2,r3
	add_lsr 4,r1,2,r3
	add_asl 4,r1,r2,r3
	add_asl 4,r1,2,r3

dsp_cmp_tests:
	cmp_add r4,r1,r2,r3
	cmp_add r4,r1,2,r3
	cmp_sub r4,r1,r2,r3
	cmp_sub r4,r1,2,r3
	cmp_mov r4,r1,r2,r3
	cmp_mov r4,r1,2,r3
	cmp_asr r4,r1,r2,r3
	cmp_asr r4,r1,2,r3
	cmp_lsr r4,r1,r2,r3
	cmp_lsr r4,r1,2,r3
	cmp_asl r4,r1,r2,r3
	cmp_asl r4,r1,2,r3
	cmp_add 4,r1,r2,r3
	cmp_add 4,r1,2,r3
	cmp_sub 4,r1,r2,r3
	cmp_sub 4,r1,2,r3
	cmp_mov 4,r1,r2,r3
	cmp_mov 4,r1,2,r3
	cmp_asr 4,r1,r2,r3
	cmp_asr 4,r1,2,r3
	cmp_lsr 4,r1,r2,r3
	cmp_lsr 4,r1,2,r3
	cmp_asl 4,r1,r2,r3
	cmp_asl 4,r1,2,r3

dsp_sub_tests:
	sub_add r4,r1,r2,r3
	sub_add r4,r1,2,r3
	sub_sub r4,r1,r2,r3
	sub_sub r4,r1,2,r3
	sub_cmp r4,r1,r2,r3
	sub_cmp r4,r1,2,r3
	sub_mov r4,r1,r2,r3
	sub_mov r4,r1,2,r3
	sub_asr r4,r1,r2,r3
	sub_asr r4,r1,2,r3
	sub_lsr r4,r1,r2,r3
	sub_lsr r4,r1,2,r3
	sub_asl r4,r1,r2,r3
	sub_asl r4,r1,2,r3
	sub_add 4,r1,r2,r3
	sub_add 4,r1,2,r3
	sub_sub 4,r1,r2,r3
	sub_sub 4,r1,2,r3
	sub_cmp 4,r1,r2,r3
	sub_cmp 4,r1,2,r3
	sub_mov 4,r1,r2,r3
	sub_mov 4,r1,2,r3
	sub_asr 4,r1,r2,r3
	sub_asr 4,r1,2,r3
	sub_lsr 4,r1,r2,r3
	sub_lsr 4,r1,2,r3
	sub_asl 4,r1,r2,r3
	sub_asl 4,r1,2,r3

dsp_mov_tests:
	mov_add r4,r1,r2,r3
	mov_add r4,r1,2,r3
	mov_sub r4,r1,r2,r3
	mov_sub r4,r1,2,r3
	mov_cmp r4,r1,r2,r3
	mov_cmp r4,r1,2,r3
	mov_mov r4,r1,r2,r3
	mov_mov r4,r1,2,r3
	mov_asr r4,r1,r2,r3
	mov_asr r4,r1,2,r3
	mov_lsr r4,r1,r2,r3
	mov_lsr r4,r1,2,r3
	mov_asl r4,r1,r2,r3
	mov_asl r4,r1,2,r3
	mov_add 4,r1,r2,r3
	mov_add 4,r1,2,r3
	mov_sub 4,r1,r2,r3
	mov_sub 4,r1,2,r3
	mov_cmp 4,r1,r2,r3
	mov_cmp 4,r1,2,r3
	mov_mov 4,r1,r2,r3
	mov_mov 4,r1,2,r3
	mov_asr 4,r1,r2,r3
	mov_asr 4,r1,2,r3
	mov_lsr 4,r1,r2,r3
	mov_lsr 4,r1,2,r3
	mov_asl 4,r1,r2,r3
	mov_asl 4,r1,2,r3

dsp_logical_tests:
	and_add r4,r1,r2,r3
	and_add r4,r1,2,r3
	and_sub r4,r1,r2,r3
	and_sub r4,r1,2,r3
	and_cmp r4,r1,r2,r3
	and_cmp r4,r1,2,r3
	and_mov r4,r1,r2,r3
	and_mov r4,r1,2,r3
	and_asr r4,r1,r2,r3
	and_asr r4,r1,2,r3
	and_lsr r4,r1,r2,r3
	and_lsr r4,r1,2,r3
	and_asl r4,r1,r2,r3
	and_asl r4,r1,2,r3
	xor_add r4,r1,r2,r3
	xor_add r4,r1,2,r3
	xor_sub r4,r1,r2,r3
	xor_sub r4,r1,2,r3
	xor_cmp r4,r1,r2,r3
	xor_cmp r4,r1,2,r3
	xor_mov r4,r1,r2,r3
	xor_mov r4,r1,2,r3
	xor_asr r4,r1,r2,r3
	xor_asr r4,r1,2,r3
	xor_lsr r4,r1,r2,r3
	xor_lsr r4,r1,2,r3
	xor_asl r4,r1,r2,r3
	xor_asl r4,r1,2,r3
	or_add r4,r1,r2,r3
	or_add r4,r1,2,r3
	or_sub r4,r1,r2,r3
	or_sub r4,r1,2,r3
	or_cmp r4,r1,r2,r3
	or_cmp r4,r1,2,r3
	or_mov r4,r1,r2,r3
	or_mov r4,r1,2,r3
	or_asr r4,r1,r2,r3
	or_asr r4,r1,2,r3
	or_lsr r4,r1,r2,r3
	or_lsr r4,r1,2,r3
	or_asl r4,r1,r2,r3
	or_asl r4,r1,2,r3

dsp_misc_tests:
	dmach_add r4,r1,r2,r3
	dmach_add r4,r1,2,r3
	dmach_sub r4,r1,r2,r3
	dmach_sub r4,r1,2,r3
	dmach_cmp r4,r1,r2,r3
	dmach_cmp r4,r1,2,r3
	dmach_mov r4,r1,r2,r3
	dmach_mov r4,r1,2,r3
	dmach_asr r4,r1,r2,r3
	dmach_asr r4,r1,2,r3
	dmach_lsr r4,r1,r2,r3
	dmach_lsr r4,r1,2,r3
	dmach_asl r4,r1,r2,r3
	dmach_asl r4,r1,2,r3
	swhw_add r4,r1,r2,r3
	swhw_add r4,r1,2,r3
	swhw_sub r4,r1,r2,r3
	swhw_sub r4,r1,2,r3
	swhw_cmp r4,r1,r2,r3
	swhw_cmp r4,r1,2,r3
	swhw_mov r4,r1,r2,r3
	swhw_mov r4,r1,2,r3
	swhw_asr r4,r1,r2,r3
	swhw_asr r4,r1,2,r3
	swhw_lsr r4,r1,r2,r3
	swhw_lsr r4,r1,2,r3
	swhw_asl r4,r1,r2,r3
	swhw_asl r4,r1,2,r3
	sat16_add r4,r1,r2,r3
	sat16_add r4,r1,2,r3
	sat16_sub r4,r1,r2,r3
	sat16_sub r4,r1,2,r3
	sat16_cmp r4,r1,r2,r3
	sat16_cmp r4,r1,2,r3
	sat16_mov r4,r1,r2,r3
	sat16_mov r4,r1,2,r3
	sat16_asr r4,r1,r2,r3
	sat16_asr r4,r1,2,r3
	sat16_lsr r4,r1,r2,r3
	sat16_lsr r4,r1,2,r3
	sat16_asl r4,r1,r2,r3
	sat16_asl r4,r1,2,r3

autoincrement_tests:
	mov (r1+),r2
	mov r3,(r4+)
	movhu (r6+),r7
	movhu r8,(r9+)
	mov (r1+,64),r2
	mov r1,(r2+,64)
	movhu (r1+,64),r2
	movhu r1,(r2+,64)
	mov (r1+,0x1ffef),r2
	mov r1,(r2+,0x1ffef)
	movhu (r1+,0x1ffef),r2
	movhu r1,(r2+,0x1ffef)
	mov (r1+,0x7ffefdfc),r2
	mov r1,(r2+,0x7ffefdfc)
	movhu (r1+,0x7ffefdfc),r2
	movhu r1,(r2+,0x7ffefdfc)

dsp_autoincrement_tests:
	mov_llt (r1+,4),r2
	mov_lgt (r1+,4),r2
	mov_lge (r1+,4),r2
	mov_lle (r1+,4),r2
	mov_lcs (r1+,4),r2
	mov_lhi (r1+,4),r2
	mov_lcc (r1+,4),r2
	mov_lls (r1+,4),r2
	mov_leq (r1+,4),r2
	mov_lne (r1+,4),r2
	mov_lra (r1+,4),r2
