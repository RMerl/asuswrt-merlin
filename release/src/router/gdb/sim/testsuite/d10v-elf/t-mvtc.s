.include "t-macros.i"

	start

;;; Try out each bit in the PSW

	loadpsw2 PSW_SM
	checkpsw2 1 PSW_SM

	loadpsw2 PSW_01
	checkpsw2 2 0 ;; PSW_01

	loadpsw2 PSW_EA
	checkpsw2 3 PSW_EA

	loadpsw2 PSW_DB
	checkpsw2 4 PSW_DB

	loadpsw2 PSW_DM
	checkpsw2 5 0 ;; PSW_DM

	loadpsw2 PSW_IE
	checkpsw2 6 PSW_IE

	loadpsw2 PSW_RP
	checkpsw2 7 PSW_RP

	loadpsw2 PSW_MD
	checkpsw2 8 PSW_MD

	loadpsw2 PSW_FX|PSW_ST
	checkpsw2 9 PSW_FX|PSW_ST

	;; loadpsw2 PSW_ST
	;; checkpsw2 10 

	loadpsw2 PSW_10
	checkpsw2 11 0 ;; PSW_10

	loadpsw2 PSW_11
	checkpsw2 12 0 ;; PSW_11

	loadpsw2 PSW_F0
	checkpsw2 13 PSW_F0

	loadpsw2 PSW_F1
	checkpsw2 14 PSW_F1

	loadpsw2 PSW_14
	checkpsw2 15 0 ;; PSW_14

	loadpsw2 PSW_C
	checkpsw2 16 PSW_C


;;; Check that bit 0 (LSB) of the MOD_E & MOD_S registers are stuck at ZERO.

	ldi	r6, #0xdead
	mvtc	r6, cr10
	ldi	r6, #0xbeef
	mvtc	r6, cr11
	
	mvfc	r7, cr10
	check 17 r7 0xdeac
	mvfc	r7, cr11
	check 18 r7 0xbeee

;;; Check that certain bits of the PSW, DPSW and BPSW are hardwired to zero

psw_ffff:
	ldi	r6, 0xffff
	mvtc	r6, psw
	mvfc	r7, psw
	check 18 r7 0xb7cd

bpsw_ffff:
	ldi	r6, 0xffff
	mvtc	r6, bpsw
	mvfc	r7, bpsw
	check 18 r7 0xb7cd

dpsw_ffff:
	ldi	r6, 0xffff
	mvtc	r6, dpsw
	mvfc	r7, dpsw
	check 18 r7 0xb7cd

;;; Another check. Very similar

psw_dfff:
	ldi	r6, 0xdfff
	mvtc	r6, psw
	mvfc	r7, psw
	check 18 r7 0x97cd

bpsw_dfff:
	ldi	r6, 0xdfff
	mvtc	r6, bpsw
	mvfc	r7, bpsw
	check 18 r7 0x97cd

dpsw_dfff:
	ldi	r6, 0xdfff
	mvtc	r6, dpsw
	mvfc	r7, dpsw
	check 18 r7 0x97cd

;;; And again.

psw_8005:
	ldi	r6, 0x8005
	mvtc	r6, psw
	mvfc	r7, psw
	check 18 r7 0x8005

bpsw_8005:
	ldi	r6, 0x8005
	mvtc	r6, bpsw
	mvfc	r7, bpsw
	check 18 r7 0x8005

dpsw_8005:
	ldi	r6, 0x8005
	mvtc	r6, dpsw
	mvfc	r7, dpsw
	check 18 r7 0x8005


	exit0
