/*
 *   Copyright (C) The Internet Society (2001).  All Rights Reserved.
 *
 *   This document and translations of it may be copied and furnished to
 *   others, and derivative works that comment on or otherwise explain it
 *   or assist in its implementation may be prepared, copied, published
 *   and distributed, in whole or in part, without restriction of any
 *   kind, provided that the above copyright notice and this paragraph are
 *   included on all such copies and derivative works.  However, this
 *   document itself may not be modified in any way, such as by removing
 *   the copyright notice or references to the Internet Society or other
 *   Internet organizations, except as needed for the purpose of
 *   developing Internet standards in which case the procedures for
 *   copyrights defined in the Internet Standards process must be
 *   followed, or as required to translate it into languages other than
 *   English.
 *
 *   The limited permissions granted above are perpetual and will not be
 *   revoked by the Internet Society or its successors or assigns.
 *
 *   This document and the information contained herein is provided on an
 *   "AS IS" basis and THE INTERNET SOCIETY AND THE INTERNET ENGINEERING
 *   TASK FORCE DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING
 *   BUT NOT LIMITED TO ANY WARRANTY THAT THE USE OF THE INFORMATION
 *   HEREIN WILL NOT INFRINGE ANY RIGHTS OR ANY IMPLIED WARRANTIES OF
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: sha1.c 241182 2011-02-17 21:50:03Z $
 *
 * From rfc3174.txt
 */

/*
 *  sha1.c
 *
 *  Description:
 *      This file implements the Secure Hashing Algorithm 1 as
 *      defined in FIPS PUB 180-1 published April 17, 1995.
 *
 *      The SHA-1, produces a 160-bit message digest for a given
 *      data stream.  It should take about 2**n steps to find a
 *      message with the same digest as a given message and
 *      2**(n/2) to find any two messages with the same digest,
 *      when n is the digest size in bits.  Therefore, this
 *      algorithm can serve as a means of providing a
 *      "fingerprint" for a message.
 *
 *  Portability Issues:
 *      SHA-1 is defined in terms of 32-bit "words".  This code
 *      uses <stdint.h> (included via "sha1.h" to define 32 and 8
 *      bit unsigned integer types.  If your C compiler does not
 *      support 32 bit unsigned integers, this code is not
 *      appropriate.
 *
 *  Caveats:
 *      SHA-1 is designed to work with messages less than 2^64 bits
 *      long.  Although SHA-1 allows a message digest to be generated
 *      for messages of any number of bits less than 2^64, this
 *      implementation only works with messages with a length that is
 *      a multiple of the size of an 8-bit character.
 *
 */

#include <bcmcrypto/sha1.h>

#ifdef BCMDRIVER
#include <osl.h>
#else
#include <string.h>
#endif	/* BCMDRIVER */

/*
 *  Define the SHA1 circular left shift macro
 */
#define SHA1CircularShift(bits, word) \
	(((word) << (bits)) | ((word) >> (32-(bits))))

/* Local Function Prototyptes */
static void SHA1PadMessage(SHA1Context *);
static void SHA1ProcessMessageBlock(SHA1Context *);

/*
 *  SHA1Reset
 *
 *  Description:
 *      This function will initialize the SHA1Context in preparation
 *      for computing a new SHA1 message digest.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to reset.
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int
BCMROMFN(SHA1Reset)(SHA1Context *context)
{
	if (!context) {
		return shaNull;
	}

	context->Length_Low             = 0;
	context->Length_High            = 0;
	context->Message_Block_Index    = 0;

	context->Intermediate_Hash[0]   = 0x67452301;
	context->Intermediate_Hash[1]   = 0xEFCDAB89;
	context->Intermediate_Hash[2]   = 0x98BADCFE;
	context->Intermediate_Hash[3]   = 0x10325476;
	context->Intermediate_Hash[4]   = 0xC3D2E1F0;

	context->Computed   = 0;
	context->Corrupted  = 0;

	return shaSuccess;
}

/*
 *  SHA1Result
 *
 *  Description:
 *      This function will return the 160-bit message digest into the
 *      Message_Digest array  provided by the caller.
 *      NOTE: The first octet of hash is stored in the 0th element,
 *            the last octet of hash in the 19th element.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to use to calculate the SHA-1 hash.
 *      Message_Digest: [out]
 *          Where the digest is returned.
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int
BCMROMFN(SHA1Result)(SHA1Context *context, uint8 Message_Digest[SHA1HashSize])
{
	int i;

	if (!context || !Message_Digest) {
		return shaNull;
	}

	if (context->Corrupted) {
		return context->Corrupted;
	}

	if (!context->Computed) {
		SHA1PadMessage(context);
		for (i = 0; i < 64; ++i) {
			/* message may be sensitive, clear it out */
			context->Message_Block[i] = 0;
		}
		context->Length_Low = 0;    /* and clear length */
		context->Length_High = 0;
		context->Computed = 1;
	}

	for (i = 0; i < SHA1HashSize; ++i) {
		Message_Digest[i] = context->Intermediate_Hash[i >> 2] >>
		        8 * (3 - (i & 0x03));
	}

	return shaSuccess;
}

/*
 *  SHA1Input
 *
 *  Description:
 *      This function accepts an array of octets as the next portion
 *      of the message.
 *
 *  Parameters:
 *      context: [in/out]
 *          The SHA context to update
 *      message_array: [in]
 *          An array of characters representing the next portion of
 *          the message.
 *      length: [in]
 *          The length of the message in message_array
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int
BCMROMFN(SHA1Input)(SHA1Context *context, const uint8 *message_array, unsigned length)
{
	if (!length) {
		return shaSuccess;
	}

	if (!context || !message_array) {
		return shaNull;
	}

	if (context->Computed) {
		context->Corrupted = shaStateError;
		return shaStateError;
	}

	if (context->Corrupted) {
		return context->Corrupted;
	}
	while (length-- && !context->Corrupted) {
		context->Message_Block[context->Message_Block_Index++] =
		        (*message_array & 0xFF);

		context->Length_Low += 8;
		if (context->Length_Low == 0) {
			context->Length_High++;
			if (context->Length_High == 0) {
				/* Message is too long */
				context->Corrupted = 1;
			}
		}

		if (context->Message_Block_Index == 64) {
			SHA1ProcessMessageBlock(context);
		}

		message_array++;
	}

	return shaSuccess;
}

/*
 *  SHA1ProcessMessageBlock
 *
 *  Description:
 *      This function will process the next 512 bits of the message
 *      stored in the Message_Block array.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      Many of the variable names in this code, especially the
 *      single character names, were used because those were the
 *      names used in the publication.
 *
 *
 */
static void
SHA1ProcessMessageBlock(SHA1Context *context)
{
	const uint32 K[] =    {		/* Constants defined in SHA-1   */
		0x5A827999,
		0x6ED9EBA1,
		0x8F1BBCDC,
		0xCA62C1D6
	};
	int           t;		/* Loop counter                */
	uint32      temp;		/* Temporary word value        */
	uint32      W[80];		/* Word sequence               */
	uint32      A, B, C, D, E;	/* Word buffers                */

	/*
	 *  Initialize the first 16 words in the array W
	 */
	for (t = 0; t < 16; t++) {
		W[t] = context->Message_Block[t * 4] << 24;
		W[t] |= context->Message_Block[t * 4 + 1] << 16;
		W[t] |= context->Message_Block[t * 4 + 2] << 8;
		W[t] |= context->Message_Block[t * 4 + 3];
	}

	for (t = 16; t < 80; t++) {
		W[t] = SHA1CircularShift(1, W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
	}

	A = context->Intermediate_Hash[0];
	B = context->Intermediate_Hash[1];
	C = context->Intermediate_Hash[2];
	D = context->Intermediate_Hash[3];
	E = context->Intermediate_Hash[4];

	for (t = 0; t < 20; t++) {
		temp =  SHA1CircularShift(5, A) +
		        ((B & C) | ((~B) & D)) + E + W[t] + K[0];
		E = D;
		D = C;
		C = SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}

	for (t = 20; t < 40; t++) {
		temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
		E = D;
		D = C;
		C = SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}

	for (t = 40; t < 60; t++) {
		temp = SHA1CircularShift(5, A) +
		        ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
		E = D;
		D = C;
		C = SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}

	for (t = 60; t < 80; t++) {
		temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
		E = D;
		D = C;
		C = SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}

	context->Intermediate_Hash[0] += A;
	context->Intermediate_Hash[1] += B;
	context->Intermediate_Hash[2] += C;
	context->Intermediate_Hash[3] += D;
	context->Intermediate_Hash[4] += E;

	context->Message_Block_Index = 0;
}


/*
 *  SHA1PadMessage
 *
 *  Description:
 *      According to the standard, the message must be padded to an even
 *      512 bits.  The first padding bit must be a '1'.  The last 64
 *      bits represent the length of the original message.  All bits in
 *      between should be 0.  This function will pad the message
 *      according to those rules by filling the Message_Block array
 *      accordingly.  It will also call the ProcessMessageBlock function
 *      provided appropriately.  When it returns, it can be assumed that
 *      the message digest has been computed.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to pad
 *      ProcessMessageBlock: [in]
 *          The appropriate SHA*ProcessMessageBlock function
 *  Returns:
 *      Nothing.
 *
 */

static void
SHA1PadMessage(SHA1Context *context)
{
	/*
	 *  Check to see if the current message block is too small to hold
	 *  the initial padding bits and length.  If so, we will pad the
	 *  block, process it, and then continue padding into a second
	 *  block.
	 */
	if (context->Message_Block_Index > 55) {
		context->Message_Block[context->Message_Block_Index++] = 0x80;
		while (context->Message_Block_Index < 64) {
			context->Message_Block[context->Message_Block_Index++] = 0;
		}

		SHA1ProcessMessageBlock(context);

		while (context->Message_Block_Index < 56) {
			context->Message_Block[context->Message_Block_Index++] = 0;
		}
	} else {
		context->Message_Block[context->Message_Block_Index++] = 0x80;
		while (context->Message_Block_Index < 56) {
			context->Message_Block[context->Message_Block_Index++] = 0;
		}
	}

	/*
	 *  Store the message length as the last 8 octets
	 */
	context->Message_Block[56] = (uint8) (context->Length_High >> 24);
	context->Message_Block[57] = (uint8) (context->Length_High >> 16);
	context->Message_Block[58] = (uint8) (context->Length_High >> 8);
	context->Message_Block[59] = (uint8) (context->Length_High);
	context->Message_Block[60] = (uint8) (context->Length_Low >> 24);
	context->Message_Block[61] = (uint8) (context->Length_Low >> 16);
	context->Message_Block[62] = (uint8) (context->Length_Low >> 8);
	context->Message_Block[63] = (uint8) (context->Length_Low);

	SHA1ProcessMessageBlock(context);
}

#ifdef BCMSHA1_TEST
/*
 *  sha1test.c
 *
 *  Description:
 *      This file will exercise the SHA-1 code performing the three
 *      tests documented in FIPS PUB 180-1 plus one which calls
 *      SHA1Input with an exact multiple of 512 bits, plus a few
 *      error test checks.
 *
 *  Portability Issues:
 *      None.
 *
 */

#include <stdio.h>

/*
 *  Define patterns for testing
 */
#define TEST1   "abc"
#define TEST2a  "abcdbcdecdefdefgefghfghighijhi"
#define TEST2b  "jkijkljklmklmnlmnomnopnopq"
#define TEST2   TEST2a TEST2b
#define TEST3   "a"
#define TEST4a  "01234567012345670123456701234567"
#define TEST4b  "01234567012345670123456701234567"
/* an exact multiple of 512 bits */
#define TEST4   TEST4a TEST4b
char *testarray[4] =
{
	TEST1,
	TEST2,
	TEST3,
	TEST4
};
int repeatcount[4] = { 1, 1, 1000000, 10 };
uint8 resultarray[4][20] =
{
	{0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
	 0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D},
	{0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E, 0xBA, 0xAE,
	 0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5, 0xE5, 0x46, 0x70, 0xF1},
	{0x34, 0xAA, 0x97, 0x3C, 0xD4, 0xC4, 0xDA, 0xA4, 0xF6, 0x1E,
	 0xEB, 0x2B, 0xDB, 0xAD, 0x27, 0x31, 0x65, 0x34, 0x01, 0x6F},
	{0xDE, 0xA3, 0x56, 0xA2, 0xCD, 0xDD, 0x90, 0xC7, 0xA7, 0xEC,
	 0xED, 0xC5, 0xEB, 0xB5, 0x63, 0x93, 0x4F, 0x46, 0x04, 0x52}
};

int
main()
{
	SHA1Context sha;
	int i, j, err, fail = 0;
	uint8 Message_Digest[20];

	/*
	 *  Perform SHA-1 tests
	 */
	for (j = 0; j < 4; ++j) {
		printf("\nTest %d: %d, '%s'\n", j + 1, repeatcount[j], testarray[j]);

		err = SHA1Reset(&sha);
		if (err) {
			fprintf(stderr, "SHA1Reset Error %d.\n", err);
			break;    /* out of for j loop */
		}

		for (i = 0; i < repeatcount[j]; ++i) {
			err = SHA1Input(&sha,
			                (const unsigned char *) testarray[j],
			                strlen(testarray[j]));
			if (err) {
				fprintf(stderr, "SHA1Input Error %d.\n", err);
				break;    /* out of for i loop */
			}
		}

		err = SHA1Result(&sha, Message_Digest);
		if (err) {
			fprintf(stderr,
			        "SHA1Result Error %d, could not compute message digest.\n",
			        err);
		} else {
			printf("\t");
			for (i = 0; i < 20; ++i) {
				printf("%02X ", Message_Digest[i]);
			}
			printf("\n");
		}
		printf("Should match:\n");
		printf("\t");
		for (i = 0; i < 20; ++i) {
			printf("%02X ", resultarray[j][i]);
		}
		printf("\n");
		if (memcmp(Message_Digest, resultarray[j], 20)) fail++;
	}

	/* Test some error returns */
	err = SHA1Input(&sha, (const unsigned char *) testarray[1], 1);
	printf("\nError %d. Should be %d.\n", err, shaStateError);
	if (err != shaStateError) fail++;

	err = SHA1Reset(0);
	printf("\nError %d. Should be %d.\n", err, shaNull);
	if (err != shaNull) fail++;

	printf("SHA1 test %s\n", fail? "FAILED" : "PASSED");
	return fail;
}
#endif /* BCMSHA1_TEST */
