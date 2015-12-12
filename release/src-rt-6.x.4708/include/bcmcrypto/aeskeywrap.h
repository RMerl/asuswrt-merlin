/*
 * aeskeywrap.h
 * Perform RFC3394 AES-based key wrap and unwrap functions.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 * $Id: aeskeywrap.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _AESWRAP_H_
#define _AESWRAP_H_

#include <typedefs.h>

#ifdef BCMDRIVER
#include <osl.h>
#else
#include <stddef.h>  /* For size_t */
#endif

#include <bcmcrypto/aes.h>

#define AKW_BLOCK_LEN	8
/* Max size of wrapped data, not including overhead
 *  The key wrap algorithm doesn't impose any upper bound the wrap length,
 *  but we need something big enought to handle all users.  802.11i and
 *  probably most other users will be limited by MTU size, so using a 2K
 *  buffer limit seems relatively safe.
 */
#define AKW_MAX_WRAP_LEN	384

/* aes_wrap: perform AES-based keywrap function defined in RFC3394
 *	return 0 on success, 1 on error
 *	input is il bytes
 *	output is (il+8) bytes
 */
int BCMROMFN(aes_wrap)(size_t kl, uint8 *key, size_t il, uint8 *input, uint8 *output);

/* aes_unwrap: perform AES-based key unwrap function defined in RFC3394,
 *	return 0 on success, 1 on error
 *	input is il bytes
 *	output is (il-8) bytes
 */
int BCMROMFN(aes_unwrap)(size_t kl, uint8 *key, size_t il, uint8 *input, uint8 *output);

#endif /* _AESWRAP_H_ */
