C Used as temporaries by the AES macros
define(<TMP1>, <%g1>)
define(<TMP2>, <%g2>)
define(<TMP3>, <%g3>)

C Loop invariants used by AES_ROUND
define(<T0>,	<%o0>)
define(<T1>,	<%o1>)
define(<T2>,	<%o2>)
define(<T3>,	<%o3>)

C AES_LOAD(i, src, key, res)
define(<AES_LOAD>, <
	ldub	[$2 + 4*$1], $4
	ldub	[$2 + 4*$1 + 1], TMP1
	ldub	[$2 + 4*$1 + 2], TMP2
	sll	TMP1, 8, TMP1
	
	or	$4, TMP1, $4
	ldub	[$2 + 4*$1+3], TMP1
	sll	TMP2, 16, TMP2
	or	$4, TMP2, $4
	
	sll	TMP1, 24, TMP1
	C	Get subkey
	ld	[$3 + 4*$1], TMP2
	or	$4, TMP1, $4
	xor	$4, TMP2, $4>)dnl

C AES_ROUND(i, a, b, c, d, key, res)
C Computes one word of the AES round
C FIXME: Could use registers pointing directly to the four tables
C FIXME: Needs better instruction scheduling, and perhaps more temporaries
C Alternatively, we can use a single table and some rotations
define(<AES_ROUND>, <
	and	$2, 0xff, TMP1		C  0
	srl	$3, 6, TMP2		C  1
	sll	TMP1, 2, TMP1		C  0
	and	TMP2, 0x3fc, TMP2	C  1
	ld	[T0 + TMP1], $7		C  0	E0
	srl	$4, 14, TMP1		C  2
	ld	[T1 + TMP2], TMP2	C  1
	and	TMP1, 0x3fc, TMP1	C  2
	xor	$7, TMP2, $7		C  1	E1
	srl	$5, 22, TMP2		C  3
	ld	[T2 + TMP1], TMP1	C  2
	and	TMP2, 0x3fc, TMP2	C  3
	xor	$7, TMP1, $7		C  2	E2
	ld	[$6 + 4*$1], TMP1	C  4
	ld	[T3 + TMP2], TMP2	C  3
	xor	$7, TMP1, $7		C  4	E4
	xor	$7, TMP2, $7		C  3	E3
>)dnl

C AES_FINAL_ROUND(i, T, a, b, c, d, key, dst)
C Compute one word in the final round function. Output is converted to
C octets and stored at dst. Relies on AES_SBOX being zero.
define(<AES_FINAL_ROUND>, <
	C	Load subkey
	ld	[$7 + 4*$1], TMP3

	and	$3, 0xff, TMP1		C  0
	srl	$4, 8, TMP2		C  1
	ldub	[T + TMP1], TMP1	C  0
	and	TMP2, 0xff, TMP2	C  1
	xor	TMP3, TMP1, TMP1	C  0
	ldub	[T + TMP2], TMP2	C  1
	stb	TMP1, [$8 + 4*$1]	C  0	E0
	srl	$5, 16, TMP1		C  2
	srl	TMP3, 8, TMP3		C  1
	and	TMP1, 0xff, TMP1	C  2
	xor	TMP3, TMP2, TMP2	C  1
	ldub	[T + TMP1], TMP1	C  2
	stb	TMP2, [$8 + 4*$1 + 1]	C  1	E1
	srl	$6, 24, TMP2		C  3
	srl	TMP3, 8, TMP3		C  2
	ldub	[T + TMP2], TMP2	C  3
	xor	TMP3, TMP1, TMP1	C  2
	srl	TMP3, 8, TMP3		C  3
	stb	TMP1, [$8 + 4*$1 + 2]	C  2	E2
	xor	TMP3, TMP2, TMP2	C  3
	stb	TMP2, [$8 + 4*$1 + 3]	C  3	E3
>)
