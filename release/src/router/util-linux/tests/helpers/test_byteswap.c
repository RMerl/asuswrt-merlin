/*
 * This testing program makes sure the byteswap functions work
 *
 * Copyright (C) 2000 by Theodore Ts'o.
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the GNU Public
 * License.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <inttypes.h>

#include "bitops.h"

uint16_t ary16[] = {
	0x0001, 0x0100,
	0x1234, 0x3412,
	0xff00, 0x00ff,
	0x4000, 0x0040,
	0xfeff, 0xfffe,
	0x0000, 0x0000
	};

uint32_t ary32[] = {
	0x00000001, 0x01000000,
	0x80000000, 0x00000080,
	0x12345678, 0x78563412,
	0xffff0000, 0x0000ffff,
	0x00ff0000, 0x0000ff00,
	0xff000000, 0x000000ff,
	0x00000000, 0x00000000
	};

uint64_t ary64[] = {
	0x0000000000000001, 0x0100000000000000,
	0x8000000000000000, 0x0000000000000080,
	0x1234567812345678, 0x7856341278563412,
	0xffffffff00000000, 0x00000000ffffffff,
	0x00ff000000000000, 0x000000000000ff00,
	0xff00000000000000, 0x00000000000000ff,
	0x0000000000000000, 0x0000000000000000
	};

int main()
{
	int	i;
	int	errors = 0;

	printf("Testing swab16\n");
	i=0;
	do {
		printf("swab16(0x%04"PRIx16") = 0x%04"PRIx16"\n",
				ary16[i], swab16(ary16[i]));
		if (swab16(ary16[i]) != ary16[i+1]) {
			printf("Error!!!   %04"PRIx16" != %04"PRIx16"\n",
			       swab16(ary16[i]), ary16[i+1]);
			errors++;
		}
		if (swab16(ary16[i+1]) != ary16[i]) {
			printf("Error!!!   %04"PRIx16" != %04"PRIx16"\n",
			       swab16(ary16[i+1]), ary16[i]);
			errors++;
		}
		i += 2;
	} while (ary16[i] != 0);

	printf("Testing swab32\n");
	i = 0;
	do {
		printf("swab32(0x%08"PRIx32") = 0x%08"PRIx32"\n",
				ary32[i], swab32(ary32[i]));
		if (swab32(ary32[i]) != ary32[i+1]) {
			printf("Error!!!   %04"PRIx32" != %04"PRIx32"\n",
				swab32(ary32[i]), ary32[i+1]);
			errors++;
		}
		if (swab32(ary32[i+1]) != ary32[i]) {
			printf("Error!!!   %04"PRIx32" != %04"PRIx32"\n",
			       swab32(ary32[i+1]), ary32[i]);
			errors++;
		}
		i += 2;
	} while (ary32[i] != 0);

	printf("Testing swab64\n");
	i = 0;
	do {
		printf("swab64(0x%016"PRIx64") = 0x%016"PRIx64"\n",
				ary64[i], swab64(ary64[i]));
		if (swab64(ary64[i]) != ary64[i+1]) {
			printf("Error!!!   %016"PRIx64" != %016"PRIx64"\n",
				swab64(ary64[i]), ary64[i+1]);
			errors++;
		}
		if (swab64(ary64[i+1]) != ary64[i]) {
			printf("Error!!!   %016"PRIx64" != %016"PRIx64"\n",
			       swab64(ary64[i+1]), ary64[i]);
			errors++;
		}
		i += 2;
	} while (ary64[i] != 0);

	if (!errors)
		printf("No errors found in the byteswap implementation\n");

	return errors;
}
