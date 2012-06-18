/* crypto.h for openSSL */

#ifndef CYASSL_CRYPTO_H_
#define CYASSL_CRYPTO_H_

#ifdef YASSL_PREFIX
#include "prefix_crypto.h"
#endif

const char*   SSLeay_version(int type);
unsigned long SSLeay(void);


#define SSLEAY_VERSION 0x0090600fL
#define SSLEAY_VERSION_NUMBER SSLEAY_VERSION


#endif /* header */

