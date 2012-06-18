/* 
   Unix SMB/CIFS implementation.
   MD5 tests
   Copyright (C) Stefan Metzmacher
   
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

#include "includes.h"
#include "../lib/crypto/crypto.h"

struct torture_context;

/*
 This uses the test values from rfc1321
*/
bool torture_local_crypto_md5(struct torture_context *torture) 
{
	bool ret = true;
	uint32_t i;
	struct {
		const char *data;
		const char *md5;
	} testarray[] = {
	{
		.data	= "",
		.md5	= "d41d8cd98f00b204e9800998ecf8427e"
	},{
		.data	= "a",
		.md5	= "0cc175b9c0f1b6a831c399e269772661"
	},{
		.data	= "abc",
		.md5	= "900150983cd24fb0d6963f7d28e17f72"
	},{
		.data	= "message digest",
		.md5	= "f96b697d7cb7938d525a2f31aaf161d0"
	},{
		.data	= "abcdefghijklmnopqrstuvwxyz",
		.md5	= "c3fcd3d76192e4007dfb496cca67e13b"
	},{
		.data	= "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
						 "abcdefghijklmnopqrstuvwxyz"
						 "0123456789",
		.md5	= "d174ab98d277d9f5a5611c2c9f419d9f"
	},{
		.data	= "123456789012345678901234567890"
						 "123456789012345678901234567890"
						 "12345678901234567890",
		.md5	= "57edf4a22be3c955ac49da2e2107b67a"
	}
	};

	for (i=0; i < ARRAY_SIZE(testarray); i++) {
		struct MD5Context ctx;
		uint8_t md5[16];
		int e;

		DATA_BLOB data;
		DATA_BLOB md5blob;

		data = data_blob_string_const(testarray[i].data);
		md5blob  = strhex_to_data_blob(NULL, testarray[i].md5);

		MD5Init(&ctx);
		MD5Update(&ctx, data.data, data.length);
		MD5Final(md5, &ctx);

		e = memcmp(md5blob.data,
			   md5,
			   MIN(md5blob.length, sizeof(md5)));
		if (e != 0) {
			printf("md5 test[%u]: failed\n", i);
			dump_data(0, data.data, data.length);
			dump_data(0, md5blob.data, md5blob.length);
			dump_data(0, md5, sizeof(md5));
			ret = false;
		}
		talloc_free(md5blob.data);
	}

	return ret;
}
