/* 
   Unix SMB/CIFS implementation.
   Interface header:    HMAC MD5 code
   Copyright (C) Luke Kenneth Casson Leighton 1996-1999
   Copyright (C) Andrew Tridgell 1992-1999
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _HMAC_MD5_H
#define _HMAC_MD5_H

#include "../lib/crypto/md5.h"

typedef struct 
{
        struct MD5Context ctx;
        uint8_t k_ipad[65];    
        uint8_t k_opad[65];

} HMACMD5Context;

void hmac_md5_init_limK_to_64(const uint8_t *key, int key_len,
			      HMACMD5Context *ctx);
void hmac_md5_update(const uint8_t *text, int text_len, HMACMD5Context *ctx);
void hmac_md5_final(uint8_t *digest, HMACMD5Context *ctx);
void hmac_md5(const uint8_t key[16], const uint8_t *data, int data_len, uint8_t *digest);
void hmac_md5_init_rfc2104(const uint8_t *key, int key_len, HMACMD5Context *ctx);

#endif /* _HMAC_MD5_H */
