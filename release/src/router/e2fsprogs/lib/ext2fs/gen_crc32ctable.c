#include <stdio.h>
#include "crc32c_defs.h"
#include <inttypes.h>

#define ENTRIES_PER_LINE 4

#if CRC_LE_BITS <= 8
#define LE_TABLE_SIZE (1 << CRC_LE_BITS)
#else
#define LE_TABLE_SIZE 256
#endif

#if CRC_BE_BITS <= 8
#define BE_TABLE_SIZE (1 << CRC_BE_BITS)
#else
#define BE_TABLE_SIZE 256
#endif

static uint32_t crc32ctable_le[8][256];
static uint32_t crc32ctable_be[8][256];

/**
 * crc32cinit_le() - allocate and initialize LE table data
 *
 * crc is the crc of the byte i; other entries are filled in based on the
 * fact that crctable[i^j] = crctable[i] ^ crctable[j].
 *
 */
static void crc32cinit_le(void)
{
	unsigned i, j;
	uint32_t crc = 1;

	crc32ctable_le[0][0] = 0;

	for (i = LE_TABLE_SIZE >> 1; i; i >>= 1) {
		crc = (crc >> 1) ^ ((crc & 1) ? CRCPOLY_LE : 0);
		for (j = 0; j < LE_TABLE_SIZE; j += 2 * i)
			crc32ctable_le[0][i + j] = crc ^ crc32ctable_le[0][j];
	}
	for (i = 0; i < LE_TABLE_SIZE; i++) {
		crc = crc32ctable_le[0][i];
		for (j = 1; j < 8; j++) {
			crc = crc32ctable_le[0][crc & 0xff] ^ (crc >> 8);
			crc32ctable_le[j][i] = crc;
		}
	}
}

/**
 * crc32cinit_be() - allocate and initialize BE table data
 */
static void crc32cinit_be(void)
{
	unsigned i, j;
	uint32_t crc = 0x80000000;

	crc32ctable_be[0][0] = 0;

	for (i = 1; i < BE_TABLE_SIZE; i <<= 1) {
		crc = (crc << 1) ^ ((crc & 0x80000000) ? CRCPOLY_BE : 0);
		for (j = 0; j < i; j++)
			crc32ctable_be[0][i + j] = crc ^ crc32ctable_be[0][j];
	}
	for (i = 0; i < BE_TABLE_SIZE; i++) {
		crc = crc32ctable_be[0][i];
		for (j = 1; j < 8; j++) {
			crc = crc32ctable_be[0][(crc >> 24) & 0xff] ^
			      (crc << 8);
			crc32ctable_be[j][i] = crc;
		}
	}
}

static void output_table(uint32_t table[8][256], int len, char trans)
{
	int i, j;

	for (j = 0 ; j < 8; j++) {
		printf("static const uint32_t t%d_%ce[] = {", j, trans);
		for (i = 0; i < len - 1; i++) {
			if ((i % ENTRIES_PER_LINE) == 0)
				printf("\n");
			printf("to%ce(0x%8.8xL),", trans, table[j][i]);
			if ((i % ENTRIES_PER_LINE) != (ENTRIES_PER_LINE - 1))
				printf(" ");
		}
		printf("to%ce(0x%8.8xL)};\n\n", trans, table[j][len - 1]);

		if (trans == 'l') {
			if ((j+1)*8 >= CRC_LE_BITS)
				break;
		} else {
			if ((j+1)*8 >= CRC_BE_BITS)
				break;
		}
	}
}

int main(int argc, char **argv)
{
	printf("/*\n");
	printf(" * crc32ctable.h - CRC32c tables\n");
	printf(" *    this file is generated - do not edit\n");
	printf(" *	# gen_crc32ctable > crc32c_table.h\n");
	printf(" *    with\n");
	printf(" *	CRC_LE_BITS = %d\n", CRC_LE_BITS);
	printf(" *	CRC_BE_BITS = %d\n", CRC_BE_BITS);
	printf(" */\n");
	printf("#include <stdint.h>\n");

	if (CRC_LE_BITS > 1) {
		crc32cinit_le();
		output_table(crc32ctable_le, LE_TABLE_SIZE, 'l');
	}

	if (CRC_BE_BITS > 1) {
		crc32cinit_be();
		output_table(crc32ctable_be, BE_TABLE_SIZE, 'b');
	}

	return 0;
}
