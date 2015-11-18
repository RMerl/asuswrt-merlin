C x86_64/umac-nh-n.asm

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

define(<OUT>, <%rdi>)
define(<ITERS>, <%rsi>)
define(<KEY>, <%rdx>)
define(<LENGTH>, <%rcx>)
define(<MSG>, <%r8>)

define(<XM0>, <%xmm0>)
define(<XM1>, <%xmm1>)
define(<XM2>, <%xmm2>)
define(<XM3>, <%xmm3>)
define(<XK0>, <%xmm4>)
define(<XK1>, <%xmm5>)
define(<XK2>, <%xmm6>)
define(<XK3>, <%xmm7>)
define(<XT0>, <%xmm8>)
define(<XT1>, <%xmm9>)
define(<XT2>, <%xmm10>)
define(<XT3>, <%xmm11>)
define(<XY0>, <%xmm12>)
define(<XY1>, <%xmm13>)

C Copy [0,1,2,3] to [1,1,3,3]
define(<HI2LO>, <pshufd	<$>0xf5,>)

C FIXME: Would be nice if we could force the key array to be 16-byte
C aligned.

	.file "umac-nh-n.asm"
	
	C umac_nh_n(uint64_t *out, unsigned n, const uint32_t *key,
	C	    unsigned length, const uint8_t *msg)
	.text
	ALIGN(16)
PROLOGUE(_nettle_umac_nh_n)
	W64_ENTRY(5, 14)
	pxor	XY0, XY0
	cmp	$3, ITERS
	jc	.Lnh2
	je	.Lnh3

.Lnh4:
	movups	(KEY), XK0
	movups	16(KEY), XT2
	movups	32(KEY), XK2	
	lea	48(KEY), KEY
	C Leave XK2 untouched, and put permuted keys in XK0, XK1, XT2, XT3
	movaps	XK0, XT0
	movaps	XK0, XK1
	
	punpcklqdq XT2, XK0	C [0,1,4,5]
	punpckhqdq XT2, XK1	C [2,3,6,7]
	movaps	XT2, XT3
	punpcklqdq XK2, XT2	C [4,5, 8, 9]
	punpckhqdq XK2, XT3	C [6,7,10,11]

	movaps	XY0, XY1
	
.Loop4:
	movups	(MSG), XT0
	movups	16(MSG), XT1

	pshufd	$0xee, XT1, XM3	C [6,7,6,7]
	pshufd	$0x44, XT1, XM2	C [4,5,4,5]
	pshufd	$0xee, XT0, XM1	C [2,3,2,3]
	pshufd	$0x44, XT0, XM0	C [0,1,0,1]

	paddd	XM0, XK0
	paddd	XM1, XK1
	paddd	XM2, XT2
	paddd 	XM3, XT3

	HI2LO	XK0, XT0
	HI2LO	XT2, XT1
	pmuludq XK0, XT2
	pmuludq XT0, XT1
	paddq	XT2, XY0
	paddq	XT1, XY0

	HI2LO	XK1, XT0
	HI2LO	XT3, XT1
	pmuludq XK1, XT3
	pmuludq XT0, XT1
	paddq	XT3, XY0
	paddq	XT1, XY0

	movaps	XK2, XK0
	movaps	XK2, XK1
	movups	(KEY), XT2
	movups	16(KEY), XK2
	punpcklqdq XT2, XK0	C [ 8, 9,12,13]
	punpckhqdq XT2, XK1	C [10,11,14,15]
	movaps	XT2, XT3

	punpcklqdq XK2, XT2	C [12,13,16,17]
	punpckhqdq XK2, XT3	C [14,15,18,19]

	paddd	XK0, XM0
	paddd	XK1, XM1
	paddd	XT2, XM2
	paddd	XT3, XM3

	HI2LO	XM0, XT0
	HI2LO	XM2, XT1
	pmuludq XM0, XM2
	pmuludq XT0, XT1
	paddq	XM2, XY1
	paddq	XT1, XY1

	HI2LO	XM1, XT0
	HI2LO	XM3, XT1
	pmuludq XM1, XM3
	pmuludq XT0, XT1
	paddq	XM3, XY1
	paddq	XT1, XY1

	subl	$32, XREG(LENGTH)
	lea	32(MSG), MSG
	lea	32(KEY), KEY
	ja	.Loop4

	movups	XY0, (OUT)
	movups	XY1, 16(OUT)

	W64_EXIT(5, 14)
	ret
	
.Lnh3:
	movups	(KEY), XK0
	movups	16(KEY), XK1
	movaps	XY0, XY1
.Loop3:
	lea	32(KEY), KEY
	movups	(MSG), XT0
	movups	16(MSG), XT1
	movups	(KEY), XK2
	movups	16(KEY), XK3
	pshufd	$0xee, XT1, XM3	C [6,7,6,7]
	pshufd	$0x44, XT1, XM2	C [4,5,4,5]
	pshufd	$0xee, XT0, XM1	C [2,3,2,3]
	pshufd	$0x44, XT0, XM0	C [0,1,0,1]

	C Iteration 2
	paddd	XK2, XT0
	paddd	XK3, XT1
	HI2LO	XT0, XT2
	HI2LO	XT1, XT3
	pmuludq	XT0, XT1
	pmuludq	XT2, XT3
	paddq	XT1, XY1
	paddq	XT3, XY1

	C Iteration 0,1
	movaps	XK0, XT0
	punpcklqdq XK1, XK0	C [0,1,4,5]
	punpckhqdq XK1, XT0	C [2,3,6,7]
	paddd	XK0, XM0
	paddd	XT0, XM1
	movaps	XK2, XK0
	movaps	XK1, XT0
	punpcklqdq XK2, XK1	C [4,5,8,9]
	punpckhqdq XK2, XT0	C [6,7,10,11]
	paddd	XK1, XM2
	paddd	XT0, XM3

	HI2LO	XM0, XT0
	HI2LO	XM2, XT1
	pmuludq XM0, XM2
	pmuludq XT0, XT1
	paddq	XM2, XY0
	paddq	XT1, XY0
	
	HI2LO	XM1, XT0
	HI2LO	XM3, XT1
	pmuludq XM1, XM3
	pmuludq XT0, XT1
	paddq	XM3, XY0
	paddq	XT1, XY0
	subl	$32, XREG(LENGTH)
	lea	32(MSG), MSG
	movaps	XK2, XK0
	movaps	XK3, XK1

	ja	.Loop3

	pshufd	$0xe, XY1, XT0
	paddq	XT0, XY1
	movups	XY0, (OUT)
	movlpd	XY1, 16(OUT)

	W64_EXIT(5, 14)
	ret
	
.Lnh2:
	C Explode message as [0,1,0,1] [2,3,2,3] [4,5,4,5] [6,7, 6, 7]
	C Interleave keys as [0,1,4,5] [2,3,6,7] [4,5,8,9] [7,8,10,11]
	movups	(KEY), XK0
	lea	16(KEY), KEY
.Loop2:
	movups	(MSG), XM0
	movups	16(MSG), XM1
	pshufd	$0xee, XM1, XM3	C [6,7,6,7]
	pshufd	$0x44, XM1, XM2	C [4,5,4,5]
	pshufd	$0xee, XM0, XM1	C [2,3,2,3]
	pshufd	$0x44, XM0, XM0	C [0,1,0,1]

	movups	(KEY), XK1
	movups	16(KEY), XK2
	movaps	XK0, XT0
	punpcklqdq XK1, XK0	C [0,1,4,5]
	punpckhqdq XK1, XT0	C [2,3,6,7]
	paddd	XK0, XM0
	paddd	XT0, XM1
	movaps	XK2, XK0
	movaps	XK1, XT0
	punpcklqdq XK2, XK1	C [4,5,8,9]
	punpckhqdq XK2, XT0	C [6,7,10,11]
	paddd	XK1, XM2
	paddd	XT0, XM3

	HI2LO	XM0, XT0
	HI2LO	XM2, XT1
	pmuludq XM0, XM2
	pmuludq XT0, XT1
	paddq	XM2, XY0
	paddq	XT1, XY0
	
	HI2LO	XM1, XT0
	HI2LO	XM3, XT1
	pmuludq XM1, XM3
	pmuludq XT0, XT1
	paddq	XM3, XY0
	paddq	XT1, XY0
	subl	$32, XREG(LENGTH)
	lea	32(MSG), MSG
	lea	32(KEY), KEY

	ja	.Loop2

	movups	XY0, (OUT)
.Lend:
	W64_EXIT(5, 14)
	ret
EPILOGUE(_nettle_umac_nh_n)
