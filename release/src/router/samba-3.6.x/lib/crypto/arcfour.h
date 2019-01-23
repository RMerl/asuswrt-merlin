#ifndef ARCFOUR_HEADER_H
#define ARCFOUR_HEADER_H

#include "../lib/util/data_blob.h"

struct arcfour_state {
	uint8_t sbox[256];
	uint8_t index_i;
	uint8_t index_j;
};

void arcfour_init(struct arcfour_state *state, const DATA_BLOB *key);
void arcfour_crypt_sbox(struct arcfour_state *state, uint8_t *data, int len);
void arcfour_crypt_blob(uint8_t *data, int len, const DATA_BLOB *key);
void arcfour_crypt(uint8_t *data, const uint8_t keystr[16], int len);

#endif /* ARCFOUR_HEADER_H */
