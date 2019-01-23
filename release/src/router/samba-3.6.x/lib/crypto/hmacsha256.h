/* 
   Unix SMB/CIFS implementation.

   Interface header:    HMAC SHA256 code

   Copyright (C) Andrew Tridgell 2008

   based on hmacsha1.h which is:

    Copyright (C) Stefan Metzmacher 2006
   
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

#ifndef _HMAC_SHA256_H

struct HMACSHA256Context {
        SHA256_CTX ctx;
        uint8_t k_ipad[65];    
        uint8_t k_opad[65];
};

void hmac_sha256_init(const uint8_t *key, size_t key_len, struct HMACSHA256Context *ctx);
void hmac_sha256_update(const uint8_t *data, size_t data_len, struct HMACSHA256Context *ctx);
void hmac_sha256_final(uint8_t digest[20], struct HMACSHA256Context *ctx);

#endif /* _HMAC_SHA256_H */
