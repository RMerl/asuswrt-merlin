#ifndef MD5_H
#define MD5_H
#ifndef HEADER_MD5_H
/* Try to avoid clashes with OpenSSL */
#define HEADER_MD5_H 
#endif

#ifdef HAVE_MD5_H
/*
 * Try to avoid clashes with Solaris MD5 implementation.
 * ...where almost all implementations follows:
 * "Schneier's Cryptography Classics Library"
 */
#include <md5.h>
#else

struct MD5Context {
	uint32_t buf[4];
	uint32_t bits[2];
	uint8_t in[64];
};
typedef struct MD5Context MD5_CTX;

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, const uint8_t *buf,
	       size_t len);
void MD5Final(uint8_t digest[16], struct MD5Context *context);

#endif /* !HAVE_MD5_H */

#endif /* !MD5_H */
