C sparc32/aes-encrypt-internal.asm

ifelse(<
   Copyright (C) 2002, 2005, 2013 Niels Möller

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

include_src(<sparc32/aes.m4>)

C	Arguments
define(<ROUNDS>,<%i0>)
define(<KEYS>,	<%i1>)
define(<T>,	<%i2>)
define(<LENGTH>,<%i3>)
define(<DST>,	<%i4>)
define(<SRC>,	<%i5>)

C	AES state, two copies for unrolling

define(<W0>,	<%l0>)
define(<W1>,	<%l1>)
define(<W2>,	<%l2>)
define(<W3>,	<%l3>)

define(<X0>,	<%l4>)
define(<X1>,	<%l5>)
define(<X2>,	<%l6>)
define(<X3>,	<%l7>)

C	%o0-%03 are used for loop invariants T0-T3
define(<KEY>,	<%o4>)
define(<COUNT>, <%o5>)

C %g1, %g2, %g3 are TMP1, TMP2 and TMP3

C I'm still slightly confused by the frame layout, specified in
C "SYSTEM V APPLICATION BINARY INTERFACE SPARC Processor Supplement".
C However, Sun's cc generates a 104 byte stack frame for a function
C with no local variables, so that should be good enough for us too.

C The sparc32 stack frame looks like
C
C %fp -   4: OS-dependent link field
C %fp -   8: OS-dependent link field
C %fp - 104: OS register save area 
define(<FRAME_SIZE>, 104)

	.file "aes-encrypt-internal.asm"

	C _aes_encrypt(unsigned rounds, const uint32_t *keys,
	C	       const struct aes_table *T,
	C	       size_t length, uint8_t *dst,
	C	       uint8_t *src)

	.section	".text"
	.align 16
	.proc	020
	
PROLOGUE(_nettle_aes_encrypt)

	save	%sp, -FRAME_SIZE, %sp
	cmp	LENGTH, 0
	be	.Lend

	C	Loop invariants
	add	T, AES_TABLE0, T0
	add	T, AES_TABLE1, T1
	add	T, AES_TABLE2, T2
	add	T, AES_TABLE3, T3

	C	Must be even, and includes the final round
	srl	ROUNDS, 1, ROUNDS
	C	Last two rounds handled specially
	sub	ROUNDS, 1, ROUNDS

.Lblock_loop:
	C  Read src, and add initial subkey
	mov	KEYS, KEY
	AES_LOAD(0, SRC, KEY, W0)
	AES_LOAD(1, SRC, KEY, W1)
	AES_LOAD(2, SRC, KEY, W2)
	AES_LOAD(3, SRC, KEY, W3)

	mov	ROUNDS, COUNT
	add	SRC, 16, SRC
	add	KEY, 16, KEY

.Lround_loop:
	C The AES_ROUND macro uses T0,... T3
	C	Transform W -> X
	AES_ROUND(0, W0, W1, W2, W3, KEY, X0)
	AES_ROUND(1, W1, W2, W3, W0, KEY, X1)
	AES_ROUND(2, W2, W3, W0, W1, KEY, X2)
	AES_ROUND(3, W3, W0, W1, W2, KEY, X3)

	C	Transform X -> W
	AES_ROUND(4, X0, X1, X2, X3, KEY, W0)
	AES_ROUND(5, X1, X2, X3, X0, KEY, W1)
	AES_ROUND(6, X2, X3, X0, X1, KEY, W2)
	AES_ROUND(7, X3, X0, X1, X2, KEY, W3)

	subcc	COUNT, 1, COUNT
	bne	.Lround_loop
	add	KEY, 32, KEY

	C	Penultimate round
	AES_ROUND(0, W0, W1, W2, W3, KEY, X0)
	AES_ROUND(1, W1, W2, W3, W0, KEY, X1)
	AES_ROUND(2, W2, W3, W0, W1, KEY, X2)
	AES_ROUND(3, W3, W0, W1, W2, KEY, X3)

	add	KEY, 16, KEY
	C	Final round
	AES_FINAL_ROUND(0, T, X0, X1, X2, X3, KEY, DST)
	AES_FINAL_ROUND(1, T, X1, X2, X3, X0, KEY, DST)
	AES_FINAL_ROUND(2, T, X2, X3, X0, X1, KEY, DST)
	AES_FINAL_ROUND(3, T, X3, X0, X1, X2, KEY, DST)

	subcc	LENGTH, 16, LENGTH
	bne	.Lblock_loop
	add	DST, 16, DST

.Lend:
	ret
	restore
EPILOGUE(_nettle_aes_encrypt)

C Some stats from adriana.lysator.liu.se (SS1000$, 85 MHz), for AES 128

C 1:	nettle-1.13 C-code
C 2:	nettle-1.13 assembler
C 3:	New C-code
C 4:	New assembler, first correct version
C 5:	New assembler, with basic scheduling of AES_ROUND.
C 6:	New assembpler, with loop invariants T0-T3.
C 7:	New assembler, with basic scheduling also of AES_FINAL_ROUND.
	
C	MB/s	cycles/block	Code size (bytes)
C 1	1.2	1107		592
C 2	2.3	572		1032
C 3	2.1	627
C 4	1.8	722
C 5	2.6	496
C 6	3.0	437
C 7	3.1	415		1448
