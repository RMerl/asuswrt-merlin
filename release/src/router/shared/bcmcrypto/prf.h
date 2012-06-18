/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * prf.h
 * PRF function used in WPA and TGi
 *
 * Copyright 2003, ASUSTeK Inc.
 * All Rights Reserved.
 *     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.; the
 * contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of ASUSTeK Inc..
 *
 */

#ifndef _PRF_H_
#define _PRF_H_

#include <typedefs.h>

/* lengths in Bytes */
#define PRF_MAX_I_D_LEN	1024
#define PRF_MAX_KEY_LEN	64
#define PRF_OUTBUF_LEN	80

int PRF(unsigned char *key, int key_len, unsigned char *prefix, 
	int prefix_len, unsigned char *data, int data_len, 
	unsigned char *output, int len);

int fPRF(unsigned char *key, int key_len, unsigned char *prefix, 
	int prefix_len, unsigned char *data, int data_len, 
	unsigned char *output, int len);

void hmac_sha1(unsigned char *text, int text_len, unsigned char *key,
	       int key_len, unsigned char *digest);

void hmac_md5(unsigned char *text, int text_len, unsigned char *key,
	      int key_len, unsigned char *digest);

#endif /* _PRF_H_ */

