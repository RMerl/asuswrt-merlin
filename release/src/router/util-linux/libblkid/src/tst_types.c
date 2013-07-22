/*
 * This testing program makes sure the stdint.h header file
 *
 * Copyright (C) 2006 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <sys/types.h>
#include <stdint.h>

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	if (sizeof(uint8_t) != 1) {
		printf("Sizeof(uint8_t) is %d should be 1\n",
		       (int)sizeof(uint8_t));
		exit(1);
	}
	if (sizeof(int8_t) != 1) {
		printf("Sizeof(int8_t) is %d should be 1\n",
		       (int)sizeof(int8_t));
		exit(1);
	}
	if (sizeof(uint16_t) != 2) {
		printf("Sizeof(uint16_t) is %d should be 2\n",
		       (int)sizeof(uint16_t));
		exit(1);
	}
	if (sizeof(int16_t) != 2) {
		printf("Sizeof(int16_t) is %d should be 2\n",
		       (int)sizeof(int16_t));
		exit(1);
	}
	if (sizeof(uint32_t) != 4) {
		printf("Sizeof(uint32_t) is %d should be 4\n",
		       (int)sizeof(uint32_t));
		exit(1);
	}
	if (sizeof(int32_t) != 4) {
		printf("Sizeof(int32_t) is %d should be 4\n",
		       (int)sizeof(int32_t));
		exit(1);
	}
	if (sizeof(uint64_t) != 8) {
		printf("Sizeof(uint64_t) is %d should be 8\n",
		       (int)sizeof(uint64_t));
		exit(1);
	}
	if (sizeof(int64_t) != 8) {
		printf("Sizeof(int64_t) is %d should be 8\n",
		       (int)sizeof(int64_t));
		exit(1);
	}
	printf("The stdint.h types are correct.\n");
	exit(0);
}

