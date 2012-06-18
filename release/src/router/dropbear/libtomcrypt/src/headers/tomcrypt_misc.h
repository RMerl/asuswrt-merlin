/* ---- BASE64 Routines ---- */
#ifdef BASE64
int base64_encode(const unsigned char *in,  unsigned long len, 
                        unsigned char *out, unsigned long *outlen);

int base64_decode(const unsigned char *in,  unsigned long len, 
                        unsigned char *out, unsigned long *outlen);
#endif

/* ---- MEM routines ---- */
void zeromem(void *dst, size_t len);
void burn_stack(unsigned long len);

const char *error_to_string(int err);

extern const char *crypt_build_settings;

/* ---- HMM ---- */
int crypt_fsa(void *mp, ...);

/* $Source: /cvs/libtom/libtomcrypt/src/headers/tomcrypt_misc.h,v $ */
/* $Revision: 1.4 $ */
/* $Date: 2006/11/06 03:03:01 $ */
