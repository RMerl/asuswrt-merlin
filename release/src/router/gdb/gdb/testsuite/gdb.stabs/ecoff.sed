# GDB legitimately expects a file name.
1i\
 .file	1 "weird.c"\
\ #@stabs\
\ #.stabs "weird.c",0x64,0,0,0
/^#/d
s/" *, */",/g
s/\([0-9]\) *, */\1,/g
s/  *$//
s/N_LSYM/0x80/
s/N_GSYM/0x20/
s/\.begin_common\(.*\)/.stabs \1,0xe2,0,0,0/
s/\.end_common\(.*\)/.stabs \1,0xe4,0,0,0/
s/\.align_it/.align 2/
/.if/d
/.endif/d
s/\.stabs/ #.stabs/
