dnl LREG(reg) gives the 8-bit register corresponding to the given 32-bit register.
define(<LREG>,<ifelse(
	$1, %eax, %al,
	$1, %ebx, %bl,
	$1, %ecx, %cl,
	$1, %edx, %dl,
	$1, %esi, %sil,
	$1, %edi, %dil,
	$1, %ebp, %bpl,
	$1, %esp, %spl,
	$1, %r8d, %r8b,
	$1, %r9d, %r9b,
	$1, %r10d, %r10b,
	$1, %r11d, %r11b,
	$1, %r12d, %r12b,
	$1, %r13d, %r13b,
	$1, %r14d, %r14b,
	$1, %r15d, %r15b)>)dnl

define(<HREG>,<ifelse(
	$1, %eax, %ah,
	$1, %ebx, %bh,
	$1, %ecx, %ch,
	$1, %edx, %dh,
	error)>)

define(<XREG>,<ifelse(
	$1, %rax, %eax,
	$1, %rbx, %ebx,
	$1, %rcx, %ecx,
	$1, %rdx, %edx,
	$1, %rsi, %esi,
	$1, %rdi, %edi,
	$1, %rbp, %ebp,
	$1, %rsp, %esp,
	$1, %r8, %r8d,
	$1, %r9, %r9d,
	$1, %r10,%r10d,
	$1, %r11,%r11d,
	$1, %r12,%r12d,
	$1, %r13,%r13d,
	$1, %r14,%r14d,
	$1, %r15,%r15d)>)dnl

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
	movzb	LREG($2), $7
	movl	AES_TABLE0 ($1, $7, 4),$6
	movzb	HREG($3), XREG($7)
	xorl	AES_TABLE1 ($1, $7, 4),$6
	movl	$4,XREG($7)
	shr	<$>16,$7
	and	<$>0xff,$7
	xorl	AES_TABLE2 ($1, $7, 4),$6
	movl	$5,XREG($7)
	shr	<$>24,$7
	xorl	AES_TABLE3 ($1, $7, 4),$6>)dnl

dnl AES_FINAL_ROUND(a, b, c, d, table, out, tmp)
dnl Computes one word of the final round. Leaves result in $6. Also
dnl performs the first substitution step, on the least significant
dnl byte, and rotates 8 bits.
define(<AES_FINAL_ROUND>, <
	movzb	LREG($1),$7
	movzbl	($5, $7), $6
	movl	$2,XREG($7)
	andl	<$>0x0000ff00,XREG($7)
	orl	XREG($7), $6
	movl	$3,XREG($7)
	andl	<$>0x00ff0000,XREG($7)
	orl	XREG($7), $6
	movl	$4,XREG($7)
	andl	<$>0xff000000,XREG($7)
	orl	XREG($7), $6
	roll	<$>8, $6>)dnl

dnl AES_SUBST_BYTE(A, B, C, D, table, tmp)
dnl Substitutes the least significant byte of
dnl each of eax, ebx, ecx and edx, and also rotates
dnl the words one byte to the left.
dnl Uses that AES_SBOX == 0
define(<AES_SUBST_BYTE>, <
	movzb	LREG($1),$6
	movb	($5, $6),LREG($1)
	roll	<$>8,$1

	movzb  LREG($2),$6
	movb	($5, $6),LREG($2)
	roll	<$>8,$2

	movzb  LREG($3),$6
	movb	($5, $6),LREG($3)
	roll	<$>8,$3

	movzb  LREG($4),$6
	movb	($5, $6),LREG($4)
	roll	<$>8,$4>)dnl
