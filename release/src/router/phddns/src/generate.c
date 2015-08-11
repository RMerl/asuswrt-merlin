#include <stdio.h> 
#include <memory.h>
#include "md5.h"
#include "generate.h"
#include "lutil.h"
 


#ifdef OFFICIAL_CLIENT
#include "official/official_crypt.c"
#else
//__stdcall
int GenerateCrypt(char *szUser, 
							 char *szPassword, 
							 char *szChallenge64, 
                                                         int clientinfo,
                                                         int embkey,
							 char *szResult)
{
	unsigned char szDecoded[256];
	unsigned char szKey[256];
	unsigned char szAscii[256];

	unsigned int nDecodedLen;
	long challengetime = 0;
	int nMoveBits;
	long challengetime_new = 0;
	unsigned int nKey;
	int nUser;
	unsigned int nEncoded;
	int temp;
	unsigned int uTemp;
	int temp1, temp3, temp4, temp2, temp5;

	//Base64 ¿¿
	nDecodedLen =  lutil_b64_pton(szChallenge64, szDecoded, 256);
	uTemp = *(szDecoded +9);
	uTemp = (uTemp << 24) & 0xff000000;
	challengetime += uTemp;
	uTemp = *(szDecoded +8);
	uTemp = (uTemp << 16) & 0x00ff0000;
	challengetime += uTemp;
	uTemp = *(szDecoded +7);
	uTemp = (uTemp << 8) & 0x0000ff00;
	challengetime += uTemp;
	uTemp = *(szDecoded +6);
	challengetime += uTemp;
	//challengetime = *((int*)(szDecoded + 6));
	temp = ~embkey;
	challengetime |= temp;
	nMoveBits = challengetime % 30;
	temp1 = challengetime << ((32 - nMoveBits)&31);
	temp3 = challengetime >> (nMoveBits&31);
	temp4  = 0xffffffff << ((32 - nMoveBits)&31);
	temp5 = ~temp4;
	temp2 = temp3 & temp5;
	challengetime_new = temp1 | temp2;

	//KEY-MD5
	nKey = KeyMD5Encode(szKey, (unsigned char*)szPassword, strlen((char*)szPassword), (unsigned char*)szDecoded, nDecodedLen);
	szKey[nKey] = 0;
	
	nUser = strlen((char *)szUser);

	memcpy(szAscii, szUser, nUser);
	szAscii[nUser] = ' ';
	temp1 = challengetime_new & 0x000000ff;
	szAscii[nUser + 1] = (char)temp1;
	temp1 = (challengetime_new & 0x0000ff00) >> 8;
	szAscii[nUser + 2] = (char)temp1;
	temp1 = (challengetime_new & 0x00ff0000) >> 16;
	szAscii[nUser + 3] = (char)temp1;
	temp1 = (challengetime_new & 0xff000000) >> 24;
	szAscii[nUser + 4] = (char)temp1;
	//memcpy(szAscii+nUser+1, &challengetime_new,4);
	temp1 = clientinfo & 0x000000ff;
	szAscii[nUser + 5] = (char)temp1;
	temp1 = (clientinfo & 0x0000ff00) >> 8;
	szAscii[nUser + 6] = (char)temp1;
	temp1 = (clientinfo & 0x00ff0000) >> 16;
	szAscii[nUser + 7] = (char)temp1;
	temp1 = (clientinfo & 0xff000000) >> 24;
	szAscii[nUser + 8] = (char)temp1;
	//memcpy(szAscii+nUser+1+4,&clientinfo,4);
	memcpy(szAscii+nUser+1+4+4, szKey, nKey);

	//base64 ¿¿
	nEncoded =  lutil_b64_ntop((unsigned char *)szAscii, nUser + 1 + 4 + 4 + nKey, szResult, 256);
	return nEncoded;
}
#endif
 
