# Support macros for the Hitachi H8 assembly test cases.

; Set up a minimal machine state
	.macro start
	.equ	h8300,  0
	.equ	h8300h, 1
	.equ	h8300s, 2
	.equ	h8sx,   3
	.if (sim_cpu == h8300s)
	.h8300s
	.else
	.if (sim_cpu == h8300h)
	.h8300h
	.else
	.if (sim_cpu == h8sx)
	.h8300sx
	.endif
	.endif
	.endif

	.text
	.align 2
	.global _start
_start:
	jmp	_main

	.data
	.align 2
	.global pass_str
	.global fail_str
	.global ok_str
	.global pass_loc
	.global fail_loc
	.global ok_loc
pass_str:
	.ascii "pass\n"
fail_str:
	.ascii "fail\n"
ok_str:
	.ascii "ok\n"
pass_loc16:
	.word pass_str
pass_loc32:
	.long pass_str
fail_loc16:
	.word fail_str
fail_loc32:
	.long fail_str
ok_loc16:
	.word ok_str
ok_loc32:
	.long ok_str
	.text

	.global _write_and_exit
_write_and_exit:
;ssize_t write(int fd, const void *buf, size_t count);
;Integer arguments have to be zero extended.
.if (sim_cpu)
#if __INT_MAX__ == 32767
	extu.l  er0
#endif
.endif
	jsr	@@0xc7
	mov	#0, r0
	jmp 	_exit

	.global _exit
_exit:
	mov.b	r0l, r0h
	mov.w	#0xdead, r1
	mov.w	#0xbeef, r2
	sleep

	.global _main
_main:
	.endm


; Exit with an exit code
	.macro exit code
	mov.w	#\code, r0
	jmp	_exit
	.endm

; Output "pass\n"
	.macro pass
	mov.w	#0, r0		; fd == stdout
.if (sim_cpu == h8300)
	mov.w	#pass_str, r1	; buf == "pass\n"
	mov.w	#5, r2		; len == 5
.else
	mov.l	#pass_str, er1	; buf == "pass\n"
	mov.l	#5, er2		; len == 5
.endif
	jmp	_write_and_exit
	.endm

; Output "fail\n"
	.macro fail
	mov.w	#0, r0		; fd == stdout
.if (sim_cpu == h8300)
	mov.w	#fail_str, r1	; buf == "fail\n"
	mov.w	#5, r2		; len == 5
.else
	mov.l	#fail_str, er1	; buf == "fail\n"
	mov.l	#5, er2		; len == 5
.endif
	jmp	_write_and_exit
	.endm


; Load an 8-bit immediate value into a general register
; (reg must be r0l - r7l or r0h - r7h)
	.macro mvi_h_gr8 val reg
	mov.b	#\val, \reg
	.endm

; Load a 16-bit immediate value into a general register
; (reg must be r0 - r7)
	.macro mvi_h_gr16 val reg
	mov.w	#\val, \reg
	.endm

; Load a 32-bit immediate value into a general register
; (reg must be er0 - er7)
	.macro mvi_h_gr32 val reg
	mov.l	#\val, \reg
	.endm

; Test the value of an 8-bit immediate against a general register
; (reg must be r0l - r7l or r0h - r7h)
	.macro test_h_gr8 val reg
	cmp.b	#\val, \reg
	beq	.Ltest_gr8\@
	fail
.Ltest_gr8\@:
	.endm

; Test the value of a 16-bit immediate against a general register
; (reg must be r0 - r7)
	.macro test_h_gr16 val reg h=h l=l
	.if (sim_cpu == h8300)
	test_h_gr8 (\val >> 8) \reg\h
	test_h_gr8 (\val & 0xff) \reg\l
	.else
	cmp.w	#\val, \reg
	beq	.Ltest_gr16\@
	fail
.Ltest_gr16\@:
	.endif
	.endm

; Test the value of a 32-bit immediate against a general register
; (reg must be er0 - er7)
	.macro test_h_gr32 val reg
	cmp.l	#\val, \reg
	beq	.Ltest_gr32\@
	fail
.Ltest_gr32\@:
	.endm

; Set a general register to the fixed pattern 'a5a5a5a5'
	.macro set_gr_a5a5 reg
	.if (sim_cpu == 0)
	; h8300
	mov.w	#0xa5a5, r\reg
	.else
	mov.l	#0xa5a5a5a5, er\reg
	.endif
	.endm

; Set all general registers to the fixed pattern 'a5a5a5a5'
	.macro set_grs_a5a5
	.if (sim_cpu == 0)
	; h8300
	mov.w	#0xa5a5, r0
	mov.w	#0xa5a5, r1
	mov.w	#0xa5a5, r2
	mov.w	#0xa5a5, r3
	mov.w	#0xa5a5, r4
	mov.w	#0xa5a5, r5
	mov.w	#0xa5a5, r6
	mov.w	#0xa5a5, r7
	.else
	mov.l	#0xa5a5a5a5, er0
	mov.l	#0xa5a5a5a5, er1
	mov.l	#0xa5a5a5a5, er2
	mov.l	#0xa5a5a5a5, er3
	mov.l	#0xa5a5a5a5, er4
	mov.l	#0xa5a5a5a5, er5
	mov.l	#0xa5a5a5a5, er6
	mov.l	#0xa5a5a5a5, er7
	.endif
	.endm

; Test that a general register contains the fixed pattern 'a5a5a5a5'
	.macro test_gr_a5a5 reg
	.if (sim_cpu == 0)
	; h8300
	test_h_gr16 0xa5a5 r\reg
	.else
	test_h_gr32 0xa5a5a5a5 er\reg
	.endif
	.endm

; Test that all general regs contain the fixed pattern 'a5a5a5a5'
	.macro test_grs_a5a5
	test_gr_a5a5 0
	test_gr_a5a5 1
	test_gr_a5a5 2
	test_gr_a5a5 3
	test_gr_a5a5 4
	test_gr_a5a5 5
	test_gr_a5a5 6
	test_gr_a5a5 7
	.endm

; Set condition code register to an explicit value
	.macro set_ccr val
	ldc	#\val, ccr
	.endm

; Set all condition code flags to zero
	.macro set_ccr_zero
	ldc	#0, ccr
	.endm

; Set carry flag true
	.macro set_carry_flag
	orc	#1, ccr
	.endm

; Clear carry flag
	.macro clear_carry_flag
	andc	0xfe, ccr
	.endm

; Set zero flag true
	.macro set_zero_flag
	orc	#4, ccr
	.endm

; Clear zero flag
	.macro clear_zero_flag
	andc	0xfb, ccr
	.endm

; Set neg flag true
	.macro set_neg_flag
	orc	#8, ccr
	.endm

; Clear neg flag
	.macro clear_neg_flag
	andc	0xf7, ccr
	.endm

; Test that carry flag is clear
	.macro test_carry_clear
	bcc	.Lcc\@
	fail	; carry flag not clear
.Lcc\@:
	.endm

; Test that carry flag is set
	.macro test_carry_set
	bcs	.Lcs\@
	fail	; carry flag not clear
.Lcs\@:
	.endm

; Test that overflow flag is clear
	.macro test_ovf_clear
	bvc	.Lvc\@
	fail	; overflow flag not clear
.Lvc\@:
	.endm

; Test that overflow flag is set
	.macro test_ovf_set
	bvs	.Lvs\@
	fail	; overflow flag not clear
.Lvs\@:
	.endm

; Test that zero flag is clear
	.macro test_zero_clear
	bne	.Lne\@
	fail	; zero flag not clear
.Lne\@:
	.endm

; Test that zero flag is set
	.macro test_zero_set
	beq	.Leq\@
	fail	; zero flag not clear
.Leq\@:
	.endm

; Test that neg flag is clear
	.macro test_neg_clear
	bpl	.Lneg\@
	fail	; negative flag not clear
.Lneg\@:
	.endm

; Test that neg flag is set
	.macro test_neg_set
	bmi	.Lneg\@
	fail	; negative flag not clear
.Lneg\@:
	.endm

; Test ccr against an explicit value
	.macro test_ccr val
	.data
tccr\@:	.byte	0
	.text
	mov.b	r0l, @tccr\@
	stc	ccr, r0l
	cmp.b	#\val, r0l
	bne .Ltcc\@
	fail
.Ltcc\@:
	mov.b	@tccr\@, r0l
	.endm

; Test that all (accessable) condition codes are clear
	.macro test_cc_clear
	test_carry_clear
	test_ovf_clear
	test_zero_clear
	test_neg_clear
		; leaves H, I, U, and UI untested
	.endm

; Compare memory, fail if not equal (h8sx only, len > 0).
	.macro memcmp src dst len
	mov.l	#\src, er5
	mov.l	#\dst, er6
	mov.l	#\len, er4
.Lmemcmp_\@:
	cmp.b	@er5+, @er6+
	beq	.Lmemcmp2_\@
	fail
.Lmemcmp2_\@:
	dec.l	#1, er4
	bne	.Lmemcmp_\@
	.endm

