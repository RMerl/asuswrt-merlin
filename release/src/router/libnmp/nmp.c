
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <netinet/in.h>
#include <nmp.h>


char buf[WLC_IOCTL_MAXLEN];

//static wlc_ap_list_info_t ap_list[WLC_MAX_AP_SCAN_LIST_LEN];
static char scan_result[WLC_DUMP_BUF_LEN];

static int wps_error_count = 0;

struct apinfo apinfos[32];

#define wan_prefix(unit, prefix)	snprintf(prefix, sizeof(prefix), "wan%d_", unit)

#define SSID_FMT_BUF_LEN 4*32+1	/* Length for SSID format string */

static bool g_swap = FALSE;
#define htod32(i) (g_swap?bcmswap32(i):(uint32)(i)) 
#define dtoh32(i) (g_swap?bcmswap32(i):(uint32)(i))
#define dtoh16(i) (g_swap?bcmswap16(i):(uint16)(i))
#define dtohchanspec(i) (g_swap?dtoh16(i):i)

/* +++ MD5 +++ */
typedef struct {
	unsigned long state[4];			/* state (ABCD) */
	unsigned long count[2];			/* number of bits, modulo 2^64 (lsb first) */
 	unsigned char buffer[64];		/* input buffer */
} ASUS_MD5_CTX;

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void ASUS_MD5Transform(unsigned long [4], unsigned char [64]);
static void ASUS_Encode(unsigned char *, unsigned long *, unsigned int);
static void ASUS_Decode(unsigned long *, unsigned char *, unsigned int);
static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
Rotation is separate from addition to prevent recomputation.
 */
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (unsigned long)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (unsigned long)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (unsigned long)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (unsigned long)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

/* MD5 initialization. Begins an MD5 operation, writing a new context.
 */
void ASUS_MD5Init (ASUS_MD5_CTX * context)
{
	context->count[0] = context->count[1] = 0;
	/* Load magic initialization constants.*/
	context->state[0] = 0x67452301;
	context->state[1] = 0xefcdab89;
	context->state[2] = 0x98badcfe;
	context->state[3] = 0x10325476;
}

/* MD5 block update operation. Continues an MD5 message-digest
  operation, processing another message block, and updating the
  context.
 */
void ASUS_MD5Update (ASUS_MD5_CTX *context, unsigned char *input, unsigned int inputLen)
{
	unsigned int i, index, partLen;

  	/* Compute number of bytes mod 64 */
  	index = (unsigned int)((context->count[0] >> 3) & 0x3F);

	/* Update number of bits */
	if ((context->count[0] += ((unsigned long)inputLen << 3)) < ((unsigned long)inputLen << 3))
		context->count[1]++;
  	context->count[1] += ((unsigned long)inputLen >> 29);

  	partLen = 64 - index;

  /* Transform as many times as possible. */
  	if (inputLen >= partLen) {
 		memcpy(&context->buffer[index], input, partLen);
 		ASUS_MD5Transform (context->state, context->buffer);

 		for (i = partLen; i + 63 < inputLen; i += 64)
   			ASUS_MD5Transform (context->state, &input[i]);

		index = 0;
  	}
  	else
 		i = 0;

  /* Buffer remaining input */
  	memcpy(&context->buffer[index], &input[i],inputLen-i);
}


/* MD5 finalization. Ends an MD5 message-digest operation, writing the
  the message digest and zeroizing the context.
 */
void ASUS_MD5Final (unsigned char digest[16], ASUS_MD5_CTX *context)
{
  unsigned char bits[8];
  unsigned int index, padLen;

  /* Save number of bits */
  ASUS_Encode (bits, context->count, 8);

  /* Pad out to 56 mod 64.
*/
  index = (unsigned int)((context->count[0] >> 3) & 0x3f);
  padLen = (index < 56) ? (56 - index) : (120 - index);
  ASUS_MD5Update (context, PADDING, padLen);

  /* Append length (before padding) */
  ASUS_MD5Update (context, bits, 8);

  /* Store state in digest */
  ASUS_Encode (digest, context->state, 16);

  /* Zeroize sensitive information.
*/
  memset (context, 0, sizeof (*context));
}

/* MD5 basic transformation. Transforms state based on block.
 */
static void ASUS_MD5Transform (unsigned long state[4], unsigned char block[64])
{
  unsigned long a = state[0], b = state[1], c = state[2], d = state[3], x[16];

  ASUS_Decode (x, block, 64);

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
  memset (x, 0, sizeof (x));
}

/* Encodes input (unsigned long) into output (unsigned char). Assumes len is
  a multiple of 4.
 */
static void ASUS_Encode (unsigned char *output, unsigned long *input, unsigned int len)
{
	unsigned int i, j;

	for (i = 0, j = 0; j < len; i++, j += 4) {
		output[j] = (unsigned char)(input[i] & 0xff);
		output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
		output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
		output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
	}
}

/* Decodes input (unsigned char) into output (unsigned long). Assumes len is
  a multiple of 4.
 */
static void ASUS_Decode (unsigned long *output, unsigned char *input, unsigned int len)
{
	unsigned int i, j;

	for (i = 0, j = 0; j < len; i++, j += 4)
		output[i] = ((unsigned long)input[j]) | (((unsigned long)input[j+1]) << 8) |
		(((unsigned long)input[j+2]) << 16) | (((unsigned long)input[j+3]) << 24);
}
/* --- MD5 --- */

char *nmp_get(const char *name)
{
	return nvram_get(name);
}

int nmp_get_int(const char *key)
{
	return atoi(nvram_safe_get(key));
}

int nmp_set(const char *name, const char *value)
{
	return nvram_set(name, value);
}

int nmp_set_int(const char *key, int value)
{
	char nvramstr[16];
	snprintf(nvramstr, sizeof(nvramstr), "%d", value);
	return nvram_set(key, nvramstr);
}

void nmp_commit()
{
	nvram_commit();
}

void nmp_notify_rc(const char *event_name)
{
	notify_rc(event_name);
}

/* Get channel list in the specified country */
int channels_in_country(int unit, int channels[])
{
        int i, j, retval = 0;
        char buf[4096];
        wl_channels_in_country_t *cic = (wl_channels_in_country_t *)buf;
        char tmp[256], prefix[] = "wlXXXXXXXXXX_";
        char *country_code;
        char *name;

        snprintf(prefix, sizeof(prefix), "wl%d_", unit);
        country_code = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
        name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	i = 0;
	channels[i++] = 0;
	channels[i] = -1;

        cic->buflen = sizeof(buf);
        strcpy(cic->country_abbrev, country_code);
        if (!unit)
                cic->band = WLC_BAND_2G;
        else 
                cic->band = WLC_BAND_5G;
        cic->count = 0;

        if (wl_ioctl(name, WLC_GET_CHANNELS_IN_COUNTRY, cic, cic->buflen) != 0)
                return retval;

        if (cic->count == 0)
                return retval;
        else
        {
		for (j = 0; j < cic->count; j++)
			channels[i++] = cic->channel[j];		
		channels[i] = -1;
        }

        return cic->count;
}

#define SPRINT_MAX_LEN	2560
void generateWepKey(const char* unit, unsigned char *passphrase)
{
	int  i, j, wPasswdLen, randNumber;
	unsigned char UserInputKey[127];
	unsigned char pseed[4] = {0, 0, 0, 0};
	unsigned int wep_key[4][5];
	static unsigned char string[SPRINT_MAX_LEN];
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	static unsigned char tmpstr[SPRINT_MAX_LEN];
	static long tmpval = 0;
    
	memset(string, 0, SPRINT_MAX_LEN);
    
	if(strlen(passphrase))
		strcpy(UserInputKey, passphrase);
	else
        	strcpy(UserInputKey, "");

	wPasswdLen = strlen(UserInputKey);
    
	snprintf(prefix, sizeof(prefix), "wl%s_", unit);

	if(nvram_safe_get(strcat_r(prefix, "wep_x", tmp)) != NULL)
    		tmpval = strtol(nvram_safe_get(strcat_r(prefix, "wep_x", tmp)), NULL, 10);
	
    	if( tmpval==1 ) //64-bits
    	{
		/* generate seed for random number generator using */
		/* key string... */
		for (i=0; i<wPasswdLen; i++) {
			pseed[i%4]^= UserInputKey[i];
		}

		/* init PRN generator... note that this is equivalent */
		/*  to the Microsoft srand() function. */
		randNumber =	(int)pseed[0] |
				((int)pseed[1])<<8 |
				((int)pseed[2])<<16 |
				((int)pseed[3])<<24;

		/* generate keys. */
		for (i=0; i<4; i++) {
			for (j=0; j<5; j++) {
				/* Note that these three lines are */
				/* equivalent to the Microsoft rand() */
				/* function. */
				randNumber *= 0x343fd;
				randNumber += 0x269ec3;
				wep_key[i][j] = (unsigned char)((randNumber>>16) & 0x7fff);
			}	
		}
		
		for (i=0; i<4; i++) 
		{
			memset(tmpstr, 0, SPRINT_MAX_LEN);
			sprintf(tmpstr, "%skey%d", prefix, i+1);
			tmpstr[strlen(tmpstr)] = '\0';
			sprintf(string,"%02X%02X%02X%02X%02X",wep_key[i][0],wep_key[i][1],
				wep_key[i][2],wep_key[i][3],wep_key[i][4]);	    									
			nvram_set(tmpstr,string); 
		}	
    	}//end of if( tmpval==1 )
    	else if( tmpval==2 ) //128-bits
    	{    	
		// assume 104-bit key and use MD5 for passphrase munging
		ASUS_MD5_CTX MD;
		char *cp;
		char password_buf[65];
		unsigned char key[16];
		int k;

		// Initialize MD5 structures     
		ASUS_MD5Init(&MD);

		// concatenate input passphrase repeatedly to fill password_buf
		cp = password_buf;
		for (i=0; i<64; i++)
		{
		    if(wPasswdLen)
		        *cp++ = UserInputKey[i % wPasswdLen];
		    else
		        *cp++ = UserInputKey[0];
		}

		// generate 128-bit signature using MD5
		ASUS_MD5Update(&MD, (unsigned char *)password_buf, 64);
		ASUS_MD5Final((unsigned char *)key, &MD);
		//  copy 13 bytes (104 bits) of MG5 generated signature to
		//  default key 0 (id = 1)
		k=0;
		for (i=0; i<3; i++)
		{
	    		for (j=0; j<5; j++)
				    wep_key[i][j] = key[k++];
		}
				    
		for (i=0; i<4; i++) 
		{
			memset(tmpstr, 0, SPRINT_MAX_LEN);
			sprintf(tmpstr, "%skey%d", prefix, i+1);
			tmpstr[strlen(tmpstr)] = '\0';
			sprintf(string,"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
					wep_key[0][0],wep_key[0][1],wep_key[0][2],wep_key[0][3],
					wep_key[0][4],wep_key[1][0],wep_key[1][1],wep_key[1][2],
					wep_key[1][3],wep_key[1][4],wep_key[2][0],wep_key[2][1],
					wep_key[2][2]);	    									
			nvram_set(tmpstr,string); 
		}
    	}//end of else if( tmpval==2 )
}

void mssid_generateWepKey(int unit, int subunit, unsigned char *passphrase)
{
	int  i, j, wPasswdLen, randNumber;
	unsigned char UserInputKey[127];
//	char ctmp[3];
	unsigned char pseed[4]={0,0,0,0};
	unsigned int wep_key[4][5];
	static unsigned char string[SPRINT_MAX_LEN];
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	static unsigned char tmpstr[SPRINT_MAX_LEN];
	static long tmpval = 0;
    
    	memset(string, 0, SPRINT_MAX_LEN);
   
    	if(strlen(passphrase))
        	strcpy(UserInputKey,passphrase);
    	else
        	strcpy(UserInputKey, "");

    	wPasswdLen = strlen(UserInputKey);

	snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);

	if(nvram_safe_get(strcat_r(prefix, "wep_x", tmp)) != NULL)
    		tmpval = strtol(nvram_safe_get(strcat_r(prefix, "wep_x", tmp)), NULL, 10);
	
    	if( tmpval==1 ) //64-bits
    	{
		/* generate seed for random number generator using */
		/* key string... */
		for (i=0; i<wPasswdLen; i++) {
			pseed[i%4]^= UserInputKey[i];
		}

		/* init PRN generator... note that this is equivalent */
		/*  to the Microsoft srand() function. */
		randNumber =	(int)pseed[0] |
				((int)pseed[1])<<8 |
				((int)pseed[2])<<16 |
				((int)pseed[3])<<24;

		/* generate keys. */
		for (i=0; i<4; i++) {
			for (j=0; j<5; j++) {
				/* Note that these three lines are */
				/* equivalent to the Microsoft rand() */
				/* function. */
				randNumber *= 0x343fd;
				randNumber += 0x269ec3;
				wep_key[i][j] = (unsigned char)((randNumber>>16) & 0x7fff);
			}	
		}
		
		for (i=0; i<4; i++) 
		{
			memset(tmpstr, 0, SPRINT_MAX_LEN);
			//sprintf(tmpstr, "mssid_key%d_x%d", i+1, ssid_index);
			sprintf(tmpstr, "%skey%d", prefix, i+1);
			tmpstr[strlen(tmpstr)] = '\0';
			sprintf(string,"%02X%02X%02X%02X%02X",wep_key[i][0],wep_key[i][1],
				wep_key[i][2],wep_key[i][3],wep_key[i][4]);	    									
			nvram_set(tmpstr,string); 
		}	
    	}//end of if( tmpval==1 )
    	else if( tmpval==2 ) //128-bits
    	{    	
        	// assume 104-bit key and use MD5 for passphrase munging
        	ASUS_MD5_CTX MD;
        	char			*cp;
        	char			password_buf[65];
        	unsigned char 	key[16];
        	int 			k;

        	// Initialize MD5 structures     
		ASUS_MD5Init(&MD);

        	// concatenate input passphrase repeatedly to fill password_buf
        	cp = password_buf;
        	for (i=0; i<64; i++)
        	{
            		if(wPasswdLen)
                		*cp++ = UserInputKey[i % wPasswdLen];
            		else
                		*cp++ = UserInputKey[0];
        	}

        	// generate 128-bit signature using MD5
        	ASUS_MD5Update(&MD, (unsigned char *)password_buf, 64);
        	ASUS_MD5Final((unsigned char *)key, &MD);
        	//  copy 13 bytes (104 bits) of MG5 generated signature to
        	//  default key 0 (id = 1)
        	k=0;
        	for (i=0; i<3; i++)
    			for (j=0; j<5; j++)
			    wep_key[i][j] = key[k++];
			    
		for (i=0; i<4; i++) 
		{
			memset(tmpstr, 0, SPRINT_MAX_LEN);
			//sprintf(tmpstr, "mssid_key%d_x%d", i+1, ssid_index);
			sprintf(tmpstr, "%skey%d", prefix, i+1);
			tmpstr[strlen(tmpstr)] = '\0';
			sprintf(string,"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
							wep_key[0][0],wep_key[0][1],wep_key[0][2],wep_key[0][3],
							wep_key[0][4],wep_key[1][0],wep_key[1][1],wep_key[1][2],
							wep_key[1][3],wep_key[1][4],wep_key[2][0],wep_key[2][1],
							wep_key[2][2]);	    									
			nvram_set(tmpstr,string); 
		}				    
		    
    	}//end of else if( tmpval==2 )
}

int rsn_selector_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, RSN_CIPHER_SUITE_NONE, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_NONE_;
	if (memcmp(s, RSN_CIPHER_SUITE_WEP40, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP40_;
	if (memcmp(s, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_TKIP_;
	if (memcmp(s, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_CCMP_;
	if (memcmp(s, RSN_CIPHER_SUITE_WEP104, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP104_;
	return 0;
}

int rsn_key_mgmt_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X, RSN_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X2_;
	if (memcmp(s, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X, RSN_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK2_;
	return 0;
}

int wpa_selector_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, WPA_CIPHER_SUITE_NONE, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_NONE_;
	if (memcmp(s, WPA_CIPHER_SUITE_WEP40, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP40_;
	if (memcmp(s, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_TKIP_;
	if (memcmp(s, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_CCMP_;
	if (memcmp(s, WPA_CIPHER_SUITE_WEP104, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP104_;
	return 0;
}

int wpa_key_mgmt_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X, WPA_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X_;
	if (memcmp(s, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X, WPA_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK_;
	if (memcmp(s, WPA_AUTH_KEY_MGMT_NONE, WPA_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_WPA_NONE_;
	return 0;
}

int wpa_parse_wpa_ie_rsn(const unsigned char *rsn_ie, size_t rsn_ie_len, struct wpa_ie_data *data)
{
	const struct rsn_ie_hdr *hdr;
	const unsigned char *pos;
	int left;
	int i, count;

	data->proto = WPA_PROTO_RSN_;
	data->pairwise_cipher = WPA_CIPHER_CCMP_;
	data->group_cipher = WPA_CIPHER_CCMP_;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X2_;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;

	if (rsn_ie_len == 0) {
		/* No RSN IE - fail silently */
		return -1;
	}

	if (rsn_ie_len < sizeof(struct rsn_ie_hdr)) {
//		fprintf(stderr, "ie len too short %lu", (unsigned long) rsn_ie_len);
		return -1;
	}

	hdr = (const struct rsn_ie_hdr *) rsn_ie;

	if (hdr->elem_id != DOT11_MNG_RSN_ID ||
	    hdr->len != rsn_ie_len - 2 ||
	    WPA_GET_LE16(hdr->version) != RSN_VERSION_) {
//		fprintf(stderr, "malformed ie or unknown version");
		return -1;
	}

	pos = (const unsigned char *) (hdr + 1);
	left = rsn_ie_len - sizeof(*hdr);

	if (left >= RSN_SELECTOR_LEN) {
		data->group_cipher = rsn_selector_to_bitfield(pos);
		pos += RSN_SELECTOR_LEN;
		left -= RSN_SELECTOR_LEN;
	} else if (left > 0) {
//		fprintf(stderr, "ie length mismatch, %u too much", left);
		return -1;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (pairwise), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= rsn_selector_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for key mgmt)");
		return -1;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (key mgmt), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= rsn_key_mgmt_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for capabilities)");
		return -1;
	}

	if (left >= 2) {
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left >= 2) {
		data->num_pmkid = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (left < data->num_pmkid * PMKID_LEN) {
//			fprintf(stderr, "PMKID underflow "
//				   "(num_pmkid=%d left=%d)", data->num_pmkid, left);
			data->num_pmkid = 0;
		} else {
			data->pmkid = pos;
			pos += data->num_pmkid * PMKID_LEN;
			left -= data->num_pmkid * PMKID_LEN;
		}
	}

	if (left > 0) {
//		fprintf(stderr, "ie has %u trailing bytes - ignored", left);
	}

	return 0;
}

int wpa_parse_wpa_ie_wpa(const unsigned char *wpa_ie, size_t wpa_ie_len, struct wpa_ie_data *data)
{
	const struct wpa_ie_hdr *hdr;
	const unsigned char *pos;
	int left;
	int i, count;

	data->proto = WPA_PROTO_WPA_;
	data->pairwise_cipher = WPA_CIPHER_TKIP_;
	data->group_cipher = WPA_CIPHER_TKIP_;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X_;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;

	if (wpa_ie_len == 0) {
		/* No WPA IE - fail silently */
		return -1;
	}

	if (wpa_ie_len < sizeof(struct wpa_ie_hdr)) {
//		fprintf(stderr, "ie len too short %lu", (unsigned long) wpa_ie_len);
		return -1;
	}

	hdr = (const struct wpa_ie_hdr *) wpa_ie;

	if (hdr->elem_id != DOT11_MNG_WPA_ID ||
	    hdr->len != wpa_ie_len - 2 ||
	    memcmp(&hdr->oui, WPA_OUI_TYPE_ARR, WPA_SELECTOR_LEN) != 0 ||
	    WPA_GET_LE16(hdr->version) != WPA_VERSION_) {
//		fprintf(stderr, "malformed ie or unknown version");
		return -1;
	}

	pos = (const unsigned char *) (hdr + 1);
	left = wpa_ie_len - sizeof(*hdr);

	if (left >= WPA_SELECTOR_LEN) {
		data->group_cipher = wpa_selector_to_bitfield(pos);
		pos += WPA_SELECTOR_LEN;
		left -= WPA_SELECTOR_LEN;
	} else if (left > 0) {
//		fprintf(stderr, "ie length mismatch, %u too much", left);
		return -1;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (pairwise), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= wpa_selector_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for key mgmt)");
		return -1;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (key mgmt), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= wpa_key_mgmt_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for capabilities)");
		return -1;
	}

	if (left >= 2) {
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left > 0) {
//		fprintf(stderr, "ie has %u trailing bytes", left);
		return -1;
	}

	return 0;
}

int wpa_parse_wpa_ie(const unsigned char *wpa_ie, size_t wpa_ie_len,
		     struct wpa_ie_data *data)
{
	if (wpa_ie_len >= 1 && wpa_ie[0] == DOT11_MNG_RSN_ID)
		return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
	else
		return wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, data);
}

static char *wl_get_scan_results(char *ifname)
{
	int ret, retry_times = 0;
	wl_scan_params_t *params;
	wl_scan_results_t *list = (wl_scan_results_t*)scan_result;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + NUMCHANS * sizeof(uint16);
	int org_scan_time = 20, scan_time = 40;

	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL) {
		return NULL;
	}

	memset(params, 0, params_size);
	params->bss_type = DOT11_BSSTYPE_ANY;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->scan_type = -1;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	/* extend scan channel time to get more AP probe resp */
	wl_ioctl(ifname, WLC_GET_SCAN_CHANNEL_TIME, &org_scan_time, sizeof(org_scan_time));
	if (org_scan_time < scan_time)
		wl_ioctl(ifname, WLC_SET_SCAN_CHANNEL_TIME, &scan_time,	sizeof(scan_time));

retry:
	ret = wl_ioctl(ifname, WLC_SCAN, params, params_size);
	if (ret < 0) {
		if (retry_times++ < WLC_SCAN_RETRY_TIMES) {
			printf("set scan command failed, retry %d\n", retry_times);
			sleep(1);
			goto retry;
		}
	}

	sleep(2);

	list->buflen = WLC_DUMP_BUF_LEN;
	ret = wl_ioctl(ifname, WLC_SCAN_RESULTS, scan_result, WLC_DUMP_BUF_LEN);
	if (ret < 0 && retry_times++ < WLC_SCAN_RETRY_TIMES) {
		printf("get scan result failed, retry %d\n", retry_times);
		sleep(1);
		goto retry;
	}

	free(params);

	/* restore original scan channel time */
	wl_ioctl(ifname, WLC_SET_SCAN_CHANNEL_TIME, &org_scan_time, sizeof(org_scan_time));

	if (ret < 0)
		return NULL;

	return scan_result;
}

int wl_scan(wlc_ap_list_info_t *ap_list, int unit)
{
	char *name = NULL;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	wl_scan_results_t *list = (wl_scan_results_t*)scan_result;
	wl_bss_info_t *bi;
	wl_bss_info_107_t *old_bi;
	uint i, ap_count = 0;
	//unsigned char *bssidp;
	//char ssid_str[128];
	//char macstr[18];
	//int retval = 0;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if (wl_get_scan_results(name) == NULL)
		return 0;

	//memset(ap_list, 0, sizeof(ap_list));
	if (list->count == 0)
		return 0;
	else if (list->version != WL_BSS_INFO_VERSION &&
			list->version != LEGACY_WL_BSS_INFO_VERSION) {
		/* fprintf(stderr, "Sorry, your driver has bss_info_version %d "
		    "but this program supports only version %d.\n",
		    list->version, WL_BSS_INFO_VERSION); */
		return 0;
	}

	bi = list->bss_info;
	for (i = 0; i < list->count; i++) {
	/* Convert version 107 to 108 */
		if (bi->version == LEGACY_WL_BSS_INFO_VERSION) {
			old_bi = (wl_bss_info_107_t *)bi;
			bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
			bi->ie_length = old_bi->ie_length;
			bi->ie_offset = sizeof(wl_bss_info_107_t);
		}    
		if (bi->ie_length) {
			if (ap_count < WLC_MAX_AP_SCAN_LIST_LEN){
#if 0
				ap_list[ap_count].used = TRUE;
#endif
				memcpy(ap_list[ap_count].BSSID, (uint8 *)&bi->BSSID, 6);
				strncpy((char *)ap_list[ap_count].ssid, (char *)bi->SSID, bi->SSID_len);
				ap_list[ap_count].ssid[bi->SSID_len] = '\0';
#if 0
				ap_list[ap_count].ssidLen= bi->SSID_len;
				ap_list[ap_count].ie_buf = (uint8 *)(((uint8 *)bi) + bi->ie_offset);
				ap_list[ap_count].ie_buflen = bi->ie_length;
#endif
				ap_list[ap_count].channel = (uint8)(bi->chanspec & WL_CHANSPEC_CHAN_MASK);
#if 0
				ap_list[ap_count].wep = bi->capability & DOT11_CAP_PRIVACY;
#endif
				ap_count++;
			}
		}
		bi = (wl_bss_info_t*)((int8*)bi + bi->length);
	}

	return ap_count;
}

char *wlc_nvname(char *keyword)
{
	return(wl_nvname(keyword, nvram_get_int("wlc_band"), -1));
}

int wlcscan_core(ap_info_t *ap_info, int unit)
{
	int ret, i, k, left;//, ht_extcha;
	int ap_count = 0, idx_same = -1, count = 0;
	unsigned char *bssidp;
	char *info_b;
	unsigned char rate;
	unsigned char bssid[6];
	char macstr[18];
	char ure_mac[18];
	//char ssid_str[256];
	wl_scan_results_t *result;
	wl_bss_info_t *info;
	wl_bss_info_107_t *old_info;
	struct bss_ie_hdr *ie;
	NDIS_802_11_NETWORK_TYPE NetWorkType;
	struct maclist *authorized;
	int maclist_size;
	int max_sta_count = 128;
	int wl_authorized = 0;
	wl_scan_params_t *params;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + NUMCHANS * sizeof(uint16);
	char *wif = NULL;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
//	FILE *fp;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	wif = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL)
		return ap_count;

	memset(params, 0, params_size);
	params->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
//	params->scan_type = -1;
	params->scan_type = DOT11_SCANTYPE_ACTIVE;
//	params->scan_type = DOT11_SCANTYPE_PASSIVE;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	while ((ret = wl_ioctl(wif, WLC_SCAN, params, params_size)) < 0 &&
				count++ < 2){
		//dbg("[rc] set scan command failed, retry %d\n", count);
		sleep(1);
	}

	free(params);

	nvram_set("ap_selecting", "1");
	//dbg("[rc] Please wait 4 seconds ");
	//sleep(1);
	//dbg(".");
	//sleep(1);
	//dbg(".");
	//sleep(1);
	//dbg(".");
	//sleep(1);
	//dbg(".\n\n");
	//nvram_set("ap_selecting", "0");
	sleep(4);

	if (ret == 0){
		count = 0;

		result = (wl_scan_results_t *)buf;
		result->buflen = WLC_IOCTL_MAXLEN - sizeof(result);

		while ((ret = wl_ioctl(wif, WLC_SCAN_RESULTS, result, WLC_IOCTL_MAXLEN)) < 0 && count++ < 2)
		{
			//dbg("[rc] set scan results command failed, retry %d\n", count);
			sleep(1);
		}

		if (ret == 0)
		{
			info = &(result->bss_info[0]);

			/* Convert version 107 to 109 */
			if (dtoh32(info->version) == LEGACY_WL_BSS_INFO_VERSION) {
				old_info = (wl_bss_info_107_t *)info;
				info->chanspec = CH20MHZ_CHSPEC(old_info->channel);
				info->ie_length = old_info->ie_length;
				info->ie_offset = sizeof(wl_bss_info_107_t);
			}

			info_b = (unsigned char *)info;

			for(i = 0; i < result->count; i++)
			{
				if (info->SSID_len > 32/* || info->SSID_len == 0*/)
					goto next_info;
				bssidp = (unsigned char *)&info->BSSID;
				sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X",
										(unsigned char)bssidp[0],
										(unsigned char)bssidp[1],
										(unsigned char)bssidp[2],
										(unsigned char)bssidp[3],
										(unsigned char)bssidp[4],
										(unsigned char)bssidp[5]);

				idx_same = -1;
				for (k = 0; k < ap_count; k++){
					/* deal with old version of Broadcom Multiple SSID
						(share the same BSSID) */
					if(strcmp(apinfos[k].BSSID, macstr) == 0 &&
						strcmp(apinfos[k].SSID, info->SSID) == 0){
						idx_same = k;
						break;
					}
				}

				if (idx_same != -1)
				{
					if (info->RSSI >= -50)
						apinfos[idx_same].RSSI_Quality = 100;
					else if (info->RSSI >= -80)	// between -50 ~ -80dbm
						apinfos[idx_same].RSSI_Quality = (int)(24 + ((info->RSSI + 80) * 26)/10);
					else if (info->RSSI >= -90)	// between -80 ~ -90dbm
						apinfos[idx_same].RSSI_Quality = (int)(((info->RSSI + 90) * 26)/10);
					else					// < -84 dbm
						apinfos[idx_same].RSSI_Quality = 0;
				}
				else
				{
					strcpy(apinfos[ap_count].BSSID, macstr);
//					strcpy(apinfos[ap_count].SSID, info->SSID);
					memset(apinfos[ap_count].SSID, 0x0, 33);
					memcpy(apinfos[ap_count].SSID, info->SSID, info->SSID_len);
					apinfos[ap_count].channel = (uint8)(info->chanspec & WL_CHANSPEC_CHAN_MASK);
					if ( info->ctl_ch == 0 )
					{
						apinfos[ap_count].ctl_ch = apinfos[ap_count].channel;
					}else
					{
						apinfos[ap_count].ctl_ch = info->ctl_ch;
					}

					if (info->RSSI >= -50)
						apinfos[ap_count].RSSI_Quality = 100;
					else if (info->RSSI >= -80)	// between -50 ~ -80dbm
						apinfos[ap_count].RSSI_Quality = (int)(24 + ((info->RSSI + 80) * 26)/10);
					else if (info->RSSI >= -90)	// between -80 ~ -90dbm
						apinfos[ap_count].RSSI_Quality = (int)(((info->RSSI + 90) * 26)/10);
					else					// < -84 dbm
						apinfos[ap_count].RSSI_Quality = 0;

					if ((info->capability & 0x10) == 0x10)
						apinfos[ap_count].wep = 1;
					else
						apinfos[ap_count].wep = 0;
					apinfos[ap_count].wpa = 0;

/*
					unsigned char *RATESET = &info->rateset;
					for (k = 0; k < 18; k++)
						dbg("%02x ", (unsigned char)RATESET[k]);
					dbg("\n");
*/

					NetWorkType = Ndis802_11DS;
					if ((uint8)(info->chanspec & WL_CHANSPEC_CHAN_MASK) <= 14)
					{
						for (k = 0; k < info->rateset.count; k++)
						{
							rate = info->rateset.rates[k] & 0x7f;	// Mask out basic rate set bit
							if ((rate == 2) || (rate == 4) || (rate == 11) || (rate == 22))
								continue;
							else
							{
								NetWorkType = Ndis802_11OFDM24;
								break;
							}
						}
					}
					else
						NetWorkType = Ndis802_11OFDM5;

					if (info->n_cap)
					{
						if (NetWorkType == Ndis802_11OFDM5)
							NetWorkType = Ndis802_11OFDM5_N;
						else
							NetWorkType = Ndis802_11OFDM24_N;
					}

					apinfos[ap_count].NetworkType = NetWorkType;

					ap_count++;
				}

				ie = (struct bss_ie_hdr *) ((unsigned char *) info + sizeof(*info));
				for (left = info->ie_length; left > 0; // look for RSN IE first
					left -= (ie->len + 2), ie = (struct bss_ie_hdr *) ((unsigned char *) ie + 2 + ie->len)) 
				{
					if (ie->elem_id != DOT11_MNG_RSN_ID)
						continue;

					if (wpa_parse_wpa_ie(&ie->elem_id, ie->len + 2, &apinfos[ap_count - 1].wid) == 0)
					{
						apinfos[ap_count-1].wpa = 1;
						goto next_info;
					}
				}

				ie = (struct bss_ie_hdr *) ((unsigned char *) info + sizeof(*info));
				for (left = info->ie_length; left > 0; // then look for WPA IE
					left -= (ie->len + 2), ie = (struct bss_ie_hdr *) ((unsigned char *) ie + 2 + ie->len)) 
				{
					if (ie->elem_id != DOT11_MNG_WPA_ID)
						continue;

					if (wpa_parse_wpa_ie(&ie->elem_id, ie->len + 2, &apinfos[ap_count-1].wid) == 0)
					{
						apinfos[ap_count-1].wpa = 1;
						break;
					}
				}

next_info:
				info = (wl_bss_info_t *) ((unsigned char *) info + info->length);
			}
		}
	}

#if 0
	/* Print scanning result to console */
	if (ap_count == 0){
		dbg("[wlc] No AP found!\n");
	}else{
		dbg("%-4s%4s%-33s%-18s%-9s%-16s%-9s%8s%3s%3s\n",
				"idx", "CH ", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode", "CC", "EC");
		for (k = 0; k < ap_count; k++)
		{
			dbg("%2d. ", k + 1);
			dbg("%3d ", apinfos[k].ctl_ch);
			dbg("%-33s", apinfos[k].SSID);
			dbg("%-18s", apinfos[k].BSSID);

			if (apinfos[k].wpa == 1)
				dbg("%-9s%-16s", wpa_cipher_txt(apinfos[k].wid.pairwise_cipher), wpa_key_mgmt_txt(apinfos[k].wid.key_mgmt, apinfos[k].wid.proto));
			else if (apinfos[k].wep == 1)
				dbg("WEP      Unknown         ");
			else
				dbg("NONE     Open System     ");
			dbg("%9d ", apinfos[k].RSSI_Quality);

			if (apinfos[k].NetworkType == Ndis802_11FH || apinfos[k].NetworkType == Ndis802_11DS)
				dbg("%-7s", "11b");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5)
				dbg("%-7s", "11a");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5_N)
				dbg("%-7s", "11a/n");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24)
				dbg("%-7s", "11b/g");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24_N)
				dbg("%-7s", "11b/g/n");
			else
				dbg("%-7s", "unknown");

			dbg("%3d", apinfos[k].ctl_ch);

			if (	((apinfos[k].NetworkType == Ndis802_11OFDM5_N) ||
				 (apinfos[k].NetworkType == Ndis802_11OFDM24_N)) &&
					(apinfos[k].channel != apinfos[k].ctl_ch)){
				if (apinfos[k].ctl_ch < apinfos[k].channel)
					ht_extcha = 1;
				else
					ht_extcha = 0;

				dbg("%3d", ht_extcha);
			}

			dbg("\n");
		}
	}
#endif

	ret = wl_ioctl(wif, WLC_GET_BSSID, bssid, sizeof(bssid));
	memset(ure_mac, 0x0, 18);
	if (!ret){
		if ( !(!bssid[0] && !bssid[1] && !bssid[2] && !bssid[3] && !bssid[4] && !bssid[5]) ){
			sprintf(ure_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
										(unsigned char)bssid[0],
										(unsigned char)bssid[1],
										(unsigned char)bssid[2],
										(unsigned char)bssid[3],
										(unsigned char)bssid[4],
										(unsigned char)bssid[5]);
		}
	}

	if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
		maclist_size = sizeof(authorized->count) + max_sta_count * sizeof(struct ether_addr);
		authorized = malloc(maclist_size);

		// query wl for authorized sta list
		strcpy((char*)authorized, "autho_sta_list");
		if (!wl_ioctl(wif, WLC_GET_VAR, authorized, maclist_size)){
			if (authorized->count > 0) wl_authorized = 1;
		}

		if (authorized) free(authorized);
	}

#if 0
	/* Print scanning result to web format */
	if (ap_count > 0){
		/* write pid */
		spinlock_lock(SPINLOCK_SiteSurvey);
		if ((fp = fopen(ofile, "a")) == NULL){
			printf("[wlcscan] Output %s error\n", ofile);
		}else{
			for (i = 0; i < ap_count; i++){
				if(apinfos[i].ctl_ch < 0 ){
					fprintf(fp, "\"ERR_BNAD\",");
				}else if( apinfos[i].ctl_ch > 0 &&
							 apinfos[i].ctl_ch < 14){
					fprintf(fp, "\"2G\",");
				}else if( apinfos[i].ctl_ch > 14 &&
							 apinfos[i].ctl_ch < 166){
					fprintf(fp, "\"5G\",");
				}else{
					fprintf(fp, "\"ERR_BNAD\",");
				}

				if (strlen(apinfos[i].SSID) == 0){
					fprintf(fp, "\"\",");
				}else{
					memset(ssid_str, 0, sizeof(ssid_str));
					char_to_ascii(ssid_str, apinfos[i].SSID);
					fprintf(fp, "\"%s\",", ssid_str);
				}

				fprintf(fp, "\"%d\",", apinfos[i].ctl_ch);

				if (apinfos[i].wpa == 1){
					if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_)
						fprintf(fp, "\"%s\",", "WPA");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X2_)
						fprintf(fp, "\"%s\",", "WPA2");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK_)
						fprintf(fp, "\"%s\",", "WPA-PSK");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK2_)
						fprintf(fp, "\"%s\",", "WPA2-PSK");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_NONE_)
						fprintf(fp, "\"%s\",", "NONE");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA_)
						fprintf(fp, "\"%s\",", "IEEE 802.1X");
					else
						fprintf(fp, "\"%s\",", "Unknown");
				}else if (apinfos[i].wep == 1){
					fprintf(fp, "\"%s\",", "Unknown");
				}else{
					fprintf(fp, "\"%s\",", "Open System");
				}

				if (apinfos[i].wpa == 1){
					if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_NONE_)
						fprintf(fp, "\"%s\",", "NONE");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_WEP40_)
						fprintf(fp, "\"%s\",", "WEP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_WEP104_)
						fprintf(fp, "\"%s\",", "WEP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_TKIP_)
						fprintf(fp, "\"%s\",", "TKIP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_CCMP_)
						fprintf(fp, "\"%s\",", "AES");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_TKIP_|WPA_CIPHER_CCMP_)
						fprintf(fp, "\"%s\",", "TKIP+AES");
					else
						fprintf(fp, "\"%s\",", "Unknown");
				}else if (apinfos[i].wep == 1){
					fprintf(fp, "\"%s\",", "WEP");
				}else{
					fprintf(fp, "\"%s\",", "NONE");
				}

				fprintf(fp, "\"%d\",", apinfos[i].RSSI_Quality);
				fprintf(fp, "\"%s\",", apinfos[i].BSSID);

				if (apinfos[i].NetworkType == Ndis802_11FH || apinfos[i].NetworkType == Ndis802_11DS)
					fprintf(fp, "\"%s\",", "b");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM5)
					fprintf(fp, "\"%s\",", "a");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM5_N)
					fprintf(fp, "\"%s\",", "an");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM24)
					fprintf(fp, "\"%s\",", "bg");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM24_N)
					fprintf(fp, "\"%s\",", "bgn");
				else
					fprintf(fp, "\"%s\",", "");

				if (strcmp(nvram_safe_get(wlc_nvname("ssid")), apinfos[i].SSID)){
					if (strcmp(apinfos[i].SSID, ""))
						fprintf(fp, "\"%s\"", "0");				// none
					else if (!strcmp(ure_mac, apinfos[i].BSSID)){
						// hidden AP (null SSID)
						if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
							if (wl_authorized){
								// in profile, connected
								fprintf(fp, "\"%s\"", "4");
							}else{
								// in profile, connecting
								fprintf(fp, "\"%s\"", "5");
							}
						}else{
							// in profile, connected
							fprintf(fp, "\"%s\"", "4");
						}
					}else{
						// hidden AP (null SSID)
						fprintf(fp, "\"%s\"", "0");				// none
					}
				}else if (!strcmp(nvram_safe_get(wlc_nvname("ssid")), apinfos[i].SSID)){
					if (!strlen(ure_mac)){
						// in profile, disconnected
						fprintf(fp, "\"%s\",", "1");
					}else if (!strcmp(ure_mac, apinfos[i].BSSID)){
						if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
							if (wl_authorized){
								// in profile, connected
								fprintf(fp, "\"%s\"", "2");
							}else{
								// in profile, connecting
								fprintf(fp, "\"%s\"", "3");
							}
						}else{
							// in profile, connected
							fprintf(fp, "\"%s\"", "2");
						}
					}else{
						fprintf(fp, "\"%s\"", "0");				// impossible...
					}
				}else{
					// wl0_ssid is empty
					fprintf(fp, "\"%s\"", "0");
				}

				if (i == ap_count - 1){
					fprintf(fp, "\n");
				}else{
					fprintf(fp, "\n");
				}
			}	/* for */
			fclose(fp);
		}
		spinlock_unlock(SPINLOCK_SiteSurvey);
	}	/* if */
#endif

	/* Convert apinfos to ap_info */
	if (ap_count > 0){
		/* write pid */
		spinlock_lock(SPINLOCK_SiteSurvey);
		//if ((fp = fopen(ofile, "a")) == NULL){
		//	printf("[wlcscan] Output %s error\n", ofile);
		//}else{
			for (i = 0; i < ap_count; i++){
				if(apinfos[i].ctl_ch < 0 ){
					//fprintf(fp, "\"ERR_BNAD\",");
					sprintf(ap_info[i].band, "ERR_BNAD");
				}else if( apinfos[i].ctl_ch > 0 &&
							 apinfos[i].ctl_ch < 14){
					//fprintf(fp, "\"2G\",");
					sprintf(ap_info[i].band, "2G");
				}else if( apinfos[i].ctl_ch > 14 &&
							 apinfos[i].ctl_ch < 166){
					//fprintf(fp, "\"5G\",");
					sprintf(ap_info[i].band, "5G");
				}else{
					//fprintf(fp, "\"ERR_BNAD\",");
					sprintf(ap_info[i].band, "ERR_BNAD");
				}

				/*if (strlen(apinfos[i].SSID) == 0){
					fprintf(fp, "\"\",");
				}else{
					memset(ssid_str, 0, sizeof(ssid_str));
					char_to_ascii(ssid_str, apinfos[i].SSID);
					fprintf(fp, "\"%s\",", ssid_str);
				}*/
				sprintf(ap_info[i].ssid, "%s", apinfos[i].SSID);

				//fprintf(fp, "\"%d\",", apinfos[i].ctl_ch);
				ap_info[i].channel = apinfos[i].ctl_ch;

				if (apinfos[i].wpa == 1){
					if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_)
						//fprintf(fp, "\"%s\",", "WPA");
						sprintf(ap_info[i].auth, "WPA");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X2_)
						//fprintf(fp, "\"%s\",", "WPA2");
						sprintf(ap_info[i].auth, "WPA2");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK_)
						//fprintf(fp, "\"%s\",", "WPA-PSK");
						sprintf(ap_info[i].auth, "WPA-PSK");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK2_)
						//fprintf(fp, "\"%s\",", "WPA2-PSK");
						sprintf(ap_info[i].auth, "WPA2-PSK");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_NONE_)
						//fprintf(fp, "\"%s\",", "NONE");
						sprintf(ap_info[i].auth, "NONE");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA_)
						//fprintf(fp, "\"%s\",", "IEEE 802.1X");
						sprintf(ap_info[i].auth, "IEEE 802.1X");
					else
						//fprintf(fp, "\"%s\",", "Unknown");
						sprintf(ap_info[i].auth, "Unknown");
				}else if (apinfos[i].wep == 1){
					//fprintf(fp, "\"%s\",", "Unknown");
					sprintf(ap_info[i].auth, "Unknown");
				}else{
					//fprintf(fp, "\"%s\",", "Open System");
					sprintf(ap_info[i].auth, "Open System");
				}

				if (apinfos[i].wpa == 1){
					if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_NONE_)
						//fprintf(fp, "\"%s\",", "NONE");
						sprintf(ap_info[i].enc, "NONE");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_WEP40_)
						//fprintf(fp, "\"%s\",", "WEP");
						sprintf(ap_info[i].enc, "WEP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_WEP104_)
						//fprintf(fp, "\"%s\",", "WEP");
						sprintf(ap_info[i].enc, "WEP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_TKIP_)
						//fprintf(fp, "\"%s\",", "TKIP");
						sprintf(ap_info[i].enc, "TKIP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_CCMP_)
						//fprintf(fp, "\"%s\",", "AES");
						sprintf(ap_info[i].enc, "AES");
					else if (apinfos[i].wid.pairwise_cipher == (WPA_CIPHER_TKIP_|WPA_CIPHER_CCMP_))
						//fprintf(fp, "\"%s\",", "TKIP+AES");
						sprintf(ap_info[i].enc, "TKIP+AES"); 
					else
						//fprintf(fp, "\"%s\",", "Unknown");
						sprintf(ap_info[i].enc, "Unknown");
				}else if (apinfos[i].wep == 1){
					//fprintf(fp, "\"%s\",", "WEP");
					sprintf(ap_info[i].enc, "WEP");
				}else{
					//fprintf(fp, "\"%s\",", "NONE");
					sprintf(ap_info[i].enc, "NONE");
				}

				//fprintf(fp, "\"%d\",", apinfos[i].RSSI_Quality);
				ap_info[i].rssi = apinfos[i].RSSI_Quality;
				//fprintf(fp, "\"%s\",", apinfos[i].BSSID);
				sprintf(ap_info[i].bssid, "%s", apinfos[i].BSSID);

				if (apinfos[i].NetworkType == Ndis802_11FH || apinfos[i].NetworkType == Ndis802_11DS)
					//fprintf(fp, "\"%s\",", "b");
					sprintf(ap_info[i].mode, "b");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM5)
					//fprintf(fp, "\"%s\",", "a");
					sprintf(ap_info[i].mode, "a");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM5_N)
					//fprintf(fp, "\"%s\",", "an");
					sprintf(ap_info[i].mode, "an");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM24)
					//fprintf(fp, "\"%s\",", "bg");
					sprintf(ap_info[i].mode, "bg");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM24_N)
					//fprintf(fp, "\"%s\",", "bgn");
					sprintf(ap_info[i].mode, "bgn");
				//else
					//fprintf(fp, "\"%s\",", "");
				//	sprintf(ap_info[i].mode, "");

				if (strcmp(nvram_safe_get(wlc_nvname("ssid")), apinfos[i].SSID)){
					if (strcmp(apinfos[i].SSID, ""))
						//fprintf(fp, "\"%s\"", "0");				// none
						ap_info[i].status = 0;
					else if (!strcmp(ure_mac, apinfos[i].BSSID)){
						// hidden AP (null SSID)
						if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
							if (wl_authorized){
								// in profile, connected
								//fprintf(fp, "\"%s\"", "4");
								ap_info[i].status = 4;
							}else{
								// in profile, connecting
								//fprintf(fp, "\"%s\"", "5");
								ap_info[i].status = 5;
							}
						}else{
							// in profile, connected
							//fprintf(fp, "\"%s\"", "4");
							ap_info[i].status = 4;
						}
					}else{
						// hidden AP (null SSID)
						//fprintf(fp, "\"%s\"", "0");				// none
						ap_info[i].status = 0;
					}
				}else if (!strcmp(nvram_safe_get(wlc_nvname("ssid")), apinfos[i].SSID)){
					if (!strlen(ure_mac)){
						// in profile, disconnected
						//fprintf(fp, "\"%s\",", "1");
						ap_info[i].status = 1;
					}else if (!strcmp(ure_mac, apinfos[i].BSSID)){
						if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
							if (wl_authorized){
								// in profile, connected
								//fprintf(fp, "\"%s\"", "2");
								ap_info[i].status = 2;
							}else{
								// in profile, connecting
								//fprintf(fp, "\"%s\"", "3");
								ap_info[i].status = 3;
							}
						}else{
							// in profile, connected
							//fprintf(fp, "\"%s\"", "2");
							ap_info[i].status = 2;
						}
					}else{
						//fprintf(fp, "\"%s\"", "0");				// impossible...
						ap_info[i].status = 0;
					}
				}else{
					// wl0_ssid is empty
					//fprintf(fp, "\"%s\"", "0");
					ap_info[i].status = 0;
				}
			}	/* for */
		//}
		spinlock_unlock(SPINLOCK_SiteSurvey);
	}	/* if */

	return ap_count;
}

int wl_wpsPincheck(char *pin_string)
{
	unsigned long PIN = strtoul(pin_string, NULL, 10);
	unsigned long int accum = 0;
	unsigned int len = strlen(pin_string);

	if (len != 4 && len != 8)
		return 0;

	if (len == 8) {
		accum += 3 * ((PIN / 10000000) % 10);
		accum += 1 * ((PIN / 1000000) % 10);
		accum += 3 * ((PIN / 100000) % 10);
		accum += 1 * ((PIN / 10000) % 10);
		accum += 3 * ((PIN / 1000) % 10);
		accum += 1 * ((PIN / 100) % 10);
		accum += 3 * ((PIN / 10) % 10);
		accum += 1 * ((PIN / 1) % 10);

		if (0 == (accum % 10))
			return 1;
	}
	else if (len == 4)
		return 1;

	return 0;
}

char *getWscStatusStr()
{
	char *status;

	status = nvram_safe_get("wps_proc_status");

	switch (atoi(status)) {
	case 1: /* WPS_ASSOCIATED */
		wps_error_count = 0;
		return "Start WPS Process";
		break;
	case 2: /* WPS_OK */
	case 7: /* WPS_MSGDONE */
		wps_error_count = 0;
		return "Success";
		break;
	case 3: /* WPS_MSG_ERR */
		if (++wps_error_count > 60)
		{
			wps_error_count = 0;
			nvram_set("wps_proc_status", "0"); 
		}
		return "Fail due to WPS message exchange error!";
		break;
	case 4: /* WPS_TIMEOUT */
		if (++wps_error_count > 60)
		{
			wps_error_count = 0;
			nvram_set("wps_proc_status", "0");
		}
		return "Fail due to WPS time out!";
		break;
	case 8: /* WPS_PBCOVERLAP */ 
		if (++wps_error_count > 60)
		{
			wps_error_count = 0;
			nvram_set("wps_proc_status", "0");
		}
		return "Fail due to WPS session overlap!";
		break;
	default:
		wps_error_count = 0;
		if(atoi(nvram_safe_get("wps_enable")))
			return "Idle";
		else
			return "Not used";
		break;
	}
}

int get_client_detail_info(client_list_info_t *client_list){
	int i, shm_client_info_id;
	void *shared_client_info=(void *) 0;

	P_CLIENT_DETAIL_INFO_TABLE p_client_info_tab;

	spinlock_lock(SPINLOCK_Networkmap);
	shm_client_info_id = shmget((key_t)1001, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
	if (shm_client_info_id == -1){
	    fprintf(stderr,"shmget failed\n");
	    spinlock_unlock(SPINLOCK_Networkmap);
	    return 0;
	}

	shared_client_info = shmat(shm_client_info_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr,"shmat failed\n");
		spinlock_unlock(SPINLOCK_Networkmap);
		return 0;
	}

	p_client_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shared_client_info;
	for(i=0; i<p_client_info_tab->ip_mac_num; i++) {
		memcpy(client_list[i].ip_addr, p_client_info_tab->ip_addr[i], 4);
		memcpy(client_list[i].mac_addr, p_client_info_tab->mac_addr[i], 6);
		memcpy(client_list[i].device_name, p_client_info_tab->device_name[i], 16); 
	}
	spinlock_unlock(SPINLOCK_Networkmap);

	return p_client_info_tab->ip_mac_num;
}

int get_dhcp_lease_info(dhcp_lease_info_t *dhcp_lease_list)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256], timestr[sizeof("999:59:59")];
	char *hwaddr, *ipaddr, *name, *next;
	unsigned int expires;
	int count = 0;

	if (!nvram_get_int("dhcp_enable_x"))
		return count;

	/* Refresh lease file to get actual expire time */
/*	nvram_set("flush_dhcp_lease", "1"); */
	killall("dnsmasq", SIGUSR2);
	sleep (1);
/*	while(nvram_match("flush_dhcp_lease", "1"))
		sleep(1); */

	/* Read leases file */
	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
		return count;

	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start from numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		hwaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if (inet_pton(AF_INET6, ipaddr, &addr6) != 0) {
			/* skip ipv6 leases, thay have no hwaddr, but client id */
			// hwaddr = next ? : "";
			continue;
		} else if (inet_pton(AF_INET, ipaddr, &addr4) == 0)
			continue;

		if (expires) {
			snprintf(timestr, sizeof(timestr), "%u:%02u:%02u",
			    expires / 3600,
			    expires % 3600 / 60,
			    expires % 60);
		}

		strcpy(dhcp_lease_list[count].expire, (expires ? timestr : "Static"));
		strcpy(dhcp_lease_list[count].mac_addr, hwaddr);
		strcpy(dhcp_lease_list[count].ip_addr, ipaddr);
		strcpy(dhcp_lease_list[count].host_name, name);
		count++;
	}
	fclose(fp);

	return count;
}

int
get_port_forwarding_info(port_forwarding_info_t *port_forwarding_list)
{
	FILE *fp;
	char *nat_argv[] = {"iptables", "-t", "nat", "-nxL", NULL};
	char line[256], tmp[256];
	char target[16], proto[16];
	char src[sizeof("255.255.255.255")];
	char dst[sizeof("255.255.255.255")];
	char *range, *host, *port, *ptr, *val;
	int count = 0;

	/* dump nat table including VSERVER and VUPNP chains */
	_eval(nat_argv, ">/tmp/vserver.log", 10, NULL);

	fp = fopen("/tmp/vserver.log", "r");
	if (fp == NULL)
		return count;

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		tmp[0] = '\0';
		if (sscanf(line,
		    "%15s%*[ \t]"		// target
		    "%15s%*[ \t]"		// prot
		    "%*s%*[ \t]"		// opt
		    "%15[^/]/%*d%*[ \t]"	// source
		    "%15[^/]/%*d%*[ \t]"	// destination
		    "%255[^\n]",		// options
		    target, proto, src, dst, tmp) < 4) continue;

		/* TODO: add port trigger, portmap, etc support */
		if (strcmp(target, "DNAT") != 0)
			continue;

		/* uppercase proto */
		for (ptr = proto; *ptr; ptr++)
			*ptr = toupper(*ptr);

		/* parse destination */
		if (strcmp(dst, "0.0.0.0") == 0)
			strcpy(dst, "ALL");

		/* parse options */
		port = host = range = "";
		ptr = tmp;
		while ((val = strsep(&ptr, " ")) != NULL) {
			if (strncmp(val, "dpt:", 4) == 0)
				range = val + 4;
			else if (strncmp(val, "dpts:", 5) == 0)
				range = val + 5;
			else if (strncmp(val, "to:", 3) == 0) {
				port = host = val + 3;
				strsep(&port, ":");
			}
		}

		strcpy(port_forwarding_list[count].dest, dst);
		strcpy(port_forwarding_list[count].protocol, proto);
		strcpy(port_forwarding_list[count].port_range, range);
		strcpy(port_forwarding_list[count].redirect_to, host);
		strcpy(port_forwarding_list[count].local_port, port ? : range);
		count++;
	}
	fclose(fp);
	unlink("/tmp/vserver.log");

	return count;
}


int
get_active_connection_info(active_connection_info_t *active_connection_list) /* Cherry Cho added in 2014/5/6. */
{
	FILE *fp;
	char line[256];
	char proto[16], state[16];
	char src[sizeof("255.255.255.255:65535")];
	char dst[sizeof("255.255.255.255:65535")];
	int count = 0;

	fp = fopen("/tmp/syscmd.log", "r");
	if (fp == NULL)
		return count;

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		if (sscanf(line,
		    "%15s%*[ \t]"	// proto
		    "%21s%*[ \t]"	// source
		    "%21s%*[ \t]"	// destination
		    "%16[^\n]",		// state
		    proto, src, dst, state) < 4) continue;

		if(!strcmp(proto, "Proto")) continue;

		strcpy(active_connection_list[count].protocol, proto);
		strcpy(active_connection_list[count].nated_addr, src);	
		strcpy(active_connection_list[count].dest, dst);
		strcpy(active_connection_list[count].state, state);
		count++;
	}
	fclose(fp);

	return count;
}


int get_routing_info(routing_info_t *routing_list)
{
	char buff[256];
	int  nl = 0;
	struct in_addr dest;
	struct in_addr gateway;
	struct in_addr mask;
	int flgs, ref, use, metric;
	char flags[4];
	char sdest[16], sgw[16], *iface;
	FILE *fp;
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int count = 0;

//	ret += websWrite(wp, "Destination     Gateway         Genmask         Flags Metric Ref    Use Iface\n");

	if (!(fp = fopen("/proc/net/route", "r"))) return count;

	while (fgets(buff, sizeof(buff), fp) != NULL ) 
	{
		if (nl) 
		{
			int ifl = 0;
			while (buff[ifl]!=' ' && buff[ifl]!='\t' && buff[ifl]!='\0')
				ifl++;
			buff[ifl]=0;    /* interface */
			if (sscanf(buff+ifl+1, "%x%x%d%d%d%d%x",
			   &dest.s_addr, &gateway.s_addr, &flgs, &ref, &use, &metric, &mask.s_addr)!=7) {
				//error_msg_and_die( "Unsuported kernel route format\n");
				//continue;
			}

			ifl = 0;	/* parse flags */
			if (flgs&1)
				flags[ifl++]='U';
			if (flgs&2)
				flags[ifl++]='G';
			if (flgs&4)
				flags[ifl++]='H';
			flags[ifl]=0;
			strcpy(sdest,  (dest.s_addr==0 ? "default" :
					inet_ntoa(dest)));
			strcpy(sgw,    (gateway.s_addr==0   ? "*"       :
					inet_ntoa(gateway)));

			/* Skip interfaces here */
			if (strstr(buff, "lo"))
				continue;

			/* If unknown, just expose interface name */
			iface = buff;
			//if (nvram_match("lan_ifname", buff)) /* br0, wl0, etc */
			if (!strcmp(nvram_safe_get("lan_ifname"), buff)) /* br0, wl0, etc */
				iface = "LAN";
			else
			/* Tricky, it's better to move wan_ifunit/wanx_ifunit to shared instead */
			for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
				wan_prefix(unit, prefix);
				//if (nvram_match(strcat_r(prefix, "pppoe_ifname", tmp), buff)) {
				if (!strcmp(nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp)), buff)) {
					iface = "WAN";
					break;
				}
				//if (nvram_match(strcat_r(prefix, "ifname", tmp), buff)) {
				if (!strcmp(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), buff)) {
					char *wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
					iface = (strcmp(wan_proto, "dhcp") == 0 ||
						 strcmp(wan_proto, "static") == 0 ) ? "WAN" : "MAN";
					break;
				}
			}

#if 0
			ret += websWrite(wp, "%-16s%-16s%-16s%-6s%-6d %-2d %7d %s\n",
			sdest, sgw,
			inet_ntoa(mask),
			flags, metric, ref, use, iface);
#endif

			strcpy(routing_list[count].dest, sdest);
			strcpy(routing_list[count].gateway, sgw);
			strcpy(routing_list[count].genmask, inet_ntoa(mask));
			strcpy(routing_list[count].flags, flags);
			routing_list[count].metric = metric;
			routing_list[count].ref = ref;
			routing_list[count].use = use;
			strcpy(routing_list[count].iface, iface);
			count++;
		}
		nl++;
	}
	fclose(fp);
#if 0
#ifdef RTCONFIG_IPV6
	if ((fp = fopen("/proc/net/if_inet6", "r")) == (FILE*)0)
		return ret;

	while (fgets(buf, 256, fp) != NULL)
	{
		if(strstr(buf, "br0") == (char*) 0)
			continue;

		if (sscanf(buf, "%*s %*02x %*02x %02x", &fl) != 1)
			continue;

		if ((fl & 0xF0) == 0x20)
		{
			/* Link-Local Address is ready */
			found = 1;
			break;
		}
	}
	fclose(fp);

	if (found)
		INET6_displayroutes(wp);
#endif
#endif

	return count;
}

sta_info_t *
wl_sta_info(char *ifname, struct ether_addr *ea)
{
	static char buf[sizeof(sta_info_t)];
	sta_info_t *sta = NULL;

	strcpy(buf, "sta_info");
	memcpy(buf + strlen(buf) + 1, (void *)ea, ETHER_ADDR_LEN);

	if (!wl_ioctl(ifname, WLC_GET_VAR, buf, sizeof(buf))) {
		sta = (sta_info_t *)buf;
		sta->ver = dtoh16(sta->ver);

		/* Report unrecognized version */
		if (sta->ver > WL_STA_VER) {
			return NULL;
		}

		sta->len = dtoh16(sta->len);
		sta->cap = dtoh16(sta->cap);
#ifdef RTCONFIG_BCMARM
		sta->aid = dtoh16(sta->aid);
#endif
		sta->flags = dtoh32(sta->flags);
		sta->idle = dtoh32(sta->idle);
		sta->rateset.count = dtoh32(sta->rateset.count);
		sta->in = dtoh32(sta->in);
		sta->listen_interval_inms = dtoh32(sta->listen_interval_inms);
#ifdef RTCONFIG_BCMARM
		sta->ht_capabilities = dtoh16(sta->ht_capabilities);
		sta->vht_flags = dtoh16(sta->vht_flags);
#endif
	}

	return sta;
}

int get_wireless_info(wireless_info_t *wireless_list, int unit)
{
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	char name_vif[] = "wlX.Y_XXXXXXXXXX";
	struct maclist *auth, *assoc, *authorized;
	int max_sta_count, maclist_size;
	int i, j;//, val = 0, ret = 0;
	int ii, jj;
	int count = 0;
	/* Cherry Cho added in 2014/5/6. */
	scb_val_t scb_val; 	
	/* --- */

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
#if 0
#ifdef RTCONFIG_WIRELESSREPEATER
	if ((nvram_get_int("sw_mode") == SW_MODE_REPEATER)
		&& (nvram_get_int("wlc_band") == unit))
	{
		sprintf(name_vif, "wl%d.%d", unit, 1);
		name = name_vif;
	}
	else
#endif
#endif

#if 0
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
	wl_ioctl(name, WLC_GET_RADIO, &val, sizeof(val));
	val &= WL_RADIO_SW_DISABLE | WL_RADIO_HW_DISABLE;

	
	//if (nvram_match(strcat_r(prefix, "mode", tmp), "wds")) {
	if (!strcmp(nvram_safe_get(strcat_r(prefix, "mode", tmp)), "wds")) {
		// dump static info only for wds mode:
		//ret += websWrite(wp, "Channel: %s\n", nvram_safe_get(strcat_r(prefix, "channel", tmp)));
	}
	else {
		//ret += wl_status(eid, wp, argc, argv, unit);
	}

	if (val) 
	{
		//ret += websWrite(wp, "%s radio is disabled\n",
		//	nvram_match(strcat_r(prefix, "nband", tmp), "1") ? "5 GHz" : "2.4 GHz");
		return count;
	}

	//if (nvram_match(strcat_r(prefix, "mode", tmp), "ap"))
	if (!strcmp(nvram_safe_get(strcat_r(prefix, "mode", tmp)), "ap"))
	{
		//if (nvram_match(strcat_r(prefix, "lazywds", tmp), "1") ||
		if (!strcmp(nvram_safe_get(strcat_r(prefix, "lazywds", tmp)), "1") ||
			//nvram_invmatch(strcat_r(prefix, "wds", tmp), ""))
			strcmp(nvram_safe_get(strcat_r(prefix, "wds", tmp)), ""))
		//	ret += websWrite(wp, "Mode	: Hybrid\n");
		;//else    ret += websWrite(wp, "Mode	: AP Only\n");
	}
	//else if (nvram_match(strcat_r(prefix, "mode", tmp), "wds"))
	else if (!strcmp(nvram_safe_get(strcat_r(prefix, "mode", tmp)), "wds"))
	{
		//ret += websWrite(wp, "Mode	: WDS Only\n");
		return ret;
	}
	//else if (nvram_match(strcat_r(prefix, "mode", tmp), "sta"))
	else if (!strcmp(nvram_safe_get(strcat_r(prefix, "mode", tmp)), "sta"))
	{
		//ret += websWrite(wp, "Mode	: Stations\n");
		//ret += ej_wl_sta_status(eid, wp, name);
		return ret;
	}
	//else if (nvram_match(strcat_r(prefix, "mode", tmp), "wet"))
	else if (!strcmp(nvram_safe_get(strcat_r(prefix, "mode", tmp)), "wet"))
	{
#ifdef RTCONFIG_WIRELESSREPEATER
		if ((nvram_get_int("sw_mode") == SW_MODE_REPEATER)
			&& (nvram_get_int("wlc_band") == unit))
			sprintf(prefix, "wl%d.%d_", unit, 1);
#endif		
		;//ret += websWrite(wp, "Mode	: Repeater [ SSID local: \"%s\" ]\n", nvram_safe_get(strcat_r(prefix, "ssid", tmp)));

	}
#endif

	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	/* buffers and length */
	max_sta_count = 128;
	maclist_size = sizeof(auth->count) + max_sta_count * sizeof(struct ether_addr);
	auth = malloc(maclist_size);
	assoc = malloc(maclist_size);
	authorized = malloc(maclist_size);

	if (!auth || !assoc || !authorized)
		goto exit;

	/* query wl for authenticated sta list */
	strcpy((char*)auth, "authe_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, auth, maclist_size))
		goto exit;

	/* query wl for associated sta list */
	assoc->count = max_sta_count;
	if (wl_ioctl(name, WLC_GET_ASSOCLIST, assoc, maclist_size))
		goto exit;

	/* query wl for authorized sta list */
	strcpy((char*)authorized, "autho_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, authorized, maclist_size))
		goto exit;

	//ret += websWrite(wp, "\n");
	//ret += websWrite(wp, "Stations List                           \n");
	//ret += websWrite(wp, "----------------------------------------\n");


	/* build authenticated/associated/authorized sta list */
	for (i = 0; i < auth->count; i ++) {
		char ea[ETHER_ADDR_STR_LEN];
		int client_status = 0;

		for (j = 0; j < assoc->count; j ++) {
			if (!bcmp((void *)&auth->ea[i], (void *)&assoc->ea[j], ETHER_ADDR_LEN)) {
				client_status += 1;
				break;
			}
		}

		for (j = 0; j < authorized->count; j ++) {
			if (!bcmp((void *)&auth->ea[i], (void *)&authorized->ea[j], ETHER_ADDR_LEN)) {
				client_status += 2;
				break;
			}
		}

		sprintf(wireless_list[count].mac_addr, "%s", ether_etoa((void *)&auth->ea[i], ea));
		wireless_list[count].status = client_status;

		/* Cherry Cho added in 2014/5/6. */
		memcpy(&scb_val.ea, &auth->ea[i], ETHER_ADDR_LEN);
		if (wl_ioctl(name, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t)))
			strcpy(wireless_list[count].rssi, "");
		else
			sprintf(wireless_list[count].rssi, "%4ddBm ", scb_val.val);

		sta_info_t *sta = wl_sta_info(name, &auth->ea[i]);
		if (sta && (sta->flags & WL_STA_SCBSTATS))
		{
#if 0
#ifdef RTCONFIG_BCMARM
#ifndef RTCONFIG_QTN
			ret += websWrite(wp, "%-4s%-4s%-5s",
				(sta->flags & WL_STA_PS) ? "Yes" : "No",
				((sta->ht_capabilities & WL_STA_CAP_SHORT_GI_20) || (sta->ht_capabilities & WL_STA_CAP_SHORT_GI_40)) ? "Yes" : "No",
				((sta->ht_capabilities & WL_STA_CAP_TX_STBC) || (sta->ht_capabilities & WL_STA_CAP_RX_STBC_MASK)) ? "Yes" : "No");
#endif
#else
			ret += websWrite(wp, "%-4s",
				(sta->flags & WL_STA_PS) ? "Yes" : "No");
#endif
#endif
			wireless_list[count].psm = (sta->flags & WL_STA_PS) ? 1 : 0;
			wireless_list[count].tx_rate = sta->tx_rate;
			wireless_list[count].rx_rate = sta->rx_rate;
			wireless_list[count].connect_time = sta->in;
		}		
		/* --- */
		count++;
	}

	for (i = 1; i < 4; i++) {
#ifdef RTCONFIG_WIRELESSREPEATER
		if ((nvram_get_int("sw_mode") == SW_MODE_REPEATER)
			&& (unit == nvram_get_int("wlc_band")) && (i == 1))
			break;
#endif
		sprintf(prefix, "wl%d.%d_", unit, i);
		if (!strcmp(nvram_safe_get(strcat_r(prefix, "bss_enabled", tmp)), "1"))
		{
			sprintf(name_vif, "wl%d.%d", unit, i);

			/* query wl for authenticated sta list */
			strcpy((char*)auth, "authe_sta_list");
			if (wl_ioctl(name_vif, WLC_GET_VAR, auth, maclist_size))
				goto exit;

			/* query wl for associated sta list */
			assoc->count = max_sta_count;
			if (wl_ioctl(name_vif, WLC_GET_ASSOCLIST, assoc, maclist_size))
				goto exit;

			/* query wl for authorized sta list */
			strcpy((char*)authorized, "autho_sta_list");
			if (wl_ioctl(name_vif, WLC_GET_VAR, authorized, maclist_size))
				goto exit;

			for (ii = 0; ii < auth->count; ii++) {
				char ea[ETHER_ADDR_STR_LEN];
				int client_status = 0;

				for (jj = 0; jj < assoc->count; jj++) {
					if (!bcmp((void *)&auth->ea[ii], (void *)&assoc->ea[jj], ETHER_ADDR_LEN)) {
						client_status += 1;
						break;
					}
				}

				for (jj = 0; jj < authorized->count; jj++) {
					if (!bcmp((void *)&auth->ea[ii], (void *)&authorized->ea[jj], ETHER_ADDR_LEN)) {
						client_status += 2;
						break;
					}
				}

				sprintf(wireless_list[count].mac_addr, "%s", ether_etoa((void *)&auth->ea[i], ea));
				wireless_list[count].status = client_status;
				count++;
			}
		}
	}

	/* error/exit */
exit:
	if (auth) free(auth);
	if (assoc) free(assoc);
	if (authorized) free(authorized);

	return count;
}

int
wl_format_ssid(char* ssid_buf, uint8* ssid, int ssid_len)
{
	int i, c;
	char *p = ssid_buf;

	if (ssid_len > 32) ssid_len = 32;

	for (i = 0; i < ssid_len; i++) {
		c = (int)ssid[i];
		if (c == '\\') {
			*p++ = '\\';
//			*p++ = '\\';
		} else if (isprint((uchar)c)) {
			*p++ = (char)c;
		} else {
			p += sprintf(p, "\\x%02X", c);
		}
	}
	*p = '\0';

	return p - ssid_buf;
}

int dump_rateset(char *bss_info, uint8 *rates, uint count)
{
	uint i;
	uint r;
	bool b;
	int retval = 0;
	char string[128];

	memset(string, 0, sizeof(string));
	strcat(bss_info, "[ ");
	for (i = 0; i < count; i++) {
		r = rates[i] & 0x7f;
		b = rates[i] & 0x80;
		if (r == 0)
			break;
;
		sprintf(string, "%d%s%s ", (r / 2), (r % 2)?".5":"", b?"(b)":"");
		strcat(bss_info, string);
	}

	strcat(bss_info, "]");
	return retval;
}

/* given a chanspec and a string buffer, format the chanspec as a
 * string, and return the original pointer a.
 * Min buffer length must be CHANSPEC_STR_LEN.
 * On error return NULL
 */
char *
wf_chspec_ntoa(chanspec_t chspec, char *buf)
{
	const char *band, *bw, *sb;
	uint channel;

	band = "";
	bw = "";
	sb = "";
	channel = CHSPEC_CHANNEL(chspec); 
	/* check for non-default band spec */
	if ((CHSPEC_IS2G(chspec) && channel > CH_MAX_2G_CHANNEL) ||
	    (CHSPEC_IS5G(chspec) && channel <= CH_MAX_2G_CHANNEL))
		band = (CHSPEC_IS2G(chspec)) ? "b" : "a";
	if (CHSPEC_IS40(chspec)) {
		if (CHSPEC_SB_UPPER(chspec)) { 
			sb = "u";
			channel += CH_10MHZ_APART;
		} else {
			sb = "l";
			channel -= CH_10MHZ_APART;
		}
	} else if (CHSPEC_IS10(chspec)) {
		bw = "n";
	}

	/* Outputs a max of 6 chars including '\0'  */
	snprintf(buf, 6, "%d%s%s%s", channel, band, bw, sb);
	return (buf); 
}

int dump_bss_info(char *bss_info, wl_bss_info_t *bi)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	char chspec_str[CHANSPEC_STR_LEN];
	wl_bss_info_107_t *old_bi;
	int mcs_idx = 0;
	int retval = 0;
	char string[128];
	char ea[ETHER_ADDR_STR_LEN];

	/* Convert version 107 to 109 */
	if (dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION) {
		old_bi = (wl_bss_info_107_t *)bi;
		bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
		bi->ie_length = old_bi->ie_length;
		bi->ie_offset = sizeof(wl_bss_info_107_t);
	}

	wl_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);

	memset(string, 0, sizeof(string));
	sprintf(string, "\nSSID: \"%s\", ", ssidbuf);
	strcat(bss_info, string);

	sprintf(string, "RSSI: %d dBm, ", (int16)(dtoh16(bi->RSSI)));
	strcat(bss_info, string);
	/*
	 * SNR has valid value in only 109 version.
	 * So print SNR for 109 version only.
	 */
	if (dtoh32(bi->version) == WL_BSS_INFO_VERSION) {
		sprintf(string, "SNR: %d dB, ", (int16)(dtoh16(bi->SNR)));
		strcat(bss_info, string);
	}

	sprintf(string, "noise: %d dBm, ", bi->phy_noise);
	strcat(bss_info, string);
	if (bi->flags) {
		bi->flags = dtoh16(bi->flags);
		strcat(bss_info, "Flags: ");
		if (bi->flags & WL_BSS_FLAGS_FROM_BEACON) strcat(bss_info, "FromBcn ");
		if (bi->flags & WL_BSS_FLAGS_FROM_CACHE) strcat(bss_info, "Cached ");
		if (bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) strcat(bss_info, "RSSI on-channel ");
		strcat(bss_info, ", ");
	}

	sprintf(string, "Channel: %s, ", wf_chspec_ntoa(dtohchanspec(bi->chanspec), chspec_str));
	strcat(bss_info, string);
	sprintf(string, "BSSID: %s, ", ether_etoa((void *)&bi->BSSID, ea));
	strcat(bss_info, string);

	//retval += websWrite(wp, "Capability: ");
	strcat(bss_info, "Capability: ");
	bi->capability = dtoh16(bi->capability);
	if (bi->capability & DOT11_CAP_ESS) strcat(bss_info, "ESS ");
	if (bi->capability & DOT11_CAP_IBSS) strcat(bss_info, "IBSS ");
	if (bi->capability & DOT11_CAP_POLLABLE) strcat(bss_info, "Pollable ");
	if (bi->capability & DOT11_CAP_POLL_RQ) strcat(bss_info, "PollReq ");
	if (bi->capability & DOT11_CAP_PRIVACY) strcat(bss_info, "WEP ");
	if (bi->capability & DOT11_CAP_SHORT) strcat(bss_info, "ShortPre ");
	if (bi->capability & DOT11_CAP_PBCC) strcat(bss_info, "PBCC ");
	if (bi->capability & DOT11_CAP_AGILITY) strcat(bss_info, "Agility ");
	if (bi->capability & DOT11_CAP_SHORTSLOT) strcat(bss_info, "ShortSlot ");
	if (bi->capability & DOT11_CAP_CCK_OFDM) strcat(bss_info, "CCK-OFDM ");

	strcat(bss_info, ", ");
	strcat(bss_info, "Supported Rates: ");
	dump_rateset(bss_info, bi->rateset.rates, dtoh32(bi->rateset.count));

#if 0
	if (dtoh32(bi->ie_length))
		retval += wl_dump_wpa_rsn_ies(eid, wp, argc, argv, (uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
				    dtoh32(bi->ie_length));
#endif
 
	if (dtoh32(bi->version) != LEGACY_WL_BSS_INFO_VERSION && bi->n_cap) {
		//retval += websWrite(wp, "802.11N Capable:\n");
		strcat(bss_info, ", ");
		bi->chanspec = dtohchanspec(bi->chanspec); 
		//retval += websWrite(wp, "\tChanspec: %sGHz channel %d %dMHz (0x%x)\n",
		//	CHSPEC_IS2G(bi->chanspec)?"2.4":"5", CHSPEC_CHANNEL(bi->chanspec),
		//	CHSPEC_IS40(bi->chanspec) ? 40 : (CHSPEC_IS20(bi->chanspec) ? 20 : 10),
		//	bi->chanspec);
		sprintf(string, "Chanspec: %sGHz channel %d %dMHz (0x%x), ",
			CHSPEC_IS2G(bi->chanspec)?"2.4":"5", CHSPEC_CHANNEL(bi->chanspec),
			CHSPEC_IS40(bi->chanspec) ? 40 : (CHSPEC_IS20(bi->chanspec) ? 20 : 10),
			bi->chanspec);
		strcat(bss_info, string);
		sprintf(string, "Control channel: %d, ", bi->ctl_ch);
		strcat(bss_info, string);
		strcat(bss_info, "802.11N Capabilities: ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_40MHZ)
			strcat(bss_info, "40Mhz ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_SHORT_GI_20)
			strcat(bss_info, "SGI20 ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_SHORT_GI_40)
			strcat(bss_info, "SGI40 ");

		strcat(bss_info, ", Supported MCS : [ ");
		for (mcs_idx = 0; mcs_idx < (MCSSET_LEN * 8); mcs_idx++)
			if (isset(bi->basic_mcs, mcs_idx)) {
				sprintf(string, "%d ", mcs_idx);
				strcat(bss_info, string);
			}

		strcat(bss_info, "]");
	}

	return retval;
}

int bss_status(char *bss_info, int unit)
{
	int ret;
	struct ether_addr bssid;
	wlc_ssid_t ssid;
	char ssidbuf[SSID_FMT_BUF_LEN];
	wl_bss_info_t *bi;
	int retval = 0;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	char string[256];

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	memset(string, 0, sizeof(string));
	if ((ret = wl_ioctl(name, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN)) == 0) {
		/* The adapter is associated. */
		*(uint32*)buf = htod32(WLC_IOCTL_MAXLEN);
		if ((ret = wl_ioctl(name, WLC_GET_BSS_INFO, buf, WLC_IOCTL_MAXLEN)) < 0)
			return 0; 

		bi = (wl_bss_info_t*)(buf + 4);
		if (dtoh32(bi->version) == WL_BSS_INFO_VERSION ||
		    dtoh32(bi->version) == LEGACY2_WL_BSS_INFO_VERSION ||
		    dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION)
		{
			dump_bss_info(bss_info, bi); 
			if (!strcmp(nvram_safe_get(strcat_r(prefix, "mode", tmp)), "ap"))
			{
				if (!strcmp(nvram_safe_get(strcat_r(prefix, "lazywds", tmp)), "1") ||
					strcmp(nvram_safe_get(strcat_r(prefix, "wds", tmp)), ""))
					sprintf(string, ", Mode: Hybrid\n");
				else
					sprintf(string, ", Mode: AP Only\n");
			}
			else if (!strcmp(nvram_safe_get(strcat_r(prefix, "mode", tmp)), "wds"))
			{
				sprintf(string, " ,Mode: WDS Only\n");
			}
			else if (!strcmp(nvram_safe_get(strcat_r(prefix, "mode", tmp)), "sta"))
			{
				sprintf(string, " ,Mode: Stations\n");
			}
			else if (!strcmp(nvram_safe_get(strcat_r(prefix, "mode", tmp)), "wet"))
			{
			#ifdef RTCONFIG_WIRELESSREPEATER
				if ((nvram_get_int("sw_mode") == SW_MODE_REPEATER)
					&& (nvram_get_int("wlc_band") == unit))
					sprintf(prefix, "wl%d.%d_", unit, 1);
			#endif
				sprintf(string, ", Mode: Repeater [ SSID local: \"%s\" ]\n", nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
			}
			#ifdef RTCONFIG_PROXYSTA
			else if (!strcmp(nvram_safe_get(strcat_r(prefix, "mode", tmp)), "psta"))
			{
				if ((nvram_get_int("sw_mode") == SW_MODE_AP) &&
					(nvram_get_int("wlc_psta") == 1) &&
					(nvram_get_int("wlc_band") == unit))
				sprintf(string, ", Mode: Media Bridge\n");
			}
			#endif
			strcat(bss_info, string);
		}
		else
		{
			sprintf(string, "Sorry, your driver has bss_info_version %d "
				"but this program supports only version %d.\n",
				bi->version, WL_BSS_INFO_VERSION);
			strcat(bss_info, string);
		}
	} else {
		sprintf(string, "Not associated. Last associated with ");
		strcat(bss_info, string);

		if ((ret = wl_ioctl(name, WLC_GET_SSID, &ssid, sizeof(wlc_ssid_t))) < 0) {;
			return 0;
		}

		wl_format_ssid(ssidbuf, ssid.SSID, dtoh32(ssid.SSID_len));
		sprintf(string, "SSID: \"%s\"", ssidbuf);
		strcat(bss_info, string);
	} 

	return retval;
}

char *rfctime(const time_t *timep)
{
	static char s[201];
	struct tm tm;

	//it suppose to be convert after applying
	//time_zone_x_mapping();
	setenv("TZ", nvram_safe_get("time_zone_x"), 1);
	memcpy(&tm, localtime(timep), sizeof(struct tm));
	//strftime(s, 200, "%a, %d %b %Y %H:%M:%S %z", &tm);
	strftime(s, 200, "%a, %b %d %H:%M:%S %Y", &tm);
	return s;
}

unsigned int get_qos_orates(int priority, int min_max)
{
    	int i;
    	char *buf, *g, *p;
    	unsigned int rate;
   	unsigned int ceil;
	
	g = buf = strdup(nvram_safe_get("qos_orates"));
	for (i = 0; i < priority; ++i)  // 0~4 , 0:highest, 4:lowest
		if ((!g) || ((p = strsep(&g, ",")) == NULL)) return 0;	
	if ((!g) || ((p = strsep(&g, ",")) == NULL)) return 0;
	if ((sscanf(p, "%u-%u", &rate, &ceil) != 2)) return 0;
	free(buf);
	
	if(min_max)
		return rate;
	else
		return ceil;
}

void set_qos_orates(int priority, int min_max, unsigned int value)
{
    	int i;
    	char *buf, *g, *p;
    	unsigned int rate;
   	unsigned int ceil;
	char qos_orates_str[128];

	memset(qos_orates_str, 0, sizeof(qos_orates_str));
	g = buf = strdup(nvram_safe_get("qos_orates"));
	for (i = 0; i < 10; ++i) { // 0~4 , 0:highest, 4:lowest
		char priority_str[16];
		memset(priority_str, 0, sizeof(priority_str));

		if ((!g) || ((p = strsep(&g, ",")) == NULL)) return;
		if(i == priority) {
			if ((sscanf(p, "%u-%u", &rate, &ceil) != 2)) return;
			
			if(min_max)
				(!i) ? sprintf(priority_str, "%u-%u", value, ceil) : sprintf(priority_str, ",%u-%u", value, ceil);
			else	
				(!i) ? sprintf(priority_str, "%u-%u", rate, value) : sprintf(priority_str, ",%u-%u", rate, value);
		}		
		else
			(!i) ? sprintf(priority_str, "%s", p) : sprintf(priority_str, ",%s", p);	
		strcat(qos_orates_str, priority_str);
	}
	free(buf);
	nvram_set("qos_orates", qos_orates_str);
}

unsigned int get_qos_irates(int priority)
{
    	int i;
    	char *buf, *g, *p;
    	unsigned int rate;
	
	g = buf = strdup(nvram_safe_get("qos_irates"));
	for (i = 0; i < priority; ++i)  // 0~4 , 0:highest, 4:lowest
		if ((!g) || ((p = strsep(&g, ",")) == NULL)) return 0;	
	if ((!g) || ((p = strsep(&g, ",")) == NULL)) return 0;
	rate = atoi(p);
	free(buf);
	return rate;
}

void set_qos_irates(int priority, unsigned int value)
{
    	int i;
    	char *buf, *g, *p;
    	char qos_irates_str[128];

	memset(qos_irates_str, 0, sizeof(qos_irates_str));
	
	g = buf = strdup(nvram_safe_get("qos_irates"));
	for (i = 0; i < 10; ++i) { // 0~4 , 0:highest, 4:lowest
		char priority_str[16];
		memset(priority_str, 0, sizeof(priority_str));
		if ((!g) || ((p = strsep(&g, ",")) == NULL)) return;

		if(i == priority)
			(!i) ? sprintf(priority_str, "%u", value) : sprintf(priority_str, ",%u", value);		
		else
			(!i) ? sprintf(priority_str, "%s", p) : sprintf(priority_str, ",%s", p);	
		strcat(qos_irates_str, priority_str);
	}
	free(buf);
	nvram_set("qos_irates", qos_irates_str);
}

/* For OpenVPN Server Settings. */
#include <stdarg.h>

struct buffer
alloc_buf (size_t size)
{
	struct buffer buf;

	if (!buf_size_valid (size))
		buf_size_error (size);
	buf.capacity = (int)size;
	buf.offset = 0;
	buf.len = 0;
	buf.data = calloc (1, size);

	return buf;
}

void
buf_size_error (const size_t size)
{
	//logmessage ("OVPN", "fatal buffer size error, size=%lu", (unsigned long)size);
}

bool
buf_printf (struct buffer *buf, const char *format, ...)
{
	int ret = false;
	if (buf_defined (buf))
	{
		va_list arglist;
		uint8_t *ptr = buf_bend (buf);
		int cap = buf_forward_capacity (buf);

		if (cap > 0)
		{
			int stat;
			va_start (arglist, format);
			stat = vsnprintf ((char *)ptr, cap, format, arglist);
			va_end (arglist);
			*(buf->data + buf->capacity - 1) = 0; /* windows vsnprintf needs this */
			buf->len += (int) strlen ((char *)ptr);
			if (stat >= 0 && stat < cap)
				ret = true;
		}
	}
	return ret;
}

bool
buf_parse (struct buffer *buf, const int delim, char *line, const int size)
{
	bool eol = false;
	int n = 0;
	int c;

	do
	{
		c = buf_read_u8 (buf);
		if (c < 0)
			eol = true;
		if (c <= 0 || c == delim)
			c = 0;
		if (n >= size)
			break;
		line[n++] = c;
	}
	while (c);

	line[size-1] = '\0';
	return !(eol && !strlen (line));
}

void
buf_clear (struct buffer *buf)
{
	if (buf->capacity > 0)
		memset (buf->data, 0, buf->capacity);
	buf->len = 0;
	buf->offset = 0;
}

void
free_buf (struct buffer *buf)
{
	if (buf->data)
		free (buf->data);
	CLEAR (*buf);
}

char *
string_alloc (const char *str)
{
	if (str)
	{
		const int n = strlen (str) + 1;
		char *ret;

		ret = calloc(n, 1);
		if(!ret)
			return NULL;

		memcpy (ret, str, n);
		return ret;
	}
	else
		return NULL;
}

static inline bool
space (unsigned char c)
{
	return c == '\0' || isspace (c);
}

int
parse_line (const char *line, char *p[], const int n, const int line_num)
{
	const int STATE_INITIAL = 0;
	const int STATE_READING_QUOTED_PARM = 1;
	const int STATE_READING_UNQUOTED_PARM = 2;
	const int STATE_DONE = 3;
	const int STATE_READING_SQUOTED_PARM = 4;

	int ret = 0;
	const char *c = line;
	int state = STATE_INITIAL;
	bool backslash = false;
	char in, out;

	char parm[OPTION_PARM_SIZE];
	unsigned int parm_len = 0;

	do
	{
		in = *c;
		out = 0;

		if (!backslash && in == '\\' && state != STATE_READING_SQUOTED_PARM)
		{
			backslash = true;
		}
		else
		{
			if (state == STATE_INITIAL)
			{
				if (!space (in))
				{
					if (in == ';' || in == '#') /* comment */
						break;
					if (!backslash && in == '\"')
						state = STATE_READING_QUOTED_PARM;
					else if (!backslash && in == '\'')
						state = STATE_READING_SQUOTED_PARM;
					else
					{
						out = in;
						state = STATE_READING_UNQUOTED_PARM;
					}
				}
			}
			else if (state == STATE_READING_UNQUOTED_PARM)
			{
				if (!backslash && space (in))
					state = STATE_DONE;
				else
					out = in;
			}
			else if (state == STATE_READING_QUOTED_PARM)
			{
				if (!backslash && in == '\"')
					state = STATE_DONE;
				else
					out = in;
			}
			else if (state == STATE_READING_SQUOTED_PARM)
			{
				if (in == '\'')
					state = STATE_DONE;
				else
					out = in;
			}

			if (state == STATE_DONE)
			{
				p[ret] = calloc (parm_len + 1, 1);
				memcpy (p[ret], parm, parm_len);
				p[ret][parm_len] = '\0';
				state = STATE_INITIAL;
				parm_len = 0;
				++ret;
			}

			if (backslash && out)
			{
				if (!(out == '\\' || out == '\"' || space (out)))
				{
					//logmessage ("OVPN", "Options warning: Bad backslash ('\\') usage in %d", line_num);
					return 0;
				}
			}
			backslash = false;
		}

		/* store parameter character */
		if (out)
		{
			if (parm_len >= SIZE (parm))
			{
				parm[SIZE (parm) - 1] = 0;
				//logmessage ("OVPN", "Options error: Parameter at %d is too long (%d chars max): %s",
				//	line_num, (int) SIZE (parm), parm);
				return 0;
			}
			parm[parm_len++] = out;
		}

		/* avoid overflow if too many parms in one config file line */
		if (ret >= n)
			break;

	} while (*c++ != '\0');


	if (state == STATE_READING_QUOTED_PARM)
	{
		//logmessage ("OVPN", "Options error: No closing quotation (\") in %d", line_num);
		return 0;
	}
	if (state == STATE_READING_SQUOTED_PARM)
	{
		//logmessage ("OVPN", "Options error: No closing single quotation (\') in %d", line_num);
		return 0;
	}
	if (state != STATE_INITIAL)
	{
		//logmessage ("OVPN", "Options error: Residual parse state (%d) in %d", line_num);
		return 0;
	}

	return ret;
}

static void
bypass_doubledash (char **p)
{
	if (strlen (*p) >= 3 && !strncmp (*p, "--", 2))
		*p += 2;
}

static bool
in_src_get (const struct in_src *is, char *line, const int size)
{
	if (is->type == IS_TYPE_FP)
	{
		return BOOL_CAST (fgets (line, size, is->u.fp));
	}
	else if (is->type == IS_TYPE_BUF)
	{
		bool status = buf_parse (is->u.multiline, '\n', line, size);
		if ((int) strlen (line) + 1 < size)
			strcat (line, "\n");
		return status;
	}
	else
	{
		return false;
	}
}

static char *
read_inline_file (struct in_src *is, const char *close_tag)
{
	char line[OPTION_LINE_SIZE];
	struct buffer buf = alloc_buf (10000);
	char *ret;
	while (in_src_get (is, line, sizeof (line)))
	{
		if (!strncmp (line, close_tag, strlen (close_tag)))
			break;
		buf_printf (&buf, "%s", line);
	}
	ret = string_alloc (buf_str (&buf));
	buf_clear (&buf);
	free_buf (&buf);
	CLEAR (line);
	return ret;
}

static bool
check_inline_file (struct in_src *is, char *p[])
{
	bool ret = false;
	if (p[0] && !p[1])
	{
		char *arg = p[0];
		if (arg[0] == '<' && arg[strlen(arg)-1] == '>')
		{
			struct buffer close_tag;
			arg[strlen(arg)-1] = '\0';
			p[0] = string_alloc (arg+1);
			p[1] = string_alloc (INLINE_FILE_TAG);
			close_tag = alloc_buf (strlen(p[0]) + 4);
			buf_printf (&close_tag, "</%s>", p[0]);
			p[2] = read_inline_file (is, buf_str (&close_tag));
			p[3] = NULL;
			free_buf (&close_tag);
			ret = true;
		}
	}
	return ret;
}

static bool
check_inline_file_via_fp (FILE *fp, char *p[])
{
	struct in_src is;
	is.type = IS_TYPE_FP;
	is.u.fp = fp;
	return check_inline_file (&is, p);
}

void
add_custom(char *nv, char *p[])
{
	char *custom = nvram_safe_get(nv);
	char *param = NULL;
	char *final_custom = NULL;
	int i = 0, size = 0;

	if(!p[0])
		return;

	while(p[i]) {
		size += strlen(p[i]) + 1;
		i++;
	}

	param = (char*)calloc(size, sizeof(char));

	if(!param)
		return;

	i = 0;
	while(p[i]) {
		if(*param)
			strcat(param, " ");
		strcat(param, p[i]);
		i++;
	}

	if(custom) {
		final_custom = calloc(strlen(custom) + strlen(param) + 2, sizeof(char));
		if(final_custom) {
			if(*custom) {
				strcat(final_custom, custom);
				strcat(final_custom, "\n");
			}
			strcat(final_custom, param);
			nvram_set(nv, final_custom);
			free(final_custom);
		}
	}
	else
		nvram_set(nv, param);

	free(param);
}

static int
add_option (char *p[], int line, int unit)
{
	char buf[32] = {0};

	if  (streq (p[0], "dev") && p[1])
	{
		sprintf(buf, "vpn_client%d_if", unit);
		if(!strncmp(p[1], "tun", 3))
			nvram_set(buf, "tun");
		else if(!strncmp(p[1], "tap", 3))
			nvram_set(buf, "tap");
	}
	else if  (streq (p[0], "proto") && p[1])
	{
		sprintf(buf, "vpn_client%d_proto", unit);
		nvram_set(buf, p[1]);
	}
	else if  (streq (p[0], "remote") && p[1])
	{
		sprintf(buf, "vpn_client%d_addr", unit);
		nvram_set(buf, p[1]);

		sprintf(buf, "vpn_client%d_port", unit);
		if(p[2])
			nvram_set(buf, p[2]);
		else
			nvram_set(buf, "1194");
	}
	else if (streq (p[0], "resolv-retry") && p[1])
	{
		sprintf(buf, "vpn_client%d_retry", unit);
		if (streq (p[1], "infinite"))
			nvram_set(buf, "-1");
		else
			nvram_set(buf, p[1]);
	}
	else if (streq (p[0], "comp-lzo"))
	{
		sprintf(buf, "vpn_client%d_comp", unit);
		if(p[1])
			nvram_set(buf, p[1]);
		else
			nvram_set(buf, "adaptive");
	}
	else if (streq (p[0], "cipher") && p[1])
	{
		sprintf(buf, "vpn_client%d_cipher", unit);
		nvram_set(buf, p[1]);
	}
	else if (streq (p[0], "verb") && p[1])
	{
		nvram_set("vpn_loglevel", p[1]);
	}
	else if  (streq (p[0], "ca") && p[1])
	{
		sprintf(buf, "vpn_client%d_crypt", unit);
		nvram_set(buf, "tls");
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_ca", unit);
			nvram_set(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			return VPN_UPLOAD_NEED_CA_CERT;
		}
	}
	else if  (streq (p[0], "cert") && p[1])
	{
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_crt", unit);
			nvram_set(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			return VPN_UPLOAD_NEED_CERT;
		}
	}
	else if  (streq (p[0], "key") && p[1])
	{
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_key", unit);
			nvram_set(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			return VPN_UPLOAD_NEED_KEY;
		}
	}
	else if (streq (p[0], "tls-auth") && p[1])
	{
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_static", unit);
			nvram_set(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			if(p[2]) {
				sprintf(buf, "vpn_server%d_hmac", unit);
				nvram_set(buf, p[2]);
			}
			return VPN_UPLOAD_NEED_STATIC;
		}
	}
	else if (streq (p[0], "secret") && p[1])
	{
		sprintf(buf, "vpn_client%d_crypt", unit);
		nvram_set(buf, "secret");
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_static", unit);
			nvram_set(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			return VPN_UPLOAD_NEED_STATIC;
		}
	}
	else if (streq (p[0], "auth-user-pass"))
	{
		sprintf(buf, "vpn_client%d_userauth", unit);
		nvram_set(buf, "1");
	}
	else if (streq (p[0], "tls-remote") && p[1])
	{
		sprintf(buf, "vpn_client%d_tlsremote", unit);
		nvram_set(buf, "1");
		sprintf(buf, "vpn_client%d_cn", unit);
		nvram_set(buf, p[1]);
	}
	else if (streq (p[0], "key-direction") && p[1])
	{
		sprintf(buf, "vpn_client%d_hmac", unit);
		nvram_set(buf, p[1]);
	}
	else
	{
		sprintf(buf, "vpn_client%d_custom", unit);
		add_custom(buf, p);
	}
	return 0;
}

int
read_config_file (const char *file, int unit)
{
	FILE *fp;
	int line_num;
	char line[OPTION_LINE_SIZE];
	char *p[MAX_PARMS];
	int ret = 0;

	fp = fopen (file, "r");
	if (fp)
	{
		line_num = 0;
		while (fgets(line, sizeof (line), fp))
		{
			int offset = 0;
			CLEAR (p);
			++line_num;
			/* Ignore UTF-8 BOM at start of stream */
			if (line_num == 1 && strncmp (line, "\xEF\xBB\xBF", 3) == 0)
				offset = 3;
			if (parse_line (line + offset, p, SIZE (p), line_num))
			{
				bypass_doubledash (&p[0]);
				check_inline_file_via_fp (fp, p);
				ret |= add_option (p, line_num, unit);
			}
		}
		fclose (fp);
	}
	else
	{
		//logmessage ("OVPN", "Error opening configuration file");
	}

	CLEAR (line);
	CLEAR (p);

	return ret;
}

void reset_client_setting(int unit){
	char nv[32];

	sprintf(nv, "vpn_client%d_custom", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_client%d_comp", unit);
	nvram_set(nv, "no");
	sprintf(nv, "vpn_client%d_reneg", unit);
	nvram_set(nv, "-1");
	sprintf(nv, "vpn_client%d_hmac", unit);
	nvram_set(nv, "-1");
	sprintf(nv, "vpn_client%d_retry", unit);
	nvram_set(nv, "-1");
	sprintf(nv, "vpn_client%d_cipher", unit);
	nvram_set(nv, "default");
	sprintf(nv, "vpn_client%d_tlsremote", unit);
	nvram_set(nv, "0");
	sprintf(nv, "vpn_client%d_cn", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_client%d_userauth", unit);
	nvram_set(nv, "0");
	sprintf(nv, "vpn_client%d_username", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_client%d_password", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_client%d_comp", unit);
	nvram_set(nv, "-1");
	sprintf(nv, "vpn_crt_client%d_ca", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_crt_client%d_crt", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_crt_client%d_key", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_crt_client%d_static", unit);
	nvram_set(nv, "");
}

#if 1//def RTCONFIG_IPV6
#define DHCP_LEASE_FILE		"/var/lib/misc/dnsmasq.leases"
#define IPV6_CLIENT_NEIGH	"/tmp/ipv6_neigh"
#define IPV6_CLIENT_INFO	"/tmp/ipv6_client_info"
#define	IPV6_CLIENT_LIST	"/tmp/ipv6_client_list"
#define	MAC			1
#define	HOSTNAME		2
#define	IPV6_ADDRESS		3
#define BUFSIZE			8192

static int compare_back(FILE *fp, int current_line, char *buffer);
static int check_mac_previous(char *mac);
static char *value(FILE *fp, int line, int token);
static void find_hostname_by_mac(char *mac, char *hostname);
static void get_ipv6_client_info();
static int total_lines = 0;

/* Init File and clear the content */
void init_file(char *file)
{
	FILE *fp;

	if ((fp = fopen(file ,"w")) == NULL) {
		//dbg("can't open %s: %s", file,
		//	strerror(errno));
	}

	fclose(fp);
}

void save_file(const char *file, const char *fmt, ...)
{
	char buf[BUFSIZE];
	va_list args;
	FILE *fp;

	if ((fp = fopen(file ,"a")) == NULL) {
		//dbg("can't open %s: %s", file,
		//	strerror(errno));
	}

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	va_start(args, fmt);
	fprintf(fp, "%s", buf);
	va_end(args);

	fclose(fp);
}

static char *get_stok(char *str, char *dest, char delimiter)
{
	char *p;

	p = strchr(str, delimiter);
	if (p) {
		if (p == str)
			*dest = '\0';
		else
			strncpy(dest, str, p-str);

		p++;
	} else
		strcpy(dest, str);

	return p;
}

static char *value(FILE *fp, int line, int token)
{
	int i;
	static char temp[BUFSIZE], buffer[BUFSIZE];
	char *ptr;
	int temp_len;

	fseek(fp, 0, SEEK_SET);
	for(i = 0; i < line; i++) {
		memset(temp, 0, sizeof(temp));
		fgets(temp, sizeof(temp), fp);
		temp_len = strlen(temp);
		if (temp_len && temp[temp_len-1] == '\n')
			temp[temp_len-1] = '\0';
	}
	memset(buffer, 0, sizeof(buffer));
	switch (token) {
		case MAC:
			get_stok(temp, buffer, ' ');
			break;
		case HOSTNAME:
			ptr = get_stok(temp, buffer, ' ');
			if (ptr)
				get_stok(ptr, buffer, ' ');
			break;
		case IPV6_ADDRESS:
			ptr = get_stok(temp, buffer, ' ');
			if (ptr) {
				ptr = get_stok(ptr, buffer, ' ');
				if (ptr)
					ptr = get_stok(ptr, buffer, ' ');
			}
			break;
		default:
			//dbg("error option\n");
			strcpy(buffer, "ERROR");
			break;
	}

	return buffer;
}

static int check_mac_previous(char *mac)
{
	FILE *fp;
	char temp[BUFSIZE];
	memset(temp, 0, sizeof(temp));

	if ((fp = fopen(IPV6_CLIENT_LIST, "r")) == NULL)
	{
		//dbg("can't open %s: %s", IPV6_CLIENT_LIST,
		//	strerror(errno));

		return 0;
	}

	while (fgets(temp, BUFSIZE, fp)) {
		if (strstr(temp, mac)) {
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);

	return 0;
}

static int compare_back(FILE *fp, int current_line, char *buffer)
{
	int i = 0;
	char mac[32], compare_mac[32];

	buffer[strlen(buffer) -1] = '\0';
	strcpy(mac, value(fp, current_line, MAC));

	if (check_mac_previous(mac))
		return 0;

	for(i = 0; i<(total_lines - current_line); i++) {
		strcpy(compare_mac, value(fp, current_line + 1 + i, MAC));
		if (strcmp(mac, compare_mac) == 0) {
			strcat(buffer, ",");
			strcat(buffer, value(fp, current_line + 1 + i, IPV6_ADDRESS));
		}
	}
	save_file(IPV6_CLIENT_LIST, "%s\n", buffer);

	return 0;
}

static void find_hostname_by_mac(char *mac, char *hostname)
{
	FILE *fp;
	unsigned int expires;
	char *macaddr, *ipaddr, *host_name, *next;
	char line[256];

	if ((fp = fopen(DHCP_LEASE_FILE, "r")) == NULL)
	{
		//dbg("can't open %s: %s", DHCP_LEASE_FILE,
		//	strerror(errno));
		goto END;
	}

	while ((next = fgets(line, sizeof(line), fp)) != NULL)
	{
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		macaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		host_name = strsep(&next, " ") ? : "";

		if (strncasecmp(macaddr, mac, 17) == 0) {
			fclose(fp);
			strcpy(hostname, host_name);
			return;
		}

		memset(macaddr, 0, sizeof(macaddr));
		memset(ipaddr, 0, sizeof(ipaddr));
		memset(host_name, 0, sizeof(host_name));
	}
	fclose(fp);
END:
	strcpy(hostname, "");
}

static void get_ipv6_client_info()
{
	FILE *fp;
	char buffer[128], ipv6_addr[128], mac[32];
	char *ptr_end, hostname[64];
	doSystem("ip -f inet6 neigh show dev %s > %s", nvram_safe_get("lan_ifname"), IPV6_CLIENT_NEIGH);
	usleep(1000);

	if ((fp = fopen(IPV6_CLIENT_NEIGH, "r")) == NULL)
	{
		//dbg("can't open %s: %s", IPV6_CLIENT_NEIGH,
		//	strerror(errno));
		return;
	}

	init_file(IPV6_CLIENT_INFO);
	while (fgets(buffer, 128, fp)) {
		int temp_len = strlen(buffer);
		if (temp_len && buffer[temp_len-1] == '\n')
			buffer[temp_len-1] = '\0';
		if ((ptr_end = strstr(buffer, "lladdr")))
		{
			ptr_end = ptr_end - 1;
			memset(ipv6_addr, 0, sizeof(ipv6_addr));
			strncpy(ipv6_addr, buffer, ptr_end - buffer);
			ptr_end = ptr_end + 8;
			memset(mac, 0, sizeof(mac));
			strncpy(mac, ptr_end, 17);
			find_hostname_by_mac(mac, hostname);
			if ( (ipv6_addr[0] == '2' || ipv6_addr[0] == '3')
				&& ipv6_addr[0] != ':' && ipv6_addr[1] != ':'
				&& ipv6_addr[2] != ':' && ipv6_addr[3] != ':')
				save_file(IPV6_CLIENT_INFO, "%s %s %s\n", hostname, mac, ipv6_addr);
		}

		memset(buffer, 0, sizeof(buffer));
	}
	fclose(fp);
}

static void get_ipv6_client_list(void)
{
	FILE *fp;
	int line_index = 1;
	char temp[BUFSIZE];
	memset(temp, 0, sizeof(temp));
	init_file(IPV6_CLIENT_LIST);

	if ((fp = fopen(IPV6_CLIENT_INFO, "r")) == NULL)
	{
		//dbg("can't open %s: %s", IPV6_CLIENT_INFO,
		//	strerror(errno));

		return;
	}

	total_lines = 0;
	while (fgets(temp, BUFSIZE, fp))
		total_lines++;
	fseek(fp, 0, SEEK_SET);
	memset(temp, 0, sizeof(temp));

	while (fgets(temp, BUFSIZE, fp)) {
		compare_back(fp, line_index, temp);
		value(fp, line_index, MAC);
		line_index++;
	}
	fclose(fp);

	line_index = 1;
}

int get_lan_ipv6dev_list(ipv6_landev_info_t *ipv6_landev_info)
{
	int first, count = 0;
	FILE *fp;
	char hostname[64], macaddr[32], ipaddr[8192], ipaddrs[8192];
	char *p;

	/* Refresh lease file to get actual expire time */
	killall("dnsmasq", SIGUSR2);
	usleep(100 * 1000);

	get_ipv6_client_info();
	get_ipv6_client_list();

	if ((fp = fopen(IPV6_CLIENT_LIST, "r")) == NULL) {
		return count;
	}

	while (fscanf(fp, "%64s %32s %8192s\n", hostname, macaddr, ipaddr) == 3) {
		memset(ipaddrs, 0, sizeof(ipaddrs));
		first = 1;
		p = strtok(ipaddr, ",");
		while (p)
		{
			if (first)
				first = 0;
			else
				sprintf(ipaddrs, "%s, ", ipaddrs);

			sprintf(ipaddrs, "%s%s", ipaddrs, (char *)p);
			p = strtok(NULL, ",");
		}

		sprintf(ipv6_landev_info[count].hostname, "%s", hostname);
		sprintf(ipv6_landev_info[count].mac_addr, "%s", macaddr);
		sprintf(ipv6_landev_info[count].ipv6_addrs, "%s", ipaddrs);		
		count++;
	}
	fclose(fp);

	return count;
}


int get_ipv6_routing_info(ipv6_routing_info_t *ipv6_routing_info)
{
	FILE *fp;
	char buf[256];
	unsigned int fl = 0;
	int found = 0;
	char addr6[128], *naddr6;
	/* In addr6x, we store both 40-byte ':'-delimited ipv6 addresses.
	 * We read the non-delimited strings into the tail of the buffer
	 * using fscanf and then modify the buffer by shifting forward
	 * while inserting ':'s and the nul terminator for the first string.
	 * Hence the strings are at addr6x and addr6x+40.  This generates
	 * _much_ less code than the previous (upstream) approach. */
	char addr6x[80];
	char iface[16], flags[16];
	int iflags, metric, refcnt, use, prefix_len, slen;
	struct sockaddr_in6 snaddr6;
	int count = 0;

	if ((fp = fopen("/proc/net/if_inet6", "r")) == (FILE*)0)
		return count;

	while (fgets(buf, 256, fp) != NULL)
	{
		if(strstr(buf, "br0") == (char*) 0)
			continue;

		if (sscanf(buf, "%*s %*02x %*02x %02x", &fl) != 1)
			continue;

		if ((fl & 0xF0) == 0x20)
		{
			/* Link-Local Address is ready */
			found = 1;
			break;
		}
	}
	fclose(fp);

	if (found)
	{
		fp = fopen("/proc/net/ipv6_route", "r");

		while (1) {
			int r;
			r = fscanf(fp, "%32s%x%*s%x%32s%x%x%x%x%s\n",
					addr6x+14, &prefix_len, &slen, addr6x+40+7,
					&metric, &use, &refcnt, &iflags, iface);
			if (r != 9) {
				if ((r < 0) && feof(fp)) { /* EOF with no (nonspace) chars read. */
					break;
				}
	 ERROR:
				return count;
			}

			/* Do the addr6x shift-and-insert changes to ':'-delimit addresses.
			 * For now, always do this to validate the proc route format, even
			 * if the interface is down. */
			{
				int i = 0;
				char *p = addr6x+14;

				do {
					if (!*p) {
						if (i == 40) { /* nul terminator for 1st address? */
							addr6x[39] = 0;	/* Fixup... need 0 instead of ':'. */
							++p;	/* Skip and continue. */
							continue;
						}
						goto ERROR;
					}
					addr6x[i++] = *p++;
					if (!((i+1) % 5)) {
						addr6x[i++] = ':';
					}
				} while (i < 40+28+7);
			}

			if (!(iflags & RTF_UP)) { /* Skip interfaces that are down. */
				continue;
			}

			ipv6_set_flags(flags, (iflags & IPV6_MASK));

			r = 0;
			do {
				inet_pton(AF_INET6, addr6x + r,
						  (struct sockaddr *) &snaddr6.sin6_addr);
				snaddr6.sin6_family = AF_INET6;
				naddr6 = INET6_rresolve((struct sockaddr_in6 *) &snaddr6,
							   0x0fff /* Apparently, upstream never resolves. */
							   );

				if (!r) {			/* 1st pass */
					snprintf(addr6, sizeof(addr6), "%s/%d", naddr6, prefix_len);
					r += 40;
					free(naddr6);
				} else {			/* 2nd pass */
					sprintf(ipv6_routing_info[count].dest, "%s", addr6);
					sprintf(ipv6_routing_info[count].next_hop, "%s", naddr6);
					sprintf(ipv6_routing_info[count].flags, "%s", flags);
					ipv6_routing_info[count].metric = metric;
					ipv6_routing_info[count].ref = refcnt;
					ipv6_routing_info[count].use = use;
					sprintf(ipv6_routing_info[count].iface, "%s", iface);
					free(naddr6);
					break;
				}
			} while (1);

			count++;
		}
	}

	return count;
}

#endif


void get_day_timestr(pc_list_info_t *pc_info_entry, int day, int start_hr, int end_hr)
{
	char timestr[10];

	sprintf(timestr, "%02d-%02d", start_hr, end_hr);

	switch(day){
		case 0: /* Sunday */
			if(strlen(pc_info_entry->sunday))
				strcat(pc_info_entry->sunday, ",");
			strcat(pc_info_entry->sunday, timestr);		
			break;
		case 1: /* Monday */
			if(strlen(pc_info_entry->monday))
				strcat(pc_info_entry->monday, ",");
			strcat(pc_info_entry->monday, timestr);				
			break;
		case 2: /* Tuesday */
			if(strlen(pc_info_entry->tuesday))
				strcat(pc_info_entry->tuesday, ",");
			strcat(pc_info_entry->tuesday, timestr);			
			break;			
		case 3: /* Wednesday */
			if(strlen(pc_info_entry->wednesday))
				strcat(pc_info_entry->wednesday, ",");
			strcat(pc_info_entry->wednesday, timestr);			
			break;
		case 4: /* Thursday */
			if(strlen(pc_info_entry->thursday))
				strcat(pc_info_entry->thursday, ",");
			strcat(pc_info_entry->thursday, timestr);				
			break;
		case 5: /* Friday*/
			if(strlen(pc_info_entry->friday))
				strcat(pc_info_entry->friday, ",");
			strcat(pc_info_entry->friday, timestr);						
			break;		
		case 6:	/* Saturday */
			if(strlen(pc_info_entry->saturday))
				strcat(pc_info_entry->saturday, ",");
			strcat(pc_info_entry->saturday, timestr);
			break;	
	}
}


int get_pc_list_info(pc_list_info_t *pc_list_info) /* Get parental control list information. */
{
	int i, j, count = 0;
	char *nv, *nvp, *b; 
	char *nv2, *nvp2, *b2;
	char *ptr, *ptr_end, bak;	
	int start_day, end_day, start_hour, end_hour;
	char string[SPRINT_MAX_LEN];

	if(nvram_safe_get("MULTIFILTER_ENABLE") != NULL)
	{
		nv = nvp = strdup(nvram_safe_get("MULTIFILTER_ENABLE"));
		if (nv) {
			while ((b = strsep(&nvp, ">")) != NULL) {
				pc_list_info[count].enable = atoi(b);
				count++;      
			}
	        free(nv);
		}
	}

	i = 0;
	if(nvram_safe_get("MULTIFILTER_DEVICENAME") != NULL)
	{
		nv = nvp = strdup(nvram_safe_get("MULTIFILTER_DEVICENAME"));
		if (nv) {
			while ((b = strsep(&nvp, ">")) != NULL) {
				strcpy(pc_list_info[i].device_name, b);
				i++;      
			}
	        free(nv);
		}
	}

	i = 0;
	if(nvram_safe_get("MULTIFILTER_MAC") != NULL)	
	{		
		nv = nvp = strdup(nvram_safe_get("MULTIFILTER_MAC"));
		if (nv) {
			while ((b = strsep(&nvp, ">")) != NULL) {
				strcpy(pc_list_info[i].mac, b);
				i++;      
			}
	        free(nv);
		}
	}

	i = 0;
	if(nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME") != NULL)
	{	
	    nv = nvp = strdup(nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME"));
	    if (nv) {
	    	while ((b = strsep(&nvp, ">")) != NULL) {
	    		if(strlen(b) >= 14){
		   			while((b2 = strsep(&b, "<")) != NULL) {
		   				if(!strcmp(b2, "NoTitle")) continue;
		   				ptr = b2;
		   				ptr_end = ptr+1;
		   				bak = ptr_end[0];
		   				ptr_end[0] = 0;
		   				start_day = atoi(ptr);
		   				ptr_end[0] = bak;   				

		   				ptr = b2+1;
						ptr_end = ptr+1;
						bak = ptr_end[0];
						ptr_end[0] = 0;
						end_day = atoi(ptr);
						ptr_end[0] = bak;

						ptr = b2+2;
						ptr_end = ptr+2;
						bak = ptr_end[0];
						ptr_end[0] = 0;
						start_hour = atoi(ptr);
						ptr_end[0] = bak;

						ptr = b2+4;
						ptr_end = ptr+2;
						bak = ptr_end[0];
						ptr_end[0] = 0;
						end_hour = atoi(ptr);
						ptr_end[0] = bak;

						if(start_day == end_day) {
							if(start_hour == end_hour) {//whole week
								for( j = 0; j < 7; j++)
									get_day_timestr(&pc_list_info[i], j, 0, 24);
							}
							else {	
								get_day_timestr(&pc_list_info[i], start_day, start_hour, end_hour);			
							}
						}					
						else if(start_day < end_day || end_day == 0)
						{
							if(end_day == 0)
								end_day = 7;

							get_day_timestr(&pc_list_info[i], start_day, start_hour, 24);	

							if(end_day - start_day > 1) {
								for( j = start_day+1; j < end_day; j++)
									get_day_timestr(&pc_list_info[i], j, 0, 24);
							}

							if(end_hour > 0)
								get_day_timestr(&pc_list_info[i], end_day, 0, end_hour);					
						}
		   			}
	   			}
	   			i++;
			}
			free(nv);
		}    
	}
	return count;
}

#define MAX_PCONTROL_ENTRY  16

int set_pc_client_enable(int index, int value)
{
	int i, count, real_index;
	char string[SPRINT_MAX_LEN], val_list[MAX_PCONTROL_ENTRY][2], valstr[2];
	char *nv, *nvp, *b; 

	if(value < 0 || value > 1)
		return 0;

	for( i = 0; i < MAX_PCONTROL_ENTRY; i++)
		memset(val_list[i], 0x0, 2);	

	count = 0;
	if(nvram_safe_get("MULTIFILTER_ENABLE") != NULL)
	{
		nv = nvp = strdup(nvram_safe_get("MULTIFILTER_ENABLE"));
		if (nv) {
			while ((b = strsep(&nvp, ">")) != NULL) {
				strcpy(val_list[count], b);
				count++;      
			}
	        free(nv);
		}
	}

	if(index+1 > count)
	{
		real_index = count;
		count++;

		memset(string, 0x0, SPRINT_MAX_LEN);
		strcpy(string, nvram_safe_get("MULTIFILTER_DEVICENAME"));
		strcat(string, ">");
		nvram_set("MULTIFILTER_DEVICENAME", string);	

		memset(string, 0x0, SPRINT_MAX_LEN);
		strcpy(string, nvram_safe_get("MULTIFILTER_MAC"));
		strcat(string, ">");
		nvram_set("MULTIFILTER_MAC", string);	

		memset(string, 0x0, SPRINT_MAX_LEN);
		strcpy(string, nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME"));
		strcat(string, "><");
		nvram_set("MULTIFILTER_MACFILTER_DAYTIME", string);			

	}		
	else
		real_index = index;
	
	sprintf(val_list[real_index], "%d", value);

	memset(string, 0x0, SPRINT_MAX_LEN);
	for(i = 0; i < count; i++)
	{	
		if(i == 0){
			sprintf(string, "%s", val_list[i]);
		}
		else {
			strcat(string, ">");	
			sprintf(valstr, "%s", val_list[i]);
			strcat(string, valstr);
		}
	}	

	nvram_set("MULTIFILTER_ENABLE", string);

	return count;

}


int set_pc_client_name(int index, char *value)
{
	int i, count, real_index;
	char string[SPRINT_MAX_LEN], val_list[MAX_PCONTROL_ENTRY][33];
	char *nv, *nvp, *b; 	

	for( i = 0; i < MAX_PCONTROL_ENTRY; i++)
		memset(val_list[i], 0x0, 33);

	count = 0;
	if(nvram_safe_get("MULTIFILTER_DEVICENAME") != NULL)
	{
		nv = nvp = strdup(nvram_safe_get("MULTIFILTER_DEVICENAME"));
		if (nv) {
			while ((b = strsep(&nvp, ">")) != NULL) {
				strcpy(val_list[count], b);
				count++;      
			}
	        free(nv);
		}
	}

	if(index+1 > count)
	{
		real_index = count;
		count++;

		memset(string, 0x0, SPRINT_MAX_LEN);
		strcpy(string, nvram_safe_get("MULTIFILTER_ENABLE"));
		strcat(string, ">1");
		nvram_set("MULTIFILTER_ENABLE", string);	

		memset(string, 0x0, SPRINT_MAX_LEN);
		strcpy(string, nvram_safe_get("MULTIFILTER_MAC"));
		strcat(string, ">");
		nvram_set("MULTIFILTER_MAC", string);	

		memset(string, 0x0, SPRINT_MAX_LEN);
		strcpy(string, nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME"));
		strcat(string, "><");
		nvram_set("MULTIFILTER_MACFILTER_DAYTIME", string);	
	}		
	else
		real_index = index;

	strcpy(val_list[real_index], value);

	memset(string, 0x0, SPRINT_MAX_LEN);
	for(i = 0; i < count; i++)
	{
		if(i == 0)
			sprintf(string, "%s", val_list[i]);
		else {
			strcat(string, ">");
			strcat(string, val_list[i]);
		}		
	}	

	nvram_set("MULTIFILTER_DEVICENAME", string);

	return count;
}

int set_pc_client_mac(int index, char *value)
{
	int i, count, real_index;
	char string[SPRINT_MAX_LEN], val_list[MAX_PCONTROL_ENTRY][18];
	char *nv, *nvp, *b; 	

	for( i = 0; i < MAX_PCONTROL_ENTRY; i++)
		memset(val_list[i], 0x0, 18);

	count = 0;
	if(nvram_safe_get("MULTIFILTER_MAC") != NULL)	
	{		
		nv = nvp = strdup(nvram_safe_get("MULTIFILTER_MAC"));
		if (nv) {
			while ((b = strsep(&nvp, ">")) != NULL) {
				strcpy(val_list[count], b);
				count++;      
			}
	        free(nv);
		}
	}

	if(index+1 > count)
	{
		real_index = count;
		count++;	

		memset(string, 0x0, SPRINT_MAX_LEN);
		strcpy(string, nvram_safe_get("MULTIFILTER_ENABLE"));
		strcat(string, ">1");
		nvram_set("MULTIFILTER_ENABLE", string);	

		memset(string, 0x0, SPRINT_MAX_LEN);
		strcpy(string, nvram_safe_get("MULTIFILTER_DEVICENAME"));
		strcat(string, ">");
		nvram_set("MULTIFILTER_DEVICENAME", string);

		memset(string, 0x0, SPRINT_MAX_LEN);
		strcpy(string, nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME"));
		strcat(string, "><");
		nvram_set("MULTIFILTER_MACFILTER_DAYTIME", string);			
	}		
	else
		real_index = index;

	strcpy(val_list[real_index], value);

	memset(string, 0x0, SPRINT_MAX_LEN);
	for(i = 0; i < count; i++)
	{
		if(i == 0)
			sprintf(string, "%s", val_list[i]);
		else {
			strcat(string, ">");
			strcat(string, val_list[i]);
		}		
	}	
	
	nvram_set("MULTIFILTER_MAC", string);

	return count;
}

#define TIMESTR_LEN		1280
#define ONEDAYSTR_LEN	256

int set_pc_client_dayTime(int index, int day, char *value)
{
	int i, count, real_index;
	char string[SPRINT_MAX_LEN], val_list[MAX_PCONTROL_ENTRY][TIMESTR_LEN];
	char *nv, *nvp, *b; 		
	char *ptr, *ptr_end, bak;	
	int start_day, end_day, start_hour, end_hour;
	char daytimestr[ONEDAYSTR_LEN], tmpstr[16], timestr1[TIMESTR_LEN], timestr2[TIMESTR_LEN];

	for( i =0; i < MAX_PCONTROL_ENTRY; i++)
		memset(val_list[i], 0x0, TIMESTR_LEN);

	if( !strlen(value))
		return 0;

	/* Parse value string*/
	memset(daytimestr, 0x0, ONEDAYSTR_LEN);
	i = 0;
	nv = nvp = strdup(value);
	if (nv) {
		while ((b = strsep(&nvp, ",")) != NULL) {
   			ptr = b;
   			ptr_end = ptr+2;
   			bak = ptr_end[0];
   			ptr_end[0] = 0;
   			start_hour = atoi(ptr);
   			ptr_end[0] = bak;  			

			ptr = b+3;
			ptr_end = ptr+2;
			bak = ptr_end[0];
			ptr_end[0] = 0;
			end_hour = atoi(ptr);
			ptr_end[0] = bak;
			if(i == 0)
				sprintf(daytimestr, "NoTitle<%d%d%02d%02d", day, day, start_hour, end_hour);
			else
			{
				strcat(daytimestr, "<");
				memset(tmpstr, 0x0, 16);
				sprintf(tmpstr, "NoTitle<%d%d%02d%02d", day, day, start_hour, end_hour);
				strcat(daytimestr, tmpstr);
			}
			i++;      
		}
		free(nv);
	}	

	count = 0;
	{
		nv = nvp = strdup(nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME"));
		if (nv) {
			while ((b = strsep(&nvp, ">")) != NULL) {
				strcpy(val_list[count], b);
				count++;      
			}
	        free(nv);
		}
	}

	if(index+1 > count)
	{
		real_index = count;
		count++;
		strcpy(val_list[real_index], daytimestr);
	}		
	else
	{		
		real_index = index;
		memset(timestr1, 0x0, TIMESTR_LEN);
		memset(timestr2, 0x0, TIMESTR_LEN);				
		if(strlen(val_list[real_index]) >= 14)
		{
			nv = nvp = strdup(val_list[real_index]);
			if (nv) {
				i = 0;
				while ((b = strsep(&nvp, "<")) != NULL) {
	   				if(!strcmp(b, "NoTitle") || !strlen(b)) continue;
	   				ptr = b;
	   				ptr_end = ptr+1;
	   				bak = ptr_end[0];
	   				ptr_end[0] = 0;
	   				start_day = atoi(ptr);
	   				ptr_end[0] = bak;   				

	   				ptr = b+1;
					ptr_end = ptr+1;
					bak = ptr_end[0];
					ptr_end[0] = 0;
					end_day = atoi(ptr);
					ptr_end[0] = bak;

					ptr = b+2;
					ptr_end = ptr+2;
					bak = ptr_end[0];
					ptr_end[0] = 0;
					start_hour = atoi(ptr);
					ptr_end[0] = bak;

					ptr = b+4;
					ptr_end = ptr+2;
					bak = ptr_end[0];
					ptr_end[0] = 0;
					end_hour = atoi(ptr);
					ptr_end[0] = bak;

					if(start_day < day) 
					{
						if((start_day == end_day) || (end_day != 0 && end_day < day))
						{												
							if(i > 0) 
								strcat(timestr1, "<");
							strcat(timestr1, "NoTitle<");
							strcat(timestr1, b);
						}	
						else if(end_day == day)
						{
							sprintf(tmpstr, "NoTitle<%d%d%02d%02d", start_day, end_day, start_hour, 0);
							if(i > 0) 
								strcat(timestr1, "<");						
							strcat(timestr1, tmpstr);
						}						
						else if(end_day > day || end_day == 0)
						{
							sprintf(tmpstr, "NoTitle<%d%d%02d%02d", start_day, day, start_hour, 0);	
							if(i > 0) 
								strcat(timestr1, "<");						
							strcat(timestr1, tmpstr);
							sprintf(tmpstr, "NoTitle<%d%d%02d%02d", day+1, end_day, 0, end_hour);	
							strcat(timestr2, "<");
							strcat(timestr2, tmpstr);							
						}			
					}
					else if(start_day == day)
					{
						if(end_day > day || end_day == 0) 
						{
							sprintf(tmpstr, "NoTitle<%d%d%02d%02d", day+1, end_day, 0, end_hour);						
							strcat(timestr2, "<");
							strcat(timestr2, tmpstr);
						}
					}
					else if(start_day > day)
					{
						if(i > 0) 
							strcat(timestr2, "<");					
						strcat(timestr2, "NoTitle<");
						strcat(timestr2, b);
					}
					i++;
				}
		        free(nv);
			}
		}	

		memset(val_list[real_index], 0x0, TIMESTR_LEN);
		if(strlen(timestr1))
		{			
			strcat(val_list[real_index], timestr1);
			strcat(val_list[real_index], "<");
		}	
		strcat(val_list[real_index], daytimestr);
		if(strlen(timestr2))
			strcat(val_list[real_index], timestr2);
	}	

	memset(string, 0x0, SPRINT_MAX_LEN);
	for(i = 0; i < count; i++)
	{
		if(i == 0)
			sprintf(string, "%s", val_list[i]);
		else {
			strcat(string, ">");
			strcat(string, val_list[i]);
		}		
	}	

	nvram_set("MULTIFILTER_MACFILTER_DAYTIME", string);	

	return count;
}


int del_pc_client_entry(int index)
{
	int i, count, real_index;
	char string[SPRINT_MAX_LEN], val_list[MAX_PCONTROL_ENTRY][TIMESTR_LEN];
	char *nv, *nvp, *b; 		

	for( i = 0; i < MAX_PCONTROL_ENTRY; i++)
		memset(val_list[i], 0x0, TIMESTR_LEN);	

	count = 0;
	if(nvram_safe_get("MULTIFILTER_ENABLE") != NULL)
	{
		nv = nvp = strdup(nvram_safe_get("MULTIFILTER_ENABLE"));
		if (nv) {
			while ((b = strsep(&nvp, ">")) != NULL) {
				strcpy(val_list[count], b);
				count++;      
			}
	        free(nv);
		}
	}

	if( index < count )
	{	
		memset(val_list[index], 0x0, TIMESTR_LEN);
		memset(string, 0x0, SPRINT_MAX_LEN);
		for(i = 0; i < count; i++)
		{	
			if(i != index)
			{				
				if(i > 0)		
					strcat(string, ">");
				strcat(string, val_list[i]);
			}
		}	
		nvram_set("MULTIFILTER_ENABLE", string);

		/* Device Name*/
		for( i = 0; i < MAX_PCONTROL_ENTRY; i++)
			memset(val_list[i], 0x0, TIMESTR_LEN);	

		i = 0;
		if(nvram_safe_get("MULTIFILTER_DEVICENAME") != NULL)
		{
			nv = nvp = strdup(nvram_safe_get("MULTIFILTER_DEVICENAME"));
			if (nv) {
				while ((b = strsep(&nvp, ">")) != NULL) {
					strcpy(val_list[i], b);
					i++;      
				}
		        free(nv);
			}
		}

		memset(val_list[index], 0x0, TIMESTR_LEN);
		memset(string, 0x0, SPRINT_MAX_LEN);
		for(i = 0; i < count; i++)
		{	
			if(i != index)
			{				
				if(i > 0)		
					strcat(string, ">");
				strcat(string, val_list[i]);
			}
		}	
		nvram_set("MULTIFILTER_DEVICENAME", string);	

		/* MAC */
		for( i = 0; i < MAX_PCONTROL_ENTRY; i++)
			memset(val_list[i], 0x0, TIMESTR_LEN);	

		i = 0;
		if(nvram_safe_get("MULTIFILTER_MAC") != NULL)
		{
			nv = nvp = strdup(nvram_safe_get("MULTIFILTER_MAC"));
			if (nv) {
				while ((b = strsep(&nvp, ">")) != NULL) {
					strcpy(val_list[i], b);
					i++;      
				}
		        free(nv);
			}
		}

		memset(val_list[index], 0x0, TIMESTR_LEN);
		memset(string, 0x0, SPRINT_MAX_LEN);
		for(i = 0; i < count; i++)
		{	
			if(i != index)
			{				
				if(i > 0)		
					strcat(string, ">");
				strcat(string, val_list[i]);
			}
		}	
		nvram_set("MULTIFILTER_MAC", string);			
	
		/* Day time */
		for( i = 0; i < MAX_PCONTROL_ENTRY; i++)
			memset(val_list[i], 0x0, TIMESTR_LEN);	

		i = 0;
		if(nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME") != NULL)
		{
			nv = nvp = strdup(nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME"));
			if (nv) {
				while ((b = strsep(&nvp, ">")) != NULL) {
					strcpy(val_list[i], b);
					i++;      
				}
		        free(nv);
			}
		}

		memset(val_list[index], 0x0, TIMESTR_LEN);
		memset(string, 0x0, SPRINT_MAX_LEN);
		for(i = 0; i < count; i++)
		{	
			if(i != index)
			{				
				if(i > 0)		
					strcat(string, ">");
				strcat(string, val_list[i]);
			}
		}	
		nvram_set("MULTIFILTER_MACFILTER_DAYTIME", string);		

		count--;
	}	

	return count;
}	
