# Hitachi H8 testcase 'mova'
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
foo:	.long	0x01010101
	.long	0x10101010
	.long	0x11111111

	start

movabl16_reg8:
	set_grs_a5a5
	set_ccr_zero

	mova/b.l	@(1:16, r2l.b), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0xa6 er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movabl16_reg16:
	set_grs_a5a5
	set_ccr_zero

	mova/b.l	@(1:16, r2.w), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0xa5a6 er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movabl32_reg8:
	set_grs_a5a5
	set_ccr_zero

	mova/b.l	@(1:32, r2l.b), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0xa6 er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movabl32_reg16:
	set_grs_a5a5
	set_ccr_zero

	mova/b.l	@(1:32, r2.w), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0xa5a6 er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movawl16_reg8:
	set_grs_a5a5
	set_ccr_zero

	mova/w.l	@(1:16, r2l.b), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0x14b er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movawl16_reg16:
	set_grs_a5a5
	set_ccr_zero

	mova/w.l	@(1:16, r2.w), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0x14b4b er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movawl32_reg8:
	set_grs_a5a5
	set_ccr_zero

	mova/w.l	@(1:32, r2l.b), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0x14b er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movawl32_reg16:
	set_grs_a5a5
	set_ccr_zero

	mova/w.l	@(1:32, r2.w), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0x14b4b er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movall16_reg8:
	set_grs_a5a5
	set_ccr_zero

	mova/l.l	@(1:16, r2l.b), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0x295 er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movall16_reg16:
	set_grs_a5a5
	set_ccr_zero

	mova/l.l	@(1:16, r2.w), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0x29695 er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movall32_reg8:
	set_grs_a5a5
	set_ccr_zero

	mova/l.l	@(1:32, r2l.b), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0x295 er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

movall32_reg16:
	set_grs_a5a5
	set_ccr_zero

	mova/l.l	@(1:32, r2.w), er3

	test_cc_clear
	test_gr_a5a5	0	; Make sure other regs not affected
	test_gr_a5a5	1
	test_gr_a5a5	2
	test_h_gr32	0x29695 er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

t02_mova:
	set_grs_a5a5
	set_ccr_zero

	mov.l	#0x01010101, er1
	mova/b.c @(0x1234:16,r1l.b),er1 ; 7A891234
	test_h_gr32	0x1235, er1 ; 1s
	mov.l	#0x01010101, er1
	mova/b.c @(0x1234:16,r1.w),er1 ; 7A991234
	test_h_gr32	0x1335, er1 ; 2s
	mov.l	#0x01010101, er1
	mova/w.c @(0x1234:16,r1l.b),er1 ; 7AA91234
	test_h_gr32	0x1236, er1 ; 3s
	mov.l	#0x01010101, er1
	mova/w.c @(0x1234:16,r1.w),er1 ; 7AB91234
	test_h_gr32	0x1436, er1 ; 4s
	mov.l	#0x01010101, er1
	mova/l.c @(0x1234:16,r1l.b),er1 ; 7AC91234
	test_h_gr32	0x1238, er1 ; 5s
	mov.l	#0x01010101, er1
	mova/l.c @(0x1234:16,r1.w),er1 ; 7AD91234
	test_h_gr32	0x1638, er1 ; 6s
	mov.l	#0x01010101, er1
	mova/b.c @(0x12345678:32,r1l.b),er1 ; 7A8112345678
	test_h_gr32	0x12345679, er1	; 7s
	mov.l	#0x01010101, er1
	mova/b.c @(0x12345678:32,r1.w),er1 ; 7A9112345678
	test_h_gr32	0x12345779, er1	; 8s
	mov.l	#0x01010101, er1
	mova/w.c @(0x12345678:32,r1l.b),er1 ; 7AA112345678
	test_h_gr32	0x1234567a, er1	; 9s
	mov.l	#0x01010101, er1
	mova/w.c @(0x12345678:32,r1.w),er1 ; 7AB112345678
	test_h_gr32	0x1234587a, er1	; 10s
	mov.l	#0x01010101, er1
	mova/l.c @(0x12345678:32,r1l.b),er1 ; 7AC112345678
	test_h_gr32	0x1234567c, er1	; 11s
	mov.l	#0x01010101, er1
	mova/l.c @(0x12345678:32,r1.w),er1 ; 7AD112345678
	test_h_gr32	0x12345a7c, er1	; 12s

t02b:	
	mov.l	#0x01010101, er3
	mova/b.l @(0x1234:16,r3l.b),er1 ; 78B87A891234
	test_h_gr32	0x1235, er1 ; 1
	mova/b.l @(0x1234:16,r3.w),er1 ; 78397A991234
	test_h_gr32	0x1335, er1 ; 2
	mova/w.l @(0x1234:16,r3l.b),er1 ; 78B87AA91234
	test_h_gr32	0x1236, er1 ; 3
	mova/w.l @(0x1234:16,r3.w),er1 ; 78397AB91234
	test_h_gr32	0x1436, er1 ; 4
	mova/l.l @(0x1234:16,r3l.b),er1 ; 78B87AC91234
	test_h_gr32	0x1238, er1 ; 5
	mova/l.l @(0x1234:16,r3.w),er1 ; 78397AD91234
	test_h_gr32	0x1638, er1 ; 6
	mova/b.l @(0x12345678:32,r3l.b),er1 ; 78B87A8112345678
	test_h_gr32	0x12345679, er1	; 7
	mova/b.l @(0x12345678:32,r3.w),er1 ; 78397A9112345678
	test_h_gr32	0x12345779, er1	; 8
	mova/w.l @(0x12345678:32,r3l.b),er1 ; 78B87AA112345678
	test_h_gr32	0x1234567a, er1	; 9
	mova/w.l @(0x12345678:32,r3.w),er1 ; 78397AB112345678
	test_h_gr32	0x1234587a, er1	; 10
	mova/l.l @(0x12345678:32,r3l.b),er1 ; 78B87AC112345678
	test_h_gr32	0x1234567c, er1	; 11
	mova/l.l @(0x12345678:32,r3.w),er1 ; 78397AD112345678
	test_h_gr32	0x12345a7c, er1	; 12
	test_h_gr32	0x01010101, er3
t02c:	
	mov.l	#foo, er2
	mova/b.l @(0x1234:16,@er2.b),er1              ;017F02811234
	test_h_gr32	0x1235, er1 ; 13
	test_h_gr32	foo, er2
	mova/b.l @(0x1234:16,@(0x1:2,er2).b),er1       ;017F12811234
	test_h_gr32	0x1235, er1 ; 18
	test_h_gr32	foo, er2
	mova/b.l @(0x1234:16,@er2+.b),er1             ;017F82811234
	test_h_gr32	0x1235, er1 ; 14
	test_h_gr32	foo+1, er2 
	mova/b.l @(0x1234:16,@-er2.b),er1             ;017FB2811234
	test_h_gr32	0x1235, er1 ; 17
	test_h_gr32	foo, er2
	mova/b.l @(0x1234:16,@+er2.b),er1             ;017F92811234
	test_h_gr32	0x1235, er1 ; 16
	test_h_gr32	foo+1, er2
	mova/b.l @(0x1234:16,@er2-.b),er1             ;017FA2811234
	test_h_gr32	0x1235, er1 ; 15
	test_h_gr32	foo, er2
t02d:
	mov.l	#4, er2
	mova/b.l @(0x1234:16, @(foo:16, er2).b), er1
	test_h_gr32	0x1244, er1 ; 19
	mova/b.l @(0x1234:16, @(foo:16, r2L.b).b), er1
	test_h_gr32	0x1244, er1 ; 21
	mova/b.l @(0x1234:16, @(foo:16, r2.w).b), er1
	test_h_gr32	0x1244, er1 ; 22
	mova/b.l @(0x1234:16, @(foo:16, er2.l).b), er1
	test_h_gr32	0x1244, er1 ; 23

	mov.l	#4, er2
	mova/b.l @(0x1234:16, @(foo:32, er2).b), er1
	test_h_gr32	0x1244, er1 ; 20
	mova/b.l @(0x1234:16, @(foo:32, r2L.b).b), er1
	test_h_gr32	0x1244, er1 ; 24
	mova/b.l @(0x1234:16, @(foo:32, r2.w).b), er1
	test_h_gr32	0x1244, er1 ; 25
	mova/b.l @(0x1234:16, @(foo:32, er2.l).b), er1
	test_h_gr32	0x1244, er1 ; 26

	mova/b.l @(0x1234:16,@foo:16.b),er1
	test_h_gr32	0x1235, er1 ; 27
	mova/b.l @(0x1234:16,@foo:32.b),er1
	test_h_gr32	0x1235, er1 ; 28

t02e:
	mov.l	#foo, er2
	mova/b.l @(0x1234:16,@er2.w),er1              ;015F02911234
	test_h_gr32	0x1335, er1 ; 29
	test_h_gr32	foo, er2
	mova/b.l @(0x1234:16,@(0x2:2,er2).w),er1    ;015F12911234
	test_h_gr32	0x1335, er1 ; 34
	test_h_gr32	foo, er2
	mova/b.l @(0x1234:16,@er2+.w),er1             ;015F82911234
	test_h_gr32	0x1335, er1 ; 30
	test_h_gr32	foo+2, er2
	mova/b.l @(0x1234:16,@-er2.w),er1             ;015FB2911234
	test_h_gr32	0x1335, er1 ; 33
	test_h_gr32	foo, er2
	mova/b.l @(0x1234:16,@+er2.w),er1             ;015F92911234
	test_h_gr32	0x1335, er1 ; 32
	test_h_gr32	foo+2, er2
	mova/b.l @(0x1234:16,@er2-.w),er1             ;015FA2911234
	test_h_gr32	0x1335, er1 ; 31
	test_h_gr32	foo, er2

	mov.l	#4, er2
	mova/b.l @(0x1234:16, @(foo:16, er2).w), er1
	test_h_gr32	0x2244, er1 ; 35
	shar.l	er2
	mova/b.l @(0x1234:16, @(foo:16, r2L.b).w), er1
	test_h_gr32	0x2244, er1 ; 37
	mova/b.l @(0x1234:16, @(foo:16, r2.w).w), er1
	test_h_gr32	0x2244, er1 ; 38
	mova/b.l @(0x1234:16, @(foo:16, er2.l).w), er1
	test_h_gr32	0x2244, er1 ; 39

	mov.l	#4, er2
	mova/b.l @(0x1234:16, @(foo:32, er2).w), er1
	test_h_gr32	0x2244, er1 ; 36
	shar.l	er2
	mova/b.l @(0x1234:16, @(foo:32, r2L.b).w), er1
	test_h_gr32	0x2244, er1 ; 40
	mova/b.l @(0x1234:16, @(foo:32, r2.w).w), er1
	test_h_gr32	0x2244, er1 ; 41
	mova/b.l @(0x1234:16, @(foo:32, er2.l).w), er1
	test_h_gr32	0x2244, er1 ; 42

	mova/b.l @(0x1234:16,@foo:16.w),er1        ;015F40919ABC1234
	test_h_gr32	0x1335, er1 ; 43
	mova/b.l @(0x1234:16,@foo:32.w),er1        ;015F48919ABCDEF01234
	test_h_gr32	0x1335, er1 ; 44

t02f:
	mov.l	#foo, er2
	mova/w.l @(0x1234:16,@er2.b),er1           ;017F02A11234
	test_h_gr32	0x1236, er1 ; 45
	mova/w.l @(0x1234:16,@(0x1:2,er2).b),er1    ;017F12A11234
	test_h_gr32	0x1236, er1 ; 50
	mova/w.l @(0x1234:16,@er2+.b),er1          ;017F82A11234
	test_h_gr32	0x1236, er1 ; 46
	test_h_gr32	foo+1, er2
	mova/w.l @(0x1234:16,@-er2.b),er1          ;017FB2A11234
	test_h_gr32	0x1236, er1 ; 49
	test_h_gr32	foo, er2
	mova/w.l @(0x1234:16,@+er2.b),er1          ;017F92A11234
	test_h_gr32	0x1236, er1 ; 48
	test_h_gr32	foo+1, er2
	mova/w.l @(0x1234:16,@er2-.b),er1          ;017FA2A11234
	test_h_gr32	0x1236, er1 ; 47
	test_h_gr32	foo, er2

t02g:
	mov.l	#4, er2
	mova/w.l @(0x1234:16, @(foo:16, er2).b), er1
	test_h_gr32	0x1254, er1 ; 51
	mova/w.l @(0x1234:16, @(foo:16, r2L.b).b), er1
	test_h_gr32	0x1254, er1 ; 53
	mova/w.l @(0x1234:16, @(foo:16, r2.w).b), er1
	test_h_gr32	0x1254, er1 ; 54
	mova/w.l @(0x1234:16, @(foo:16, er2.l).b), er1
	test_h_gr32	0x1254, er1 ; 55

	mov.l	#4, er2
	mova/w.l @(0x1234:16, @(foo:32, er2).b), er1
	test_h_gr32	0x1254, er1 ; 52
	mova/w.l @(0x1234:16, @(foo:32, r2L.b).b), er1
	test_h_gr32	0x1254, er1 ; 56
	mova/w.l @(0x1234:16, @(foo:32, r2.w).b), er1
	test_h_gr32	0x1254, er1 ; 57
	mova/w.l @(0x1234:16, @(foo:32, er2.l).b), er1
	test_h_gr32	0x1254, er1 ; 58

	mova/w.l @(0x1234:16,@foo:16.b),er1        ;017F40A19ABC1234
	test_h_gr32	0x1236, er1 ; 59 (can't test -- points into the woods)
	mova/w.l @(0x1234:16,@foo:32.b),er1        ;017F48A19ABCDEF01234
	test_h_gr32	0x1236, er1 ; 60 (can't test -- points into the woods)

t02h:
	mov.l	#foo, er2
	mova/w.l @(0x1234:16,@er2.w),er1           ;015F02B11234
	test_h_gr32	0x1436, er1 ; 61
	mova/w.l @(0x1234:16,@(0x2:2,er2).w),er1 ;015F12B11234
	test_h_gr32	0x1436, er1 ; 66, 0x1234 + (@(4+foo).w << 1
	mova/w.l @(0x1234:16,@er2+.w),er1          ;015F82B11234
	test_h_gr32	0x1436, er1 ; 62
	test_h_gr32	foo+2, er2
	mova/w.l @(0x1234:16,@-er2.w),er1          ;015FB2B11234
	test_h_gr32	0x1436, er1 ; 63
	test_h_gr32	foo, er2 
	mova/w.l @(0x1234:16,@+er2.w),er1          ;015F92B11234
	test_h_gr32	0x1436, er1 ; 64
	test_h_gr32	foo+2, er2
	mova/w.l @(0x1234:16,@er2-.w),er1          ;015FA2B11234
	test_h_gr32	0x1436, er1 ; 65
	test_h_gr32	foo, er2 
t02i:
	mov.l	#4, er2
	mova/w.l @(0x1234:16, @(foo:16, er2).w), er1
	test_h_gr32	0x3254, er1 ; 67
	shar.l	er2
	mova/w.l @(0x1234:16, @(foo:16, r2L.b).w), er1
	test_h_gr32	0x3254, er1 ; 69
	mova/w.l @(0x1234:16, @(foo:16, r2.w).w), er1
	test_h_gr32	0x3254, er1 ; 70
	mova/w.l @(0x1234:16, @(foo:16, er2.l).w), er1
	test_h_gr32	0x3254, er1 ; 71

	mov.l	#4, er2
	mova/w.l @(0x1234:16, @(foo:32, er2).w), er1
	test_h_gr32	0x3254, er1 ; 68
	shar.l	er2
	mova/w.l @(0x1234:16, @(foo:32, r2L.b).w), er1
	test_h_gr32	0x3254, er1 ; 72
	mova/w.l @(0x1234:16, @(foo:32, r2.w).w), er1
	test_h_gr32	0x3254, er1 ; 73
	mova/w.l @(0x1234:16, @(foo:32, er2.l).w), er1
	test_h_gr32	0x3254, er1 ; 74

	mova/w.l @(0x1234:16,@foo:16.w),er1        ;015F40B19ABC1234
	test_h_gr32	0x1436, er1 ; 75 (can't test -- points into the woods)
	mova/w.l @(0x1234:16,@foo:32.w),er1        ;015F48B19ABCDEF01234
	test_h_gr32	0x1436, er1 ; 76 (can't test -- points into the woods)

t02j:
	mov.l	#foo, er2
	mova/l.l @(0x1234:16,@er2.b),er1           ;017F02C11234
	test_h_gr32	0x1238, er1 ; 77
	mova/l.l @(0x1234:16,@(0x1:2,er2).b),er1    ;017F12C11234
	test_h_gr32	0x1238, er1 ; 82
	mova/l.l @(0x1234:16,@er2+.b),er1          ;017F82C11234
	test_h_gr32	0x1238, er1 ; 78
	test_h_gr32	foo+1, er2
	mova/l.l @(0x1234:16,@-er2.b),er1          ;017FB2C11234
	test_h_gr32	0x1238, er1 ; 79
	test_h_gr32	foo, er2
	mova/l.l @(0x1234:16,@+er2.b),er1          ;017F92C11234
	test_h_gr32	0x1238, er1 ; 80
	test_h_gr32	foo+1, er2
	mova/l.l @(0x1234:16,@er2-.b),er1          ;017FA2C11234
	test_h_gr32	0x1238, er1 ; 81
	test_h_gr32	foo, er2

t02k:
	mov.l	#4, er2
	mova/l.l @(0x1234:16, @(foo:16, er2).b), er1
	test_h_gr32	0x1274, er1 ; 83
	mova/l.l @(0x1234:16, @(foo:16, r2L.b).b), er1
	test_h_gr32	0x1274, er1 ; 85
	mova/l.l @(0x1234:16, @(foo:16, r2.w).b), er1
	test_h_gr32	0x1274, er1 ; 86
	mova/l.l @(0x1234:16, @(foo:16, er2.l).b), er1
	test_h_gr32	0x1274, er1 ; 87

	mov.l	#4, er2
	mova/l.l @(0x1234:16, @(foo:32, er2).b), er1
	test_h_gr32	0x1274, er1 ; 84
	mova/l.l @(0x1234:16, @(foo:32, r2L.b).b), er1
	test_h_gr32	0x1274, er1 ; 88
	mova/l.l @(0x1234:16, @(foo:32, r2.w).b), er1
	test_h_gr32	0x1274, er1 ; 89
	mova/l.l @(0x1234:16, @(foo:32, er2.l).b), er1
	test_h_gr32	0x1274, er1 ; 90

	mova/l.l @(0x1234:16,@foo:16.b),er1        ;017F40C19ABC1234
	test_h_gr32	0x1238, er1 ; 91 (can't test -- points into the woods)
	mova/l.l @(0x1234:16,@foo:32.b),er1        ;017F48C19ABCDEF01234
	test_h_gr32	0x1238, er1 ; 92 (can't test -- points into the woods)

t02l:
	mov.l	#foo, er2
	mova/l.l @(0x1234:16,@er2.w),er1           ;015F02D11234
	test_h_gr32	0x1638, er1 ; 93
	mova/l.l @(0x1234:16,@(0x2:2,er2).w),er1   ;015F12D11234
	test_h_gr32	0x1638, er1 ; 98 
	mova/l.l @(0x1234:16,@er2+.w),er1          ;015F82D11234
	test_h_gr32	0x1638, er1 ; 94
	test_h_gr32	foo+2, er2
	mova/l.l @(0x1234:16,@-er2.w),er1          ;015FB2D11234
	test_h_gr32	0x1638, er1 ; 97
	test_h_gr32	foo, er2
	mova/l.l @(0x1234:16,@+er2.w),er1          ;015F92D11234
	test_h_gr32	0x1638, er1 ; 96
	test_h_gr32	foo+2, er2
	mova/l.l @(0x1234:16,@er2-.w),er1          ;015FA2D11234
	test_h_gr32	0x1638, er1 ; 95
	test_h_gr32	foo, er2

t02o:
	mov.l	#4, er2
	mova/l.l @(0x1234:16, @(foo:16, er2).w), er1
	test_h_gr32	0x5274, er1 ; 99
	shar.l	er2
	mova/l.l @(0x1234:16, @(foo:16, r2L.b).w), er1
	test_h_gr32	0x5274, er1 ; 101
	mova/l.l @(0x1234:16, @(foo:16, r2.w).w), er1
	test_h_gr32	0x5274, er1 ; 102
	mova/l.l @(0x1234:16, @(foo:16, er2.l).w), er1
	test_h_gr32	0x5274, er1 ; 103

	mov.l	#4, er2
	mova/l.l @(0x1234:16, @(foo:32, er2).w), er1
	test_h_gr32	0x5274, er1 ; 100
	shar.l	er2
	mova/l.l @(0x1234:16, @(foo:32, r2L.b).w), er1
	test_h_gr32	0x5274, er1 ; 104
	mova/l.l @(0x1234:16, @(foo:32, r2.w).w), er1
	test_h_gr32	0x5274, er1 ; 105
	mova/l.l @(0x1234:16, @(foo:32, er2.l).w), er1
	test_h_gr32	0x5274, er1 ; 106

	mova/l.l @(0x1234:16,@foo:16.w),er1        ;015F40D19ABC1234
	test_h_gr32	0x1638, er1 ; 107 (can't test -- points into the woods)
	mova/l.l @(0x1234:16,@foo:32.w),er1        ;015F48D19ABCDEF01234
	test_h_gr32	0x1638, er1 ; 108 (can't test -- points into the woods)

t02p:
	mov.l	#foo, er2
	mova/b.l @(0x12345678:32,@er2.b),er1              ;017F028912345678
	test_h_gr32	0x12345679, er1	; 109
	mova/b.l @(0x12345678:32,@(0x1:2,er2).b),er1      ;017F128912345678
	test_h_gr32	0x12345679, er1	; 114
	mova/b.l @(0x12345678:32,@er2+.b),er1             ;017F828912345678
	test_h_gr32	0x12345679, er1	; 110
	test_h_gr32	foo+1, er2
	mova/b.l @(0x12345678:32,@-er2.b),er1             ;017FB28912345678
	test_h_gr32	0x12345679, er1	; 113
	test_h_gr32	foo, er2
	mova/b.l @(0x12345678:32,@+er2.b),er1             ;017F928912345678
	test_h_gr32	0x12345679, er1	; 112
	test_h_gr32	foo+1, er2
	mova/b.l @(0x12345678:32,@er2-.b),er1             ;017FA28912345678
	test_h_gr32	0x12345679, er1	; 111
	test_h_gr32	foo, er2

t02q:
	mov.l	#4, er2
	mova/b.l @(0x12345678:32, @(foo:16, er2).b), er1
	test_h_gr32	0x12345688, er1 ; 115
	mova/b.l @(0x12345678:32, @(foo:16, r2L.b).b), er1
	test_h_gr32	0x12345688, er1 ; 117
	mova/b.l @(0x12345678:32, @(foo:16, r2.w).b), er1
	test_h_gr32	0x12345688, er1 ; 118
	mova/b.l @(0x12345678:32, @(foo:16, er2.l).b), er1
	test_h_gr32	0x12345688, er1 ; 119

	mov.l	#4, er2
	mova/b.l @(0x12345678:32, @(foo:32, er2).b), er1
	test_h_gr32	0x12345688, er1 ; 116
	mova/b.l @(0x12345678:32, @(foo:32, r2L.b).b), er1
	test_h_gr32	0x12345688, er1 ; 120
	mova/b.l @(0x12345678:32, @(foo:32, r2.w).b), er1
	test_h_gr32	0x12345688, er1 ; 121
	mova/b.l @(0x12345678:32, @(foo:32, er2.l).b), er1
	test_h_gr32	0x12345688, er1 ; 122

	mova/b.l @(0x12345678:32,@foo:16.b),er1
	test_h_gr32	0x12345679, er1 ; 123
	mova/b.l @(0x12345678:32,@foo:32.b),er1
	test_h_gr32	0x12345679, er1 ; 124

t02r:
	mov.l	#foo, er2
	mova/b.l @(0x12345678:32,@er2.w),er1              ;015F029912345678
	test_h_gr32	0x12345779, er1	; 125
	mova/b.l @(0x12345678:32,@(0x2:2,er2).w),er1      ;015F129912345678
	test_h_gr32	0x12345779, er1 ; 130
	mova/b.l @(0x12345678:32,@er2+.w),er1             ;015F829912345678
	test_h_gr32	0x12345779, er1	; 126
	test_h_gr32	foo+2, er2
	mova/b.l @(0x12345678:32,@-er2.w),er1             ;015FB29912345678
	test_h_gr32	0x12345779, er1	; 129
	test_h_gr32	foo, er2
	mova/b.l @(0x12345678:32,@+er2.w),er1             ;015F929912345678
	test_h_gr32	0x12345779, er1 ; 128
	test_h_gr32	foo+2, er2
	mova/b.l @(0x12345678:32,@er2-.w),er1             ;015FA29912345678
	test_h_gr32	0x12345779, er1 ; 127
	test_h_gr32	foo, er2

	mov.l	#4, er2
	mova/b.l @(0x12345678:32, @(foo:16, er2).w), er1
	test_h_gr32	0x12346688, er1 ; 131
	shar.l	er2
	mova/b.l @(0x12345678:32, @(foo:16, r2L.b).w), er1
	test_h_gr32	0x12346688, er1 ; 133
	mova/b.l @(0x12345678:32, @(foo:16, r2.w).w), er1
	test_h_gr32	0x12346688, er1 ; 134
	mova/b.l @(0x12345678:32, @(foo:16, er2.l).w), er1
	test_h_gr32	0x12346688, er1 ; 135

	mov.l	#4, er2
	mova/b.l @(0x12345678:32, @(foo:32, er2).w), er1
	test_h_gr32	0x12346688, er1 ; 132
	shar.l	er2
	mova/b.l @(0x12345678:32, @(foo:32, r2L.b).w), er1
	test_h_gr32	0x12346688, er1 ; 136
	mova/b.l @(0x12345678:32, @(foo:32, r2.w).w), er1
	test_h_gr32	0x12346688, er1 ; 137
	mova/b.l @(0x12345678:32, @(foo:32, er2.l).w), er1
	test_h_gr32	0x12346688, er1 ; 138

	mova/b.l @(0x12345678:32,@foo:16.w),er1
	test_h_gr32	0x12345779, er1 ; 139
	mova/b.l @(0x12345678:32,@foo:32.w),er1
	test_h_gr32	0x12345779, er1 ; 140

t02s:
	mov.l	#foo, er2
	mova/w.l @(0x12345678:32,@er2.b),er1           ;017F02A912345678
	test_h_gr32	0x1234567a, er1	; 141
	mova/w.l @(0x12345678:32,@(0x1:2,er2).b),er1   ;017F12A912345678
	test_h_gr32	0x1234567a, er1	; 146
	mova/w.l @(0x12345678:32,@er2+.b),er1          ;017F82A912345678
	test_h_gr32	0x1234567a, er1	; 142
	test_h_gr32	foo+1, er2
	mova/w.l @(0x12345678:32,@-er2.b),er1          ;017FB2A912345678
	test_h_gr32	0x1234567a, er1	; 145
	test_h_gr32	foo, er2
	mova/w.l @(0x12345678:32,@+er2.b),er1          ;017F92A912345678
	test_h_gr32	0x1234567a, er1	; 144
	test_h_gr32	foo+1, er2
	mova/w.l @(0x12345678:32,@er2-.b),er1          ;017FA2A912345678
	test_h_gr32	0x1234567a, er1	; 143
	test_h_gr32	foo, er2

	mov.l	#4, er2
	mova/w.l @(0x12345678:32, @(foo:16, er2).b), er1
	test_h_gr32	0x12345698, er1 ; 147
	mova/w.l @(0x12345678:32, @(foo:16, r2L.b).b), er1
	test_h_gr32	0x12345698, er1 ; 149
	mova/w.l @(0x12345678:32, @(foo:16, r2.w).b), er1
	test_h_gr32	0x12345698, er1 ; 150
	mova/w.l @(0x12345678:32, @(foo:16, er2.l).b), er1
	test_h_gr32	0x12345698, er1 ; 151

	mov.l	#4, er2
	mova/w.l @(0x12345678:32, @(foo:32, er2).b), er1
	test_h_gr32	0x12345698, er1 ; 148
	mova/w.l @(0x12345678:32, @(foo:32, r2L.b).b), er1
	test_h_gr32	0x12345698, er1 ; 152
	mova/w.l @(0x12345678:32, @(foo:32, r2.w).b), er1
	test_h_gr32	0x12345698, er1 ; 153
	mova/w.l @(0x12345678:32, @(foo:32, er2.l).b), er1
	test_h_gr32	0x12345698, er1 ; 154

	mova/w.l @(0x12345678:32,@foo:16.b),er1
	test_h_gr32	0x1234567a, er1 ; 155
	mova/w.l @(0x12345678:32,@foo:32.b),er1
	test_h_gr32	0x1234567a, er1 ; 156

t02t:
	mov.l	#foo, er2
	mova/w.l @(0x12345678:32,@er2.w),er1           ;015F02B912345678
	test_h_gr32	0x1234587a, er1	; 157
	mova/w.l @(0x12345678:32,@(0x2:2,er2).w),er1   ;015F12B912345678
	test_h_gr32	0x1234587a, er1	; 162
	mova/w.l @(0x12345678:32,@er2+.w),er1          ;015F82B912345678
	test_h_gr32	0x1234587a, er1	; 158
	test_h_gr32	foo+2, er2
	mova/w.l @(0x12345678:32,@-er2.w),er1          ;015FB2B912345678
	test_h_gr32	0x1234587a, er1	; 161
	test_h_gr32	foo, er2
	mova/w.l @(0x12345678:32,@+er2.w),er1          ;015F92B912345678
	test_h_gr32	0x1234587a, er1	; 160
	test_h_gr32	foo+2, er2
	mova/w.l @(0x12345678:32,@er2-.w),er1          ;015FA2B912345678
	test_h_gr32	0x1234587a, er1	; 159
	test_h_gr32	foo, er2

	mov.l	#4, er2
	mova/w.l @(0x12345678:32, @(foo:16, er2).w), er1
	test_h_gr32	0x12347698, er1 ; 163
	shar.l	er2
	mova/w.l @(0x12345678:32, @(foo:16, r2L.b).w), er1
	test_h_gr32	0x12347698, er1 ; 165
	mova/w.l @(0x12345678:32, @(foo:16, r2.w).w), er1
	test_h_gr32	0x12347698, er1 ; 166
	mova/w.l @(0x12345678:32, @(foo:16, er2.l).w), er1
	test_h_gr32	0x12347698, er1 ; 167

	mov.l	#4, er2
	mova/w.l @(0x12345678:32, @(foo:32, er2).w), er1
	test_h_gr32	0x12347698, er1 ; 164
	shar.l	er2
	mova/w.l @(0x12345678:32, @(foo:32, r2L.b).w), er1
	test_h_gr32	0x12347698, er1 ; 168
	mova/w.l @(0x12345678:32, @(foo:32, r2.w).w), er1
	test_h_gr32	0x12347698, er1 ; 169
	mova/w.l @(0x12345678:32, @(foo:32, er2.l).w), er1
	test_h_gr32	0x12347698, er1 ; 170

	mova/w.l @(0x12345678:32,@foo:16.w),er1
	test_h_gr32	0x1234587a, er1 ; 171
	mova/w.l @(0x12345678:32,@foo:32.w),er1
	test_h_gr32	0x1234587a, er1 ; 172

t02u:
	mov.l	#foo, er2
	mova/l.l @(0x12345678:32,@er2.b),er1           ;017F02C912345678
	test_h_gr32	0x1234567c, er1	; 173
	mova/l.l @(0x12345678:32,@(0x1:2,er2).b),er1   ;017F12C912345678
	test_h_gr32	0x1234567c, er1	; 178
	mova/l.l @(0x12345678:32,@er2+.b),er1          ;017F82C912345678
	test_h_gr32	0x1234567c, er1	; 174
	test_h_gr32	foo+1, er2
	mova/l.l @(0x12345678:32,@-er2.b),er1          ;017FB2C912345678
	test_h_gr32	0x1234567c, er1	; 177
	test_h_gr32	foo, er2
	mova/l.l @(0x12345678:32,@+er2.b),er1          ;017F92C912345678
	test_h_gr32	0x1234567c, er1	; 176
	test_h_gr32	foo+1, er2
	mova/l.l @(0x12345678:32,@er2-.b),er1          ;017FA2C912345678
	test_h_gr32	0x1234567c, er1	; 175
	test_h_gr32	foo, er2

	mov.l	#4, er2
	mova/l.l @(0x12345678:32, @(foo:16, er2).b), er1
	test_h_gr32	0x123456b8, er1 ; 179
	mova/l.l @(0x12345678:32, @(foo:16, r2L.b).b), er1
	test_h_gr32	0x123456b8, er1 ; 181
	mova/l.l @(0x12345678:32, @(foo:16, r2.w).b), er1
	test_h_gr32	0x123456b8, er1 ; 182
	mova/l.l @(0x12345678:32, @(foo:16, er2.l).b), er1
	test_h_gr32	0x123456b8, er1 ; 183

	mov.l	#4, er2
	mova/l.l @(0x12345678:32, @(foo:32, er2).b), er1
	test_h_gr32	0x123456b8, er1 ; 180
	mova/l.l @(0x12345678:32, @(foo:32, r2L.b).b), er1
	test_h_gr32	0x123456b8, er1 ; 184
	mova/l.l @(0x12345678:32, @(foo:32, r2.w).b), er1
	test_h_gr32	0x123456b8, er1 ; 185
	mova/l.l @(0x12345678:32, @(foo:32, er2.l).b), er1
	test_h_gr32	0x123456b8, er1 ; 186

	mova/l.l @(0x12345678:32,@foo:16.b),er1
	test_h_gr32	0x1234567c, er1 ; 187
	mova/l.l @(0x12345678:32,@foo:32.b),er1
	test_h_gr32	0x1234567c, er1 ; 188

t02v:
	mov.l	#foo, er2
	mova/l.l @(0x12345678:32,@er2.w),er1           ;015F02D912345678
	test_h_gr32	0x12345a7c, er1	; 189
	mova/l.l @(0x12345678:32,@(0x2:2,er2).w),er1   ;015F12D912345678
	test_h_gr32	0x12345a7c, er1	; 194
	mova/l.l @(0x12345678:32,@er2+.w),er1          ;015F82D912345678
	test_h_gr32	0x12345a7c, er1	; 190
	test_h_gr32	foo+2, er2
	mova/l.l @(0x12345678:32,@-er2.w),er1          ;015FB2D912345678
	test_h_gr32	0x12345a7c, er1	; 193
	test_h_gr32	foo, er2
	mova/l.l @(0x12345678:32,@+er2.w),er1          ;015F92D912345678
	test_h_gr32	0x12345a7c, er1	; 192
	test_h_gr32	foo+2, er2
	mova/l.l @(0x12345678:32,@er2-.w),er1          ;015FA2D912345678
	test_h_gr32	0x12345a7c, er1	; 191
	test_h_gr32	foo, er2

	mov.l	#4, er2
	mova/l.l @(0x12345678:32, @(foo:16, er2).w), er1
	test_h_gr32	0x123496b8, er1 ; 195
	shar.l	er2
	mova/l.l @(0x12345678:32, @(foo:16, r2L.b).w), er1
	test_h_gr32	0x123496b8, er1 ; 197
	mova/l.l @(0x12345678:32, @(foo:16, r2.w).w), er1
	test_h_gr32	0x123496b8, er1 ; 198
	mova/l.l @(0x12345678:32, @(foo:16, er2.l).w), er1
	test_h_gr32	0x123496b8, er1 ; 199

	mov.l	#4, er2
	mova/l.l @(0x12345678:32, @(foo:32, er2).w), er1
	test_h_gr32	0x123496b8, er1 ; 195
	shar.l	er2
	mova/l.l @(0x12345678:32, @(foo:32, r2L.b).w), er1
	test_h_gr32	0x123496b8, er1 ; 197
	mova/l.l @(0x12345678:32, @(foo:32, r2.w).w), er1
	test_h_gr32	0x123496b8, er1 ; 198
	mova/l.l @(0x12345678:32, @(foo:32, er2.l).w), er1
	test_h_gr32	0x123496b8, er1 ; 199

	mova/l.l @(0x12345678:32,@foo:16.w),er1
	test_h_gr32	0x12345a7c, er1 ; 203
	mova/l.l @(0x12345678:32,@foo:32.w),er1
	test_h_gr32	0x12345a7c, er1 ; 204

	test_gr_a5a5	0
	test_h_gr32	2, er2
	test_h_gr32	0x01010101, er3
	test_gr_a5a5	4
	test_gr_a5a5	5
	test_gr_a5a5	6
	test_gr_a5a5	7

	pass

	exit 0
