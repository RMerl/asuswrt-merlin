

%module cyassl
%{
    #include "openssl/ssl.h"
    #include "rsa.h"

    /* defn adds */
    char* CyaSSL_error_string(int err);
    int   CyaSSL_connect(SSL*, const char* server, int port);
    RNG*  GetRng(void);
    RsaKey* GetRsaPrivateKey(const char* file);
    void    FillSignStr(unsigned char*, const char*, int);
%}


SSL_METHOD* TLSv1_client_method(void);
SSL_CTX*    SSL_CTX_new(SSL_METHOD*);
int         SSL_CTX_load_verify_locations(SSL_CTX*, const char*, const char*);
SSL*        SSL_new(SSL_CTX*);
int         SSL_get_error(SSL*, int);
int         SSL_write(SSL*, const char*, int);
char*       CyaSSL_error_string(int);
int         CyaSSL_connect(SSL*, const char* server, int port);

int         RsaSSL_Sign(const unsigned char* in, int inLen, unsigned char* out, int outLen, RsaKey* key, RNG* rng);

int         RsaSSL_Verify(const unsigned char* in, int inLen, unsigned char* out, int outLen, RsaKey* key);

RNG* GetRng(void);
RsaKey* GetRsaPrivateKey(const char* file);
void    FillSignStr(unsigned char*, const char*, int);

%include carrays.i
%include cdata.i
%array_class(unsigned char, byteArray);
int         SSL_read(SSL*, unsigned char*, int);


#define    SSL_FAILURE      0
#define    SSL_SUCCESS      1

