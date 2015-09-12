/*
 * BCM47XX FLASH driver interface
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
 * $Id: flashutl.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _flashutl_h_
#define _flashutl_h_


#ifndef _LANGUAGE_ASSEMBLY

int	sysFlashInit(char *flash_str);
int sysFlashRead(uint off, uchar *dst, uint bytes);
int sysFlashWrite(uint off, uchar *src, uint bytes);
void nvWrite(unsigned short *data, unsigned int len);
void nvWriteChars(unsigned char *data, unsigned int len);

#endif	/* _LANGUAGE_ASSEMBLY */

#endif /* _flashutl_h_ */
