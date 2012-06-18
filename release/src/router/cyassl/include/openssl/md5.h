/* md5.h for openssl */


#ifndef CYASSL_MD5_H_
#define CYASSL_MD5_H_

#ifdef YASSL_PREFIX
#include "prefix_md5.h"
#endif

#ifdef __cplusplus
    extern "C" {
#endif


typedef struct MD5_CTX {
    int holder[24];   /* big enough to hold ctaocrypt md5, but check on init */
} MD5_CTX;

void MD5_Init(MD5_CTX*);
void MD5_Update(MD5_CTX*, const void*, unsigned long);
void MD5_Final(unsigned char*, MD5_CTX*);



#ifdef __cplusplus
    }  /* extern "C" */ 
#endif


#endif /* CYASSL_MD5_H_ */

