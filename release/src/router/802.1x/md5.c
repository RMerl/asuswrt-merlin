
#include <stdio.h>
#include <string.h>

#include <endian.h>

#include "common.h"
#include "rtmp_type.h"
#include "md5.h"

int hostapd_get_rand(u8 *buf, size_t len)
{
	FILE *f;
	size_t rc;

	f = fopen("/dev/urandom", "r");
	if (f == NULL) {
		printf("Could not open /dev/urandom.\n");
		return -1;
	}

	rc = fread(buf, 1, len, f);
	fclose(f);

	return rc != len ? -1 : 0;
}

/**
 * md5_mac:
 * @key: pointer to	the	key	used for MAC generation
 * @key_len: length	of the key in bytes
 * @data: pointer to the data area for which the MAC is	generated
 * @data_len: length of	the	data in	bytes
 * @mac: pointer to	the	buffer holding space for the MAC; the buffer should
 * have	space for 128-bit (16 bytes) MD5 hash value
 *
 * md5_mac() determines	the	message	authentication code	by using secure	hash
 * MD5(key | data |	key).
 */
void md5_mac(UCHAR *key, ULONG key_len, UCHAR *data, ULONG data_len, UCHAR *mac)
{
	MD5_CTX	context;

	MD5Init(&context);
	MD5Update(&context,	key, key_len);
	MD5Update(&context,	data, data_len);
	MD5Update(&context,	key, key_len);
	MD5Final(mac, &context);
}


/**
 * hmac_md5:
 * @key: pointer to	the	key	used for MAC generation
 * @key_len: length	of the key in bytes
 * @data: pointer to the data area for which the MAC is	generated
 * @data_len: length of	the	data in	bytes
 * @mac: pointer to	the	buffer holding space for the MAC; the buffer should
 * have	space for 128-bit (16 bytes) MD5 hash value
 *
 * hmac_md5() determines the message authentication	code using HMAC-MD5.
 * This	implementation is based	on the sample code presented in	RFC	2104.
 */
void hmac_md5(UCHAR *key, ULONG key_len, UCHAR *data, ULONG data_len, UCHAR *mac)
{
	MD5_CTX	context;
	UCHAR k_ipad[65]; /* inner	padding	- key XORd with	ipad */
	UCHAR k_opad[65]; /* outer	padding	- key XORd with	opad */
	UCHAR tk[16];
	int	i;

	//assert(key != NULL && data != NULL && mac != NULL);

	/* if key is longer	than 64	bytes reset	it to key =	MD5(key) */
	if (key_len	> 64) {
		MD5_CTX	ttcontext;

		MD5Init(&ttcontext);
		MD5Update(&ttcontext, key, key_len);
		MD5Final(tk, &ttcontext);
		//key=(PUCHAR)ttcontext.buf;
		key	= tk;
		key_len	= 16;
	}

	/* the HMAC_MD5	transform looks	like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte	key
	 * ipad	is the byte	0x36 repeated 64 times
	 * opad	is the byte	0x5c repeated 64 times
	 * and text	is the data	being protected	*/

	/* start out by	storing	key	in pads	*/
	NdisZeroMemory(k_ipad, sizeof(k_ipad));
	NdisZeroMemory(k_opad,	sizeof(k_opad));
	//assert(key_len < sizeof(k_ipad));
	NdisMoveMemory(k_ipad, key,	key_len);
	NdisMoveMemory(k_opad, key,	key_len);

	/* XOR key with	ipad and opad values */
	for	(i = 0;	i <	64;	i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/* perform inner MD5 */
	MD5Init(&context);					 /*	init context for 1st pass */
	MD5Update(&context,	k_ipad,	64);	 /*	start with inner pad */
	MD5Update(&context,	data, data_len); /*	then text of datagram */
	MD5Final(mac, &context);			 /*	finish up 1st pass */

	/* perform outer MD5 */
	MD5Init(&context);					 /*	init context for 2nd pass */
	MD5Update(&context,	k_opad,	64);	 /*	start with outer pad */
	MD5Update(&context,	mac, 16);		 /*	then results of	1st	hash */
	MD5Final(mac, &context);			 /*	finish up 2nd pass */
}

#if __BYTE_ORDER == __BIG_ENDIAN
void byteReverse(unsigned char *buf, unsigned longs);
void byteReverse(unsigned char *buf, unsigned longs)
{
	unsigned char tmp;
	
    do {
	   tmp = buf[3];
	   buf[3] = buf[0];
	   buf[0] = tmp;
	   tmp = buf[2];
	   buf[2] = buf[1];
	   buf[1] = tmp;
	   buf += 4;
    } while (--longs);
}
#else
#define byteReverse(buf, len)	// Nothing
#endif

/* ==========================  MD5 implementation =========================== */
// four base functions for MD5
#define MD5_F1(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define MD5_F2(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define MD5_F3(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_F4(x, y, z) ((y) ^ ((x) | (~z)))
#define CYCLIC_LEFT_SHIFT(w, s) (((w) << (s)) | ((w) >> (32-(s))))

#define	MD5Step(f, w, x, y,	z, data, t, s)	\
	( w	+= f(x,	y, z) +	data + t,  w = (CYCLIC_LEFT_SHIFT(w, s)) & 0xffffffff, w +=	x )


/*
 *  Function Description:
 *      Initiate MD5 Context satisfied in RFC 1321
 *
 *  Arguments:
 *      pCtx        Pointer	to MD5 context
 *
 *  Return Value:
 *      None	    
 */
VOID MD5Init(MD5_CTX *pCtx)
{
    pCtx->Buf[0]=0x67452301;
    pCtx->Buf[1]=0xefcdab89;
    pCtx->Buf[2]=0x98badcfe;
    pCtx->Buf[3]=0x10325476;

    pCtx->LenInBitCount[0]=0;
    pCtx->LenInBitCount[1]=0;
}


/*
 *  Function Description:
 *      Update MD5 Context, allow of an arrary of octets as the next portion 
 *      of the message
 *      
 *  Arguments:
 *      pCtx		Pointer	to MD5 context
 * 	    pData       Pointer to input data
 *      LenInBytes  The length of input data (unit: byte)
 *
 *  Return Value:
 *      None
 *
 *  Note:
 *      Called after MD5Init or MD5Update(itself)   
 */
VOID MD5Update(MD5_CTX *pCtx, UCHAR *pData, ULONG LenInBytes)
{
    
    ULONG TfTimes;
    ULONG temp;
	unsigned int i;
    
    temp = pCtx->LenInBitCount[0];

    pCtx->LenInBitCount[0] = (ULONG) (pCtx->LenInBitCount[0] + (LenInBytes << 3));
 
    if (pCtx->LenInBitCount[0] < temp)
        pCtx->LenInBitCount[1]++;   //carry in

    pCtx->LenInBitCount[1] += LenInBytes >> 29;

    // mod 64 bytes
    temp = (temp >> 3) & 0x3f;  
    
    // process lacks of 64-byte data 
    if (temp) 
    {
        UCHAR *pAds = (UCHAR *) pCtx->Input + temp;

        if ((temp+LenInBytes) < 64)
        {
            NdisMoveMemory(pAds, (UCHAR *)pData, LenInBytes);   
            return;
        }
        
        NdisMoveMemory(pAds, (UCHAR *)pData, 64-temp);               
        byteReverse(pCtx->Input, 16);
        MD5Transform(pCtx->Buf, (ULONG *)pCtx->Input);

        pData += 64-temp;
        LenInBytes -= 64-temp; 
    } // end of if (temp)
    
     
    TfTimes = (LenInBytes >> 6);

    for (i=TfTimes; i>0; i--)
    {
        NdisMoveMemory(pCtx->Input, (UCHAR *)pData, 64);
        byteReverse(pCtx->Input, 16);
        MD5Transform(pCtx->Buf, (ULONG *)pCtx->Input);
        pData += 64;
        LenInBytes -= 64;
    } // end of for

    // buffering lacks of 64-byte data
    if(LenInBytes)
        NdisMoveMemory(pCtx->Input, (UCHAR *)pData, LenInBytes);   
   
}


/*
 *  Function Description:
 *      Append padding bits and length of original message in the tail 
 *      The message digest has to be completed in the end  
 *  
 *  Arguments:
 *      Digest		Output of Digest-Message for MD5
 *  	pCtx        Pointer	to MD5 context
 * 	
 *  Return Value:
 *      None
 *  
 *  Note:
 *      Called after MD5Update
 */
VOID MD5Final(UCHAR Digest[16], MD5_CTX *pCtx)
{
    UCHAR Remainder;
    UCHAR PadLenInBytes;
    UCHAR *pAppend=0;
    unsigned int i;
    
    Remainder = (UCHAR)((pCtx->LenInBitCount[0] >> 3) & 0x3f);

    PadLenInBytes = (Remainder < 56) ? (56-Remainder) : (120-Remainder);
    
    pAppend = (UCHAR *)pCtx->Input + Remainder;

    // padding bits without crossing block(64-byte based) boundary
    if (Remainder < 56)
    {
        *pAppend = 0x80;
        PadLenInBytes --;
        
        NdisZeroMemory((UCHAR *)pCtx->Input + Remainder+1, PadLenInBytes); 
		
		// add data-length field, from low to high
       	for (i=0; i<4; i++)
        {
        	pCtx->Input[56+i] = (UCHAR)((pCtx->LenInBitCount[0] >> (i << 3)) & 0xff);
        	pCtx->Input[60+i] = (UCHAR)((pCtx->LenInBitCount[1] >> (i << 3)) & 0xff);
      	}
      	
        byteReverse(pCtx->Input, 16);
        MD5Transform(pCtx->Buf, (ULONG *)pCtx->Input);
    } // end of if
    
    // padding bits with crossing block(64-byte based) boundary
    else
    {
        // the first block ===
        *pAppend = 0x80;
        PadLenInBytes --;
       
        NdisZeroMemory((UCHAR *)pCtx->Input + Remainder+1, (64-Remainder-1)); 
        PadLenInBytes -= (64 - Remainder - 1);
        
        byteReverse(pCtx->Input, 16);
        MD5Transform(pCtx->Buf, (ULONG *)pCtx->Input);
        

        // the second block ===
        NdisZeroMemory((UCHAR *)pCtx->Input, PadLenInBytes); 

        // add data-length field
        for (i=0; i<4; i++)
        {
        	pCtx->Input[56+i] = (UCHAR)((pCtx->LenInBitCount[0] >> (i << 3)) & 0xff);
        	pCtx->Input[60+i] = (UCHAR)((pCtx->LenInBitCount[1] >> (i << 3)) & 0xff);
      	}

        byteReverse(pCtx->Input, 16);
        MD5Transform(pCtx->Buf, (ULONG *)pCtx->Input);
    } // end of else


    NdisMoveMemory((UCHAR *)Digest, (ULONG *)pCtx->Buf, 16); // output
    byteReverse((UCHAR *)Digest, 4);
    NdisZeroMemory(pCtx, sizeof(pCtx)); // memory free 
}


/*
 *  Function Description:
 *      The central algorithm of MD5, consists of four rounds and sixteen 
 *  	steps per round
 * 
 *  Arguments:
 *      Buf     Buffers of four states (output: 16 bytes)		
 * 	    Mes     Input data (input: 64 bytes) 
 *  
 *  Return Value:
 *      None
 *  	
 *  Note:
 *      Called by MD5Update or MD5Final
 */
VOID MD5Transform(ULONG Buf[4], ULONG Mes[16])
{  
    ULONG Reg[4], Temp; 
	unsigned int i;
    
    static UCHAR LShiftVal[16] = 
    { 	
        7, 12, 17, 22, 	
		5, 9 , 14, 20, 
		4, 11, 16, 23, 
 		6, 10, 15, 21,
 	};

	
	// [equal to 4294967296*abs(sin(index))]
    static ULONG MD5Table[64] = 
	{ 
		0xd76aa478,	0xe8c7b756,	0x242070db,	0xc1bdceee,	
		0xf57c0faf,	0x4787c62a,	0xa8304613, 0xfd469501,	
		0x698098d8,	0x8b44f7af,	0xffff5bb1,	0x895cd7be,
    	0x6b901122,	0xfd987193,	0xa679438e,	0x49b40821,
    	
    	0xf61e2562,	0xc040b340,	0x265e5a51,	0xe9b6c7aa,
    	0xd62f105d,	0x02441453,	0xd8a1e681,	0xe7d3fbc8,
    	0x21e1cde6,	0xc33707d6,	0xf4d50d87,	0x455a14ed,
    	0xa9e3e905,	0xfcefa3f8,	0x676f02d9,	0x8d2a4c8a,
    	           
    	0xfffa3942,	0x8771f681,	0x6d9d6122,	0xfde5380c,
    	0xa4beea44,	0x4bdecfa9,	0xf6bb4b60,	0xbebfbc70,
    	0x289b7ec6,	0xeaa127fa,	0xd4ef3085,	0x04881d05,
    	0xd9d4d039,	0xe6db99e5,	0x1fa27cf8,	0xc4ac5665,
    	           
    	0xf4292244,	0x432aff97,	0xab9423a7,	0xfc93a039,
   		0x655b59c3,	0x8f0ccc92,	0xffeff47d,	0x85845dd1,
    	0x6fa87e4f,	0xfe2ce6e0,	0xa3014314,	0x4e0811a1,
    	0xf7537e82,	0xbd3af235,	0x2ad7d2bb,	0xeb86d391
	};
 
				
    for (i=0; i<4; i++)
        Reg[i]=Buf[i];
			
				
    // 64 steps in MD5 algorithm
    for (i=0; i<16; i++)                    
    {
        MD5Step(MD5_F1, Reg[0], Reg[1], Reg[2], Reg[3], Mes[i],               
                MD5Table[i], LShiftVal[i & 0x3]);

        // one-word right shift
        Temp   = Reg[3]; 
        Reg[3] = Reg[2];
        Reg[2] = Reg[1];
        Reg[1] = Reg[0];
        Reg[0] = Temp;            
    }
    for (i=16; i<32; i++)                    
    {
        MD5Step(MD5_F2, Reg[0], Reg[1], Reg[2], Reg[3], Mes[(5*(i & 0xf)+1) & 0xf], 
                MD5Table[i], LShiftVal[(0x1 << 2)+(i & 0x3)]);    

        // one-word right shift
        Temp   = Reg[3]; 
        Reg[3] = Reg[2];
        Reg[2] = Reg[1];
        Reg[1] = Reg[0];
        Reg[0] = Temp;           
    }
    for (i=32; i<48; i++)                    
    {
        MD5Step(MD5_F3, Reg[0], Reg[1], Reg[2], Reg[3], Mes[(3*(i & 0xf)+5) & 0xf], 
                MD5Table[i], LShiftVal[(0x1 << 3)+(i & 0x3)]);        

        // one-word right shift
        Temp   = Reg[3]; 
        Reg[3] = Reg[2];
        Reg[2] = Reg[1];
        Reg[1] = Reg[0];
        Reg[0] = Temp;          
    }
    for (i=48; i<64; i++)                    
    {
        MD5Step(MD5_F4, Reg[0], Reg[1], Reg[2], Reg[3], Mes[(7*(i & 0xf)) & 0xf], 
                MD5Table[i], LShiftVal[(0x3 << 2)+(i & 0x3)]);   

        // one-word right shift
        Temp   = Reg[3]; 
        Reg[3] = Reg[2];
        Reg[2] = Reg[1];
        Reg[1] = Reg[0];
        Reg[0] = Temp;           
    }
    
      
    // (temporary)output
    for (i=0; i<4; i++)
        Buf[i] += Reg[i];

}



#define S_SWAP(a,b) do { u8 t = S[a]; S[a] = S[b]; S[b] = t; } while(0)

void rc4(u8 *buf, size_t len, u8 *key, size_t key_len)
{
	u8 S[256];
	u32 i, j, k;
	u8 kpos, *pos;

	/* Setup RC4 state */
	for (i = 0; i < 256; i++)
		S[i] = i;
	j = 0;
	kpos = 0;
	for (i = 0; i < 256; i++) {
		j = (j + S[i] + key[kpos]) & 0xff;
		kpos++;
		if (kpos >= key_len)
			kpos = 0;
		S_SWAP(i, j);
	}

	/* Apply RC4 to data */
	pos = buf;
	i = j = 0;
	for (k = 0; k < len; k++) {
		i = (i + 1) & 0xff;
		j = (j + S[i]) & 0xff;
		S_SWAP(i, j);
		*pos++ ^= S[(S[i] + S[j]) & 0xff];
	}
}

