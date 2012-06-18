#ifndef _BITSET_H_
#define _BITSET_H_

#include <stddef.h>

typedef struct {
	size_t *bits;
	size_t nbits;
} bitset;

bitset *bitset_init(size_t nbits);
void bitset_reset(bitset *set);
void bitset_free(bitset *set);

void bitset_clear_bit(bitset *set, size_t pos);
void bitset_set_bit(bitset *set, size_t pos);
int bitset_test_bit(bitset *set, size_t pos);

#endif
