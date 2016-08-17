C x86_64/gcm-hash8.asm

ifelse(<
   Copyright (C) 2013 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
>)

C Register usage:

define(<KEY>, <%rdi>)
define(<XP>, <%rsi>)
define(<LENGTH>, <%rdx>)
define(<SRC>, <%rcx>)
define(<X0>, <%rax>)
define(<X1>, <%rbx>)
define(<CNT>, <%ebp>)
define(<T0>, <%r8>)
define(<T1>, <%r9>)
define(<T2>, <%r10>)
define(<Z0>, <%r11>)
define(<Z1>, <%r12>)
define(<SHIFT_TABLE>, <%r13>)

	.file "gcm-hash8.asm"

	C void gcm_hash (const struct gcm_key *key, union gcm_block *x,
	C                size_t length, const uint8_t *data)

	.text
	ALIGN(16)
PROLOGUE(_nettle_gcm_hash8)
	W64_ENTRY(4, 0)
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	sub	$16, LENGTH
	lea	.Lshift_table(%rip), SHIFT_TABLE
	mov	(XP), X0
	mov	8(XP), X1
	jc	.Lfinal
ALIGN(16)
.Lblock_loop:

	xor (SRC), X0
	xor 8(SRC), X1

.Lblock_mul:
	rol	$8, X1
	movzbl	LREG(X1), XREG(T1)
	shl	$4, T1
	mov	(KEY, T1), Z0
	mov	8(KEY, T1), Z1

	C shift Z1, Z0, transforming
	C +-----------------------+-----------------------+
	C |15 14 13 12 11 10 09 08|07 06 05 04 03 02 01 00|
	C +-----------------------+-----------------------+
	C into
	C +-----------------------+-----------------------+
	C |14 13 12 11 10 09 08 07|06 05 04 03 02 01 00   |
	C +-----------------------+-----------------+-----+
	C                               xor         |T[15]|
	C                                           +-----+

	mov	$7, CNT

ALIGN(16)
.Loop_X1:
	mov	Z1, T1
	shr	$56, T1
	shl	$8, Z1
	mov	Z0, T0
	shl	$8, Z0
	shr	$56, T0
	movzwl	(SHIFT_TABLE, T1, 2), XREG(T1)
	xor	T1, Z0
	rol	$8, X1
	movzbl	LREG(X1), XREG(T2)
	shl	$4, T2
	xor	(KEY, T2), Z0
	add	T0, Z1
	xor	8(KEY, T2), Z1
	decl	CNT
	jne	.Loop_X1

	mov	$7, CNT

ALIGN(16)
.Loop_X0:
	mov	Z1, T1
	shr	$56, T1
	shl	$8, Z1
	mov	Z0, T0
	shl	$8, Z0
	shr	$56, T0
	movzwl	(SHIFT_TABLE, T1, 2), XREG(T1)
	xor	T1, Z0
	rol	$8, X0
	movzbl	LREG(X0), XREG(T2)
	shl	$4, T2
	xor	(KEY, T2), Z0
	add	T0, Z1
	xor	8(KEY, T2), Z1
	decl	CNT
	jne	.Loop_X0

	mov	Z1, T1
	shr	$56, T1
	shl	$8, Z1
	mov	Z0, T0
	shl	$8, Z0
	shr	$56, T0
	movzwl	(SHIFT_TABLE, T1, 2), XREG(T1)
	xor	T1, Z0
	rol	$8, X0
	movzbl	LREG(X0), XREG(T2)
	shl	$4, T2
	mov	(KEY, T2), X0
	xor	Z0, X0
	add	T0, Z1
	mov	8(KEY, T2), X1
	xor	Z1, X1

	add	$16, SRC
	sub	$16, LENGTH
	jnc	.Lblock_loop

.Lfinal:
	add	$16, LENGTH
	jnz	.Lpartial

	mov	X0, (XP)
	mov	X1, 8(XP)

	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	W64_EXIT(4, 0)
	ret

.Lpartial:
	C Read and xor partial block, then jump back into the loop
	C with LENGTH == 0.

	cmp	$8, LENGTH
	jc	.Llt8

	C 	8 <= LENGTH < 16
	xor	(SRC), X0
	add	$8, SRC
	sub	$8, LENGTH
	jz	.Lblock_mul
	call	.Lread_bytes
	xor	T0, X1
	jmp	.Lblock_mul

.Llt8:	C 0 < LENGTH < 8
	call	.Lread_bytes
	xor	T0, X0
	jmp	.Lblock_mul

C Read 0 < LENGTH < 8 bytes at SRC, result in T0
.Lread_bytes:
	xor	T0, T0
	sub	$1, SRC
ALIGN(16)
.Lread_loop:
	shl	$8, T0
	orb	(SRC, LENGTH), LREG(T0)
.Lread_next:
	sub	$1, LENGTH
	jnz	.Lread_loop
	ret
EPILOGUE(_nettle_gcm_hash8)

define(<W>, <0x$2$1>)
	RODATA
	ALIGN(2)
C NOTE: Sun/Oracle assembler doesn't support ".short".
C Using ".value" seems more portable.
.Lshift_table:
.value W(00,00),W(01,c2),W(03,84),W(02,46),W(07,08),W(06,ca),W(04,8c),W(05,4e)
.value W(0e,10),W(0f,d2),W(0d,94),W(0c,56),W(09,18),W(08,da),W(0a,9c),W(0b,5e)
.value W(1c,20),W(1d,e2),W(1f,a4),W(1e,66),W(1b,28),W(1a,ea),W(18,ac),W(19,6e)
.value W(12,30),W(13,f2),W(11,b4),W(10,76),W(15,38),W(14,fa),W(16,bc),W(17,7e)
.value W(38,40),W(39,82),W(3b,c4),W(3a,06),W(3f,48),W(3e,8a),W(3c,cc),W(3d,0e)
.value W(36,50),W(37,92),W(35,d4),W(34,16),W(31,58),W(30,9a),W(32,dc),W(33,1e)
.value W(24,60),W(25,a2),W(27,e4),W(26,26),W(23,68),W(22,aa),W(20,ec),W(21,2e)
.value W(2a,70),W(2b,b2),W(29,f4),W(28,36),W(2d,78),W(2c,ba),W(2e,fc),W(2f,3e)
.value W(70,80),W(71,42),W(73,04),W(72,c6),W(77,88),W(76,4a),W(74,0c),W(75,ce)
.value W(7e,90),W(7f,52),W(7d,14),W(7c,d6),W(79,98),W(78,5a),W(7a,1c),W(7b,de)
.value W(6c,a0),W(6d,62),W(6f,24),W(6e,e6),W(6b,a8),W(6a,6a),W(68,2c),W(69,ee)
.value W(62,b0),W(63,72),W(61,34),W(60,f6),W(65,b8),W(64,7a),W(66,3c),W(67,fe)
.value W(48,c0),W(49,02),W(4b,44),W(4a,86),W(4f,c8),W(4e,0a),W(4c,4c),W(4d,8e)
.value W(46,d0),W(47,12),W(45,54),W(44,96),W(41,d8),W(40,1a),W(42,5c),W(43,9e)
.value W(54,e0),W(55,22),W(57,64),W(56,a6),W(53,e8),W(52,2a),W(50,6c),W(51,ae)
.value W(5a,f0),W(5b,32),W(59,74),W(58,b6),W(5d,f8),W(5c,3a),W(5e,7c),W(5f,be)
.value W(e1,00),W(e0,c2),W(e2,84),W(e3,46),W(e6,08),W(e7,ca),W(e5,8c),W(e4,4e)
.value W(ef,10),W(ee,d2),W(ec,94),W(ed,56),W(e8,18),W(e9,da),W(eb,9c),W(ea,5e)
.value W(fd,20),W(fc,e2),W(fe,a4),W(ff,66),W(fa,28),W(fb,ea),W(f9,ac),W(f8,6e)
.value W(f3,30),W(f2,f2),W(f0,b4),W(f1,76),W(f4,38),W(f5,fa),W(f7,bc),W(f6,7e)
.value W(d9,40),W(d8,82),W(da,c4),W(db,06),W(de,48),W(df,8a),W(dd,cc),W(dc,0e)
.value W(d7,50),W(d6,92),W(d4,d4),W(d5,16),W(d0,58),W(d1,9a),W(d3,dc),W(d2,1e)
.value W(c5,60),W(c4,a2),W(c6,e4),W(c7,26),W(c2,68),W(c3,aa),W(c1,ec),W(c0,2e)
.value W(cb,70),W(ca,b2),W(c8,f4),W(c9,36),W(cc,78),W(cd,ba),W(cf,fc),W(ce,3e)
.value W(91,80),W(90,42),W(92,04),W(93,c6),W(96,88),W(97,4a),W(95,0c),W(94,ce)
.value W(9f,90),W(9e,52),W(9c,14),W(9d,d6),W(98,98),W(99,5a),W(9b,1c),W(9a,de)
.value W(8d,a0),W(8c,62),W(8e,24),W(8f,e6),W(8a,a8),W(8b,6a),W(89,2c),W(88,ee)
.value W(83,b0),W(82,72),W(80,34),W(81,f6),W(84,b8),W(85,7a),W(87,3c),W(86,fe)
.value W(a9,c0),W(a8,02),W(aa,44),W(ab,86),W(ae,c8),W(af,0a),W(ad,4c),W(ac,8e)
.value W(a7,d0),W(a6,12),W(a4,54),W(a5,96),W(a0,d8),W(a1,1a),W(a3,5c),W(a2,9e)
.value W(b5,e0),W(b4,22),W(b6,64),W(b7,a6),W(b2,e8),W(b3,2a),W(b1,6c),W(b0,ae)
.value W(bb,f0),W(ba,32),W(b8,74),W(b9,b6),W(bc,f8),W(bd,3a),W(bf,7c),W(be,be)
