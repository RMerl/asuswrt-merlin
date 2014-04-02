/*
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: md4.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _MD4_H_
#define _MD4_H_

/*
 * md4.h, copied from src/router/ppp/pppd to src/include/bcmcrypto for general use
 *
 * $Id: md4.h 241182 2011-02-17 21:50:03Z $
 *
** ********************************************************************
** md4.h -- Header file for implementation of                        **
** MD4 Message Digest Algorithm                                      **
** Updated: 2/13/90 by Ronald L. Rivest                              **
** (C) 1990 RSA Data Security, Inc.                                  **
** ********************************************************************
*/

#ifndef __P
# if defined(__STDC__) || defined(__GNUC__)
#  define __P(x) x
# else
#  define __P(x) ()
# endif
#endif	/* !__P */


/* MDstruct is the data structure for a message digest computation.
*/
typedef struct {
	unsigned int buffer[4]; /* Holds 4-word result of MD computation */
	unsigned char count[8]; /* Number of bits processed so far */
	unsigned int done;      /* Nonzero means MD computation finished */
} MD4_CTX;

/* MD4Init(MD4_CTX *)
** Initialize the MD4_CTX prepatory to doing a message digest
** computation.
*/
extern void BCMROMFN(MD4Init) __P((MD4_CTX *MD));

/* MD4Update(MD,X,count)
** Input: X -- a pointer to an array of unsigned characters.
**        count -- the number of bits of X to use (an unsigned int).
** Updates MD using the first "count" bits of X.
** The array pointed to by X is not modified.
** If count is not a multiple of 8, MD4Update uses high bits of
** last byte.
** This is the basic input routine for a user.
** The routine terminates the MD computation when count < 512, so
** every MD computation should end with one call to MD4Update with a
** count less than 512.  Zero is OK for a count.
*/
extern void BCMROMFN(MD4Update) __P((MD4_CTX *MD, unsigned char *X, unsigned int count));

/* MD4Print(MD)
** Prints message digest buffer MD as 32 hexadecimal digits.
** Order is from low-order byte of buffer[0] to high-order byte
** of buffer[3].
** Each byte is printed with high-order hexadecimal digit first.
*/
extern void MD4Print __P((MD4_CTX *));

/* MD4Final(buf, MD)
** Returns message digest from MD and terminates the message
** digest computation.
*/
extern void BCMROMFN(MD4Final) __P((unsigned char *, MD4_CTX *));

/*
** End of md4.h
*/

#endif /* _MD4_H_ */
