/*
 * crc.c
 *
 * PURPOSE
 *	Routines to generate, calculate, and test a 16-bit CRC.
 *
 * DESCRIPTION
 *	The CRC code was devised by Don P. Mitchell of AT&T Bell Laboratories
 *	and Ned W. Rhodes of Software Systems Group. It has been published in
 *	"Design and Validation of Computer Protocols", Prentice Hall,
 *	Englewood Cliffs, NJ, 1991, Chapter 3, ISBN 0-13-539925-4.
 *
 *	Copyright is held by AT&T.
 *
 *	AT&T gives permission for the free use of the CRC source code.
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 */

#include "udfdecl.h"

static uint16_t crc_table[256] = {
	0x0000U, 0x1021U, 0x2042U, 0x3063U, 0x4084U, 0x50a5U, 0x60c6U, 0x70e7U,
	0x8108U, 0x9129U, 0xa14aU, 0xb16bU, 0xc18cU, 0xd1adU, 0xe1ceU, 0xf1efU,
	0x1231U, 0x0210U, 0x3273U, 0x2252U, 0x52b5U, 0x4294U, 0x72f7U, 0x62d6U,
	0x9339U, 0x8318U, 0xb37bU, 0xa35aU, 0xd3bdU, 0xc39cU, 0xf3ffU, 0xe3deU,
	0x2462U, 0x3443U, 0x0420U, 0x1401U, 0x64e6U, 0x74c7U, 0x44a4U, 0x5485U,
	0xa56aU, 0xb54bU, 0x8528U, 0x9509U, 0xe5eeU, 0xf5cfU, 0xc5acU, 0xd58dU,
	0x3653U, 0x2672U, 0x1611U, 0x0630U, 0x76d7U, 0x66f6U, 0x5695U, 0x46b4U,
	0xb75bU, 0xa77aU, 0x9719U, 0x8738U, 0xf7dfU, 0xe7feU, 0xd79dU, 0xc7bcU,
	0x48c4U, 0x58e5U, 0x6886U, 0x78a7U, 0x0840U, 0x1861U, 0x2802U, 0x3823U,
	0xc9ccU, 0xd9edU, 0xe98eU, 0xf9afU, 0x8948U, 0x9969U, 0xa90aU, 0xb92bU,
	0x5af5U, 0x4ad4U, 0x7ab7U, 0x6a96U, 0x1a71U, 0x0a50U, 0x3a33U, 0x2a12U,
	0xdbfdU, 0xcbdcU, 0xfbbfU, 0xeb9eU, 0x9b79U, 0x8b58U, 0xbb3bU, 0xab1aU,
	0x6ca6U, 0x7c87U, 0x4ce4U, 0x5cc5U, 0x2c22U, 0x3c03U, 0x0c60U, 0x1c41U,
	0xedaeU, 0xfd8fU, 0xcdecU, 0xddcdU, 0xad2aU, 0xbd0bU, 0x8d68U, 0x9d49U,
	0x7e97U, 0x6eb6U, 0x5ed5U, 0x4ef4U, 0x3e13U, 0x2e32U, 0x1e51U, 0x0e70U,
	0xff9fU, 0xefbeU, 0xdfddU, 0xcffcU, 0xbf1bU, 0xaf3aU, 0x9f59U, 0x8f78U,
	0x9188U, 0x81a9U, 0xb1caU, 0xa1ebU, 0xd10cU, 0xc12dU, 0xf14eU, 0xe16fU,
	0x1080U, 0x00a1U, 0x30c2U, 0x20e3U, 0x5004U, 0x4025U, 0x7046U, 0x6067U,
	0x83b9U, 0x9398U, 0xa3fbU, 0xb3daU, 0xc33dU, 0xd31cU, 0xe37fU, 0xf35eU,
	0x02b1U, 0x1290U, 0x22f3U, 0x32d2U, 0x4235U, 0x5214U, 0x6277U, 0x7256U,
	0xb5eaU, 0xa5cbU, 0x95a8U, 0x8589U, 0xf56eU, 0xe54fU, 0xd52cU, 0xc50dU,
	0x34e2U, 0x24c3U, 0x14a0U, 0x0481U, 0x7466U, 0x6447U, 0x5424U, 0x4405U,
	0xa7dbU, 0xb7faU, 0x8799U, 0x97b8U, 0xe75fU, 0xf77eU, 0xc71dU, 0xd73cU,
	0x26d3U, 0x36f2U, 0x0691U, 0x16b0U, 0x6657U, 0x7676U, 0x4615U, 0x5634U,
	0xd94cU, 0xc96dU, 0xf90eU, 0xe92fU, 0x99c8U, 0x89e9U, 0xb98aU, 0xa9abU,
	0x5844U, 0x4865U, 0x7806U, 0x6827U, 0x18c0U, 0x08e1U, 0x3882U, 0x28a3U,
	0xcb7dU, 0xdb5cU, 0xeb3fU, 0xfb1eU, 0x8bf9U, 0x9bd8U, 0xabbbU, 0xbb9aU,
	0x4a75U, 0x5a54U, 0x6a37U, 0x7a16U, 0x0af1U, 0x1ad0U, 0x2ab3U, 0x3a92U,
	0xfd2eU, 0xed0fU, 0xdd6cU, 0xcd4dU, 0xbdaaU, 0xad8bU, 0x9de8U, 0x8dc9U,
	0x7c26U, 0x6c07U, 0x5c64U, 0x4c45U, 0x3ca2U, 0x2c83U, 0x1ce0U, 0x0cc1U,
	0xef1fU, 0xff3eU, 0xcf5dU, 0xdf7cU, 0xaf9bU, 0xbfbaU, 0x8fd9U, 0x9ff8U,
	0x6e17U, 0x7e36U, 0x4e55U, 0x5e74U, 0x2e93U, 0x3eb2U, 0x0ed1U, 0x1ef0U
};

/*
 * udf_crc
 *
 * PURPOSE
 *	Calculate a 16-bit CRC checksum using ITU-T V.41 polynomial.
 *
 * DESCRIPTION
 *	The OSTA-UDF(tm) 1.50 standard states that using CRCs is mandatory.
 *	The polynomial used is:	x^16 + x^12 + x^15 + 1
 *
 * PRE-CONDITIONS
 *	data		Pointer to the data block.
 *	size		Size of the data block.
 *
 * POST-CONDITIONS
 *	<return>	CRC of the data block.
 *
 * HISTORY
 *	July 21, 1997 - Andrew E. Mileski
 *	Adapted from OSTA-UDF(tm) 1.50 standard.
 */
uint16_t
udf_crc(uint8_t *data, uint32_t size, uint16_t crc)
{
	while (size--)
		crc = crc_table[(crc >> 8 ^ *(data++)) & 0xffU] ^ (crc << 8);

	return crc;
}

/****************************************************************************/
#if defined(TEST)

/*
 * PURPOSE
 *	Test udf_crc()
 *
 * HISTORY
 *	July 21, 1997 - Andrew E. Mileski
 *	Adapted from OSTA-UDF(tm) 1.50 standard.
 */

unsigned char bytes[] = { 0x70U, 0x6AU, 0x77U };

int main(void)
{
	unsigned short x;

	x = udf_crc16(bytes, sizeof bytes);
	printf("udf_crc16: calculated = %4.4x, correct = %4.4x\n", x, 0x3299U);

	return 0;
}

#endif /* defined(TEST) */

/****************************************************************************/
#if defined(GENERATE)

/*
 * PURPOSE
 *	Generate a table for fast 16-bit CRC calculations (any polynomial).
 *
 * DESCRIPTION
 *	The ITU-T V.41 polynomial is 010041.
 *
 * HISTORY
 *	July 21, 1997 - Andrew E. Mileski
 *	Adapted from OSTA-UDF(tm) 1.50 standard.
 */

#include <stdio.h>

int main(int argc, char **argv)
{
	unsigned long crc, poly;
	int n, i;

	/* Get the polynomial */
	sscanf(argv[1], "%lo", &poly);
	if (poly & 0xffff0000U){
		fprintf(stderr, "polynomial is too large\en");
		exit(1);
	}

	printf("/* CRC 0%o */\n", poly);

	/* Create a table */
	printf("static unsigned short crc_table[256] = {\n");
	for (n = 0; n < 256; n++){
		if (n % 8 == 0)
			printf("\t");
		crc = n << 8;
		for (i = 0; i < 8; i++){
			if(crc & 0x8000U)
				crc = (crc << 1) ^ poly;
			else
				crc <<= 1;
		crc &= 0xFFFFU;
		}
		if (n == 255)
			printf("0x%04xU ", crc);
		else
			printf("0x%04xU, ", crc);
		if(n % 8 == 7)
			printf("\n");
	}
	printf("};\n");

	return 0;
}

#endif /* defined(GENERATE) */
