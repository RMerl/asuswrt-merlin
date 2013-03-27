# Put everything in this csect, which seems to make things work.
# The compiler actually puts the stabs in .csect [PR], but that didn't
# work here (I guess because there is no text section).
1i\
	.csect .data[RW]
# .stabs string,type,0,0,value  ->  .stabx string,value,type,0
s/^[ 	]*\.stabs[ 	]*\("[^"]*"\),[ 	]*\([^,]*\),[ 	]*0,0,[ 	]*\(.*\)$/.stabx \1,\3,\2,0/
s/N_GSYM/128/
# This needs to be C_DECL, which is used for types, not C_LSYM, which is
# ignored on the initial scan.
s/N_LSYM/140/
s/\.begin_common/.bc/
# The AIX assembler doesn't want the name in a .ec directive
s/\.end_common.*/.ec/
s/\.align_it/.align 1/
/\.data/d
/^#/d
