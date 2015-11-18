dnl AES_LOAD(a, b, c, d, src, key)
dnl Loads the next block of data from src, and add the subkey pointed
dnl to by key.
dnl Note that x86 allows unaligned accesses.
dnl Would it be preferable to interleave the loads and stores?
define(<AES_LOAD>, <
	movl	($5),$1
	movl	4($5),$2
	movl	8($5),$3
	movl	12($5),$4
	
	xorl	($6),$1
	xorl	4($6),$2
	xorl	8($6),$3
	xorl	12($6),$4>)dnl

dnl AES_STORE(a, b, c, d, key, dst)
dnl Adds the subkey to a, b, c, d,
dnl and stores the result in the area pointed to by dst.
dnl Note that x86 allows unaligned accesses.
dnl Would it be preferable to interleave the loads and stores?
define(<AES_STORE>, <
	xorl	($5),$1
	xorl	4($5),$2
	xorl	8($5),$3
	xorl	12($5),$4

	movl	$1,($6)
	movl	$2,4($6)
	movl	$3,8($6)
	movl	$4,12($6)>)dnl

dnl AES_ROUND(table,a,b,c,d,out,ptr)
dnl Computes one word of the AES round. Leaves result in $6.
define(<AES_ROUND>, <
	movzbl	LREG($2), $7
	movl	AES_TABLE0 ($1, $7, 4),$6
	movzbl	HREG($3), $7
	xorl	AES_TABLE1 ($1, $7, 4),$6
	movl	$4,$7
	shrl	<$>16,$7
	andl	<$>0xff,$7
	xorl	AES_TABLE2 ($1, $7, 4),$6
	movl	$5,$7
	shrl	<$>24,$7
	xorl	AES_TABLE3 ($1, $7, 4),$6>)dnl

dnl AES_FINAL_ROUND(a, b, c, d, table, out, tmp)
dnl Computes one word of the final round.
dnl Note that we have to quote $ in constants.
define(<AES_FINAL_ROUND>, <
	movzbl	LREG($1),$6
	movzbl	($5, $6), $6
	movl	$2,$7
	andl	<$>0x0000ff00,$7
	orl	$7, $6
	movl	$3,$7
	andl	<$>0x00ff0000,$7
	orl	$7, $6
	movl	$4,$7
	andl	<$>0xff000000,$7
	orl	$7, $6
	roll	<$>8, $6>)dnl

dnl AES_SUBST_BYTE(A, B, C, D, table, ptr)
dnl Substitutes the least significant byte of
dnl each of eax, ebx, ecx and edx, and also rotates
dnl the words one byte to the left.
dnl Uses that AES_SBOX == 0
define(<AES_SUBST_BYTE>, <
	movzbl  LREG($1),$6
	movb	($5, $6),LREG($1)
	roll	<$>8,$1

	movzbl  LREG($2),$6
	movb	($5, $6),LREG($2)
	roll	<$>8,$2

	movzbl  LREG($3),$6
	movb	($5, $6),LREG($3)
	roll	<$>8,$3

	movzbl  LREG($4),$6
	movb	($5, $6),LREG($4)
	roll	<$>8,$4>)dnl
