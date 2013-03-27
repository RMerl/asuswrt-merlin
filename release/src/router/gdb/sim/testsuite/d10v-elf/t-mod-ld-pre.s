.include "t-macros.i"

.section        .rodata
 
        .text
        .globl  main
        .type   main,@function
main:
    mvfc        r0, PSW             ||  ldi.s       r14, #0
    ldi.l       r2, 0x100               ; MOD_E
    ldi.l       r3, 0x108               ; MOD_S
 
test_mod_dec_ld:
    mvtc        r2, MOD_E           ||  bseti       r0, #7
    mvtc        r3, MOD_S
    mvtc        r0, PSW                 ; modulo mode enable
    mv          r1,r3                           ; r1=0x108
    ld          r4, @r1-        ||      nop     ; r1=0x106
    ld          r4, @r1-        ||      nop     ; r1=0x104
    ld          r4, @r1-        ||      nop     ; r1=0x102
    ld          r4, @r1-        ||      nop     ; r1=0x100
    ld          r4, @r1-        ||      nop     ; r1=0x108 
    ld          r4, @r1-        ||      nop     ; r1=0x106 
 
    cmpeqi      r1,#0x106
    brf0f       _ERR            ;  branch to error
 
test_mod_inc_ld:
    mvtc        r2, MOD_S
    mvtc        r3, MOD_E
    mv          r1,r2                           ; r1=0x100
    ld          r4, @r1+        ||      nop     ; r1=0x102
    ld          r4, @r1+        ||      nop     ; r1=0x104
    ld          r4, @r1+        ||      nop     ; r1=0x106
    ld          r4, @r1+        ||      nop     ; r1=0x108
    ld          r4, @r1+        ||      nop     ; r1=0x100
    ld          r4, @r1+        ||      nop     ; r1=0x102
 
    cmpeqi      r1,#0x102
    brf0f       _ERR
 
test_mod_dec_ld2w:
    mvtc        r2, MOD_E
    mvtc        r3, MOD_S
    mv          r1,r3                           ; r1=0x108
    ld2W        r4, @r1-        ||      nop     ; r1=0x104
    ld2W        r4, @r1-        ||      nop     ; r1=0x100
    ld2W        r4, @r1-        ||      nop     ; r1=0x108 
    ld2W        r4, @r1-        ||      nop     ; r1=0x104 
 
    cmpeqi      r1,#0x104
    brf0f       _ERR            ; <= branch to error
 
test_mod_inc_ld2w:
    mvtc        r2, MOD_S
    mvtc        r3, MOD_E           ||  BCLRI       r0, #7
    mv          r1,r2                           ; r1=0x100
    ld2W        r4, @r1+        ||      nop     ; r1=0x104
    ld2W        r4, @r1+        ||      nop     ; r1=0x108
    ld2W        r4, @r1+        ||      nop     ; r1=0x100
    ld2W        r4, @r1+        ||      nop     ; r1=0x104
 
    cmpeqi      r1,#0x104
    brf0f       _ERR
 
test_mod_dec_ld_dis:
    mvtc        r0, PSW                 ; modulo mode disable
    mvtc        r2, MOD_E
    mvtc        r3, MOD_S
    mv          r1,r3                           ; r1=0x108
    ld          r4, @r1-        ||      nop     ; r1=0x106
    ld          r4, @r1-        ||      nop     ; r1=0x104
    ld          r4, @r1-        ||      nop     ; r1=0x102
    ld          r4, @r1-        ||      nop     ; r1=0x100
    ld          r4, @r1-        ||      nop     ; r1=0xFE
    ld          r4, @r1-        ||      nop     ; r1=0xFC
 
    cmpeqi      r1,#0xFC
    brf0f       _ERR
 
test_mod_inc_ld_dis:
    mvtc        r2, MOD_S
    mvtc        r3, MOD_E
    mv          r1,r2                           ; r1=0x100
    ld          r4, @r1+        ||      nop     ; r1=0x102
    ld          r4, @r1+        ||      nop     ; r1=0x104
    ld          r4, @r1+        ||      nop     ; r1=0x106
    ld          r4, @r1+        ||      nop     ; r1=0x108
    ld          r4, @r1+        ||      nop     ; r1=0x10A
    ld          r4, @r1+        ||      nop     ; r1=0x10C
 
    cmpeqi      r1,#0x10C
    brf0f       _ERR
 
test_mod_dec_ld2w_dis:
    mvtc        r2, MOD_E
    mvtc        r3, MOD_S
    mv          r1,r3                           ; r1=0x108
    ld2W        r4, @r1-        ||      nop     ; r1=0x104
    ld2W        r4, @r1-        ||      nop     ; r1=0x100
    ld2W        r4, @r1-        ||      nop     ; r1=0xFC
    ld2W        r4, @r1-        ||      nop     ; r1=0xF8
 
    cmpeqi      r1,#0xF8
    brf0f       _ERR

 test_mod_inc_ld2w_dis:
    mvtc        r2, MOD_S
    mvtc        r3, MOD_E
    mv          r1,r2                           ; r1=0x100
    ld2W        r4, @r1+        ||      nop     ; r1=0x104
    ld2W        r4, @r1+        ||      nop     ; r1=0x108
    ld2W        r4, @r1+        ||      nop     ; r1=0x10C
    ld2W        r4, @r1+        ||      nop     ; r1=0x110
 
    cmpeqi      r1,#0x110
    brf0f       _ERR 

_OK:
	exit0
 
_ERR:
	exit47

 

