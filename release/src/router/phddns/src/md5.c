// md5.cpp : Defines the entry point for the DLL application.
//


#include "md5.h"

/* MD5 initialization. Begins an MD5 operation, writing a new context. */
void MD5Init(MD5_CTX *context)                                         /* context */
{
  context->count[0] = context->count[1] = 0;
  /* Load magic initialization constants.*/
  context->state[0] = 0x67452301;
  context->state[1] = 0xefcdab89;
  context->state[2] = 0x98badcfe;
  context->state[3] = 0x10325476;
}
/* MD5 block update operation. Continues an MD5 message-digest
  operation, processing another message block, and updating the  context. */

void MD5Update (MD5_CTX *context, unsigned char *input, unsigned int inputLen)
	{
	unsigned int i, index, partLen;			  /* Compute number of bytes mod 64 */
	index = (unsigned int)((context->count[0] >> 3) & 0x3F);
  /* Update number of bits */  
	if ((context->count[0] += ((unsigned int)inputLen << 3))
	   < ((unsigned int)inputLen << 3)) context->count[1]++;

  context->count[1] += ((unsigned int)inputLen >> 29);
  partLen = 64 - index;
  /* Transform as many times as possible.*/
  if (inputLen >= partLen) 
  {
	MD5_memcpy((unsigned char *)&context->buffer[index], (unsigned char *)input, partLen);
	MD5Transform(context->state, context->buffer);

	for (i = partLen; i + 63 < inputLen; i += 64)
	MD5Transform (context->state, &input[i]);
	index = 0;
  }  
  else i = 0;
  /* Buffer remaining input */
  MD5_memcpy((unsigned char *)&context->buffer[index], (unsigned char *)&input[i],  inputLen-i);
	}
/* MD5 finalization. Ends an MD5 message-digest operation, writing the
  the message digest and zeroizing the context. */
void MD5Final (unsigned char* digest, MD5_CTX *context)
	{
	unsigned char bits[8];
	unsigned int index, padLen;  /* Save number of bits */
	Encode (bits, context->count, 8);  /* Pad out to 56 mod 64.*/
	index = (unsigned int)((context->count[0] >> 3) & 0x3f);
	padLen = (index < 56) ? (56 - index) : (120 - index);
	MD5Update (context, PADDING, padLen);  /* Append length (before padding) */
	MD5Update (context, bits, 8);
	  /* Store state in digest */  
	Encode (digest, context->state, 16);
  /* Zeroize sensitive information.*/
  MD5_memset ((unsigned char *)context, 0, sizeof (*context));
	}

/* MD5 basic transformation. Transforms state based on block. */
static void MD5Transform (unsigned int state[4], unsigned char block[64])
	{
  unsigned int a = state[0], b = state[1], c = state[2], d = state[3], x[16];
  Decode (x, block, 64);
  /* Round 1 */
  FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
  FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
  FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
  FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
  FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
  FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
  FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
  FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
  FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
  FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
  FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
  FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
  FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
  FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
  FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
  FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */ 
  /* Round 2 */
  GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
  GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
  GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
  GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
  GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
  GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
  GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
  GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
  GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
  GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
  GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
  GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
  GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
  GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
  GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
  GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */
  /* Round 3 */
  HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
  HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
  HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
  HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
  HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
  HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
  HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
  HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
  HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
  HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
  HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
  HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
  HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
  HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
  HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
  HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */
  /* Round 4 */
  II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
  II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
  II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
  II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
  II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
  II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
  II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
  II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
  II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
  II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
  II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
  II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
  II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
  II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
  II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
  II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  /* Zeroize sensitive information.
  */  
  MD5_memset ((unsigned char *)x, 0, sizeof (x));
	}

/* Encodes input (unsigned int) into output (unsigned char). Assumes len is
  a multiple of 4. */
	static void Encode (unsigned char *output, unsigned int *input, unsigned int len)
	{  
		unsigned int i, j;
		for (i = 0, j = 0; j < len; i++, j += 4) 
		{
			output[j] = (char)(input[i] & 0xff);
			output[j+1] = (char)((input[i] >> 8) & 0xff);
			output[j+2] = (char)((input[i] >> 16) & 0xff);
			output[j+3] = (char)((input[i] >> 24) & 0xff);  
		}
	}

/* Decodes input (unsigned char) into output (unsigned int). Assumes len is
  a multiple of 4. */
	static void Decode (unsigned int *output, unsigned char *input, unsigned int len)
		{  
			unsigned int i, j;
			for (i = 0, j = 0; j < len; i++, j += 4)
				output[i] = ((unsigned int)input[j]) | (((unsigned int)input[j+1]) << 8) |
				(((unsigned int)input[j+2]) << 16) | (((unsigned int)input[j+3]) << 24);
		}

/* Note: Replace "for loop" with standard memcpy if possible. */
	static void MD5_memcpy (unsigned char * output, unsigned char * input, unsigned int len)
		{  
			unsigned int i;  for (i = 0; i < len; i++)
			 output[i] = input[i];
		}

/* Note: Replace "for loop" with standard memset if possible. */

static void MD5_memset (unsigned char * output, int value, unsigned int len)
	{  
		unsigned int i;
		for (i = 0; i < len; i++)
			((char *)output)[i] = (char)value;
	}


int MDString (unsigned char* string, unsigned int nLen, unsigned char* digest)

{  
	MD_CTX context;
	//char digest[16];
	MDInit (&context);
	MDUpdate (&context, string, nLen);
	MDFinal (digest, &context);
	//digest[0] ='a';

	return 1;
}


#define PADLEN 64
#define MAXMD5SOURCESTRINGLEN 1024
unsigned int KeyMD5Encode(unsigned char* szEncoded, const unsigned char* szData, unsigned int nSize, unsigned char* szKey, unsigned int nKeyLen)
{
	//See rfc2104
	unsigned char ipad[PADLEN] = {0}, opad[PADLEN] = {0};
	unsigned char K[PADLEN] = {0};
	unsigned char firstPad[PADLEN + MAXMD5SOURCESTRINGLEN + 1] = {0};
	unsigned char midResult[PADLEN + 16] = {0};
	int i;

	memset(ipad, 0x36, PADLEN);
	memset(opad, 0x5c, PADLEN);
	
	
	memcpy(K, szKey, nKeyLen);
	
	for(i = 0; i < PADLEN; i++)
	{
		ipad[i] ^= K[i];
		opad[i] ^= K[i];
	}
	
	
	memcpy(firstPad, ipad, PADLEN);
	memcpy(firstPad + PADLEN, szData, nSize);
	firstPad[PADLEN + nSize] = '\x0';
	MDString(firstPad, PADLEN + nSize, midResult + PADLEN);
	
	memcpy(midResult, opad, PADLEN);
	MDString(midResult, PADLEN + 16, szEncoded);
	
	return 16;
}