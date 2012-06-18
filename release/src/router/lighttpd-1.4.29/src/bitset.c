#include "buffer.h"
#include "bitset.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define BITSET_BITS \
	( CHAR_BIT * sizeof(size_t) )

#define BITSET_MASK(pos) \
	( ((size_t)1) << ((pos) % BITSET_BITS) )

#define BITSET_WORD(set, pos) \
	( (set)->bits[(pos) / BITSET_BITS] )

#define BITSET_USED(nbits) \
	( ((nbits) + (BITSET_BITS - 1)) / BITSET_BITS )

bitset *bitset_init(size_t nbits) {
	bitset *set;

	set = malloc(sizeof(*set));
	assert(set);

	set->bits = calloc(BITSET_USED(nbits), sizeof(*set->bits));
	set->nbits = nbits;

	assert(set->bits);

	return set;
}

void bitset_reset(bitset *set) {
	memset(set->bits, 0, BITSET_USED(set->nbits) * sizeof(*set->bits));
}

void bitset_free(bitset *set) {
	free(set->bits);
	free(set);
}

void bitset_clear_bit(bitset *set, size_t pos) {
	if (pos >= set->nbits) {
	    SEGFAULT();
	}

	BITSET_WORD(set, pos) &= ~BITSET_MASK(pos);
}

void bitset_set_bit(bitset *set, size_t pos) {
	if (pos >= set->nbits) {
	    SEGFAULT();
	}

	BITSET_WORD(set, pos) |= BITSET_MASK(pos);
}

int bitset_test_bit(bitset *set, size_t pos) {
	if (pos >= set->nbits) {
	    SEGFAULT();
	}

	return (BITSET_WORD(set, pos) & BITSET_MASK(pos)) != 0;
}
