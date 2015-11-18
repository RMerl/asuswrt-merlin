C Loads one word, and adds it to the subkey. Uses T0
C AES_LOAD(SRC, KEY, REG)
define(<AES_LOAD>, <
	ldrb	$3, [$1], #+1
	ldrb	T0, [$1], #+1
	orr	$3, T0, lsl #8
	ldrb	T0, [$1], #+1
	orr	$3, T0, lsl #16
	ldrb	T0, [$1], #+1
	orr	$3, T0, lsl #24
	ldr	T0, [$2], #+4
	eor	$3, T0
>)
C Stores one word. Destroys input.
C AES_STORE(DST, X)
define(<AES_STORE>, <
	strb	$2, [$1], #+1
	ror	$2, $2, #8
	strb	$2, [$1], #+1
	ror	$2, $2, #8
	strb	$2, [$1], #+1
	ror	$2, $2, #8
	strb	$2, [$1], #+1
>)

C AES_FINAL_ROUND_V6(a,b,c,d,key,res)
define(<AES_FINAL_ROUND_V6>, <
	uxtb	T0, $1
	ldrb	$6, [TABLE, T0]
	uxtb	T0, $2, ror #8
	ldrb	T0, [TABLE, T0]
	eor	$6, $6, T0, lsl #8
	uxtb	T0, $3, ror #16
	ldrb	T0, [TABLE, T0]
	eor	$6, $6, T0, lsl #16
	ldrb	T0, [TABLE, $4, lsr #24]
	eor	$6, $6, T0, lsl #24
	ldr	T0, [$5], #+4
	eor	$6, $6, T0
>)

C AES_FINAL_ROUND_V5(a,b,c,d,key,res,mask)
C Avoids the uxtb instruction, introduced in ARMv6.
C The mask argument should hold the constant 0xff
define(<AES_FINAL_ROUND_V5>, <
	and	T0, $7, $1
	ldrb	$6, [TABLE, T0]
	and	T0, $7, $2, ror #8
	ldrb	T0, [TABLE, T0]
	eor	$6, $6, T0, lsl #8
	and	T0, $7, $3, ror #16
	ldrb	T0, [TABLE, T0]
	eor	$6, $6, T0, lsl #16
	ldrb	T0, [TABLE, $4, lsr #24]
	eor	$6, $6, T0, lsl #24
	ldr	T0, [$5], #+4
	eor	$6, T0
>)
