/* 
   Unix SMB/CIFS implementation.

   An implementation of the arcfour algorithm

   Copyright (C) Andrew Tridgell 1998
   
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
#include "../lib/crypto/arcfour.h"

/* initialise the arcfour sbox with key */
_PUBLIC_ void arcfour_init(struct arcfour_state *state, const DATA_BLOB *key) 
{
	int ind;
	uint8_t j = 0;
	for (ind = 0; ind < sizeof(state->sbox); ind++) {
		state->sbox[ind] = (uint8_t)ind;
	}
	
	for (ind = 0; ind < sizeof(state->sbox); ind++) {
		uint8_t tc;
		
		j += (state->sbox[ind] + key->data[ind%key->length]);
		
		tc = state->sbox[ind];
		state->sbox[ind] = state->sbox[j];
		state->sbox[j] = tc;
	}
	state->index_i = 0;
	state->index_j = 0;
}

/* crypt the data with arcfour */
_PUBLIC_ void arcfour_crypt_sbox(struct arcfour_state *state, uint8_t *data, int len) 
{
	int ind;
	
	for (ind = 0; ind < len; ind++) {
		uint8_t tc;
		uint8_t t;

		state->index_i++;
		state->index_j += state->sbox[state->index_i];

		tc = state->sbox[state->index_i];
		state->sbox[state->index_i] = state->sbox[state->index_j];
		state->sbox[state->index_j] = tc;
		
		t = state->sbox[state->index_i] + state->sbox[state->index_j];
		data[ind] = data[ind] ^ state->sbox[t];
	}
}

/*
  arcfour encryption with a blob key
*/
_PUBLIC_ void arcfour_crypt_blob(uint8_t *data, int len, const DATA_BLOB *key) 
{
	struct arcfour_state state;
	arcfour_init(&state, key);
	arcfour_crypt_sbox(&state, data, len);
}

/*
  a variant that assumes a 16 byte key. This should be removed
  when the last user is gone
*/
_PUBLIC_ void arcfour_crypt(uint8_t *data, const uint8_t keystr[16], int len)
{
	DATA_BLOB key = data_blob(keystr, 16);
	
	arcfour_crypt_blob(data, len, &key);

	data_blob_free(&key);
}


