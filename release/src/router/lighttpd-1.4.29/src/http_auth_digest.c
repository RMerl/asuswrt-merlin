#include "buffer.h"

#include "http_auth_digest.h"

#include <string.h>

#ifndef USE_OPENSSL
# include "md5.h"

typedef li_MD5_CTX MD5_CTX;
#define MD5_Init li_MD5_Init
#define MD5_Update li_MD5_Update
#define MD5_Final li_MD5_Final

#endif

void CvtHex(IN HASH Bin, OUT HASHHEX Hex) {
	unsigned short i;

	for (i = 0; i < HASHLEN; i++) {
		Hex[i*2] = int2hex((Bin[i] >> 4) & 0xf);
		Hex[i*2+1] = int2hex(Bin[i] & 0xf);
	}
	Hex[HASHHEXLEN] = '\0';
}

