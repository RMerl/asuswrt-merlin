C Needs T0 and T1
C QROUND(x0, x1, x2, x3)
define(<QROUND>, <
	movaps	$4, T0		C 0
	paddd	$1, T0		C 1
	movaps	T0, T1		C 2
	pslld	<$>7, T0	C 2
	psrld	<$>25, T1	C 3
	pxor	T0, $2		C 3
	pxor	T1, $2		C 4

	movaps	$1, T0		C 0
	paddd	$2, T0		C 5
	movaps	T0, T1		C 6
	pslld	<$>9, T0	C 6
	psrld	<$>23, T1	C 7
	pxor	T0, $3		C 7
	pxor	T1, $3		C 8

	movaps	$2, T0		C 0
	paddd	$3, T0		C 9
	movaps	T0, T1		C 10
	pslld	<$>13, T0	C 10
	psrld	<$>19, T1	C 11
	pxor	T0, $4		C 11
	pxor	T1, $4		C 12

	movaps	$3, T0		C 0
	paddd	$4, T0		C 13
	movaps	T0, T1		C 14
	pslld	<$>18, T0	C 14
	psrld	<$>14, T1	C 15
	pxor	T0, $1		C 15
	pxor	T1, $1		C 16
>)

C SWAP(x0, x1, mask)
C Swaps bits in x0 and x1, with bits selected by the mask
define(<SWAP>, <
	movaps	$1, T0
	pxor	$2, $1
	pand	$3, $1
	pxor	$1, $2
	pxor	T0, $1
>)
