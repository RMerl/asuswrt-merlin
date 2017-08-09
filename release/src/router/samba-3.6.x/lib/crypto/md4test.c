/* 
   Unix SMB/CIFS implementation.
   MD4 tests
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

#include "replace.h"
#include "../lib/util/util.h"
#include "../lib/crypto/crypto.h"

struct torture_context;

/*
 This uses the test values from rfc1320
*/
bool torture_local_crypto_md4(struct torture_context *torture)
{
	bool ret = true;
	uint32_t i;
	struct {
		const char *data;
		const char *md4;
	} testarray[] = {
	{
		.data	= "",
		.md4	= "31d6cfe0d16ae931b73c59d7e0c089c0"
	},{
		.data	= "a",
		.md4	= "bde52cb31de33e46245e05fbdbd6fb24"
	},{
		.data	= "abc",
		.md4	= "a448017aaf21d8525fc10ae87aa6729d"
	},{
		.data	= "message digest",
		.md4	= "d9130a8164549fe818874806e1c7014b"
	},{
		.data	= "abcdefghijklmnopqrstuvwxyz",
		.md4	= "d79e1c308aa5bbcdeea8ed63df412da9"
	},{
		.data	= "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
		.md4	= "043f8582f241db351ce627e153e7f0e4"
	},{
		.data	= "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
		.md4	= "e33b4ddc9c38f2199c3e7b164fcc0536"
	}
	};

	for (i=0; i < ARRAY_SIZE(testarray); i++) {
		uint8_t md4[16];
		int e;
		DATA_BLOB data;
		DATA_BLOB md4blob;

		data = data_blob_string_const(testarray[i].data);
		md4blob  = strhex_to_data_blob(NULL, testarray[i].md4);

		mdfour(md4, data.data, data.length);

		e = memcmp(md4blob.data, md4, MIN(md4blob.length, sizeof(md4)));
		if (e != 0) {
			printf("md4 test[%u]: failed\n", i);
			dump_data(0, data.data, data.length);
			dump_data(0, md4blob.data, md4blob.length);
			dump_data(0, md4, sizeof(md4));
			ret = false;
		}
		talloc_free(md4blob.data);
	}

	return ret;
}
