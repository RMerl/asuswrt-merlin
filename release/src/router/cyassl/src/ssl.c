/* ssl.c
 *
 * Copyright (C) 2006-2009 Sawtooth Consulting Ltd.
 *
 * This file is part of CyaSSL.
 *
 * CyaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CyaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


#include "openssl/ssl.h"
#include "cyassl_int.h"
#include "cyassl_error.h"
#include "coding.h"

#ifdef OPENSSL_EXTRA
    #include "openssl/evp.h"
    #include "openssl/hmac.h"
    #include "openssl/crypto.h"
    #include "openssl/des.h"
    #include "../ctaocrypt/include/hmac.h"
    #include "../ctaocrypt/include/random.h"
    #include "../ctaocrypt/include/des3.h"
    #include "../ctaocrypt/include/md4.h"
    #include "../ctaocrypt/include/coding.h"
#endif

#include <stdlib.h>
#ifndef _WIN32
    #include <errno.h>
#endif

#define TRUE  1
#define FALSE 0


#ifndef min

    static INLINE word32 min(word32 a, word32 b)
    {
        return a > b ? b : a;
    }

#endif /* min */



SSL_CTX* SSL_CTX_new(SSL_METHOD* method)
{
    SSL_CTX* ctx = (SSL_CTX*) XMALLOC(sizeof(SSL_CTX), 0);
    if (ctx)
        InitSSL_Ctx(ctx, method);

    return ctx;
}


void SSL_CTX_free(SSL_CTX* ctx)
{
    if (ctx)
        FreeSSL_Ctx(ctx);
}


SSL* SSL_new(SSL_CTX* ctx)
{

    SSL* ssl = (SSL*) XMALLOC(sizeof(SSL), ctx->heap);
    if (ssl)
        if (InitSSL(ssl, ctx) < 0) {
            FreeSSL(ssl);
            ssl = 0;
        }

    return ssl;
}


void SSL_free(SSL* ssl)
{
    CYASSL_ENTER("SSL_free");
    if (ssl)
        FreeSSL(ssl);
    CYASSL_LEAVE("SSL_free", 0);
}


int SSL_set_fd(SSL* ssl, int fd)
{
    ssl->rfd = fd;      /* not used directly to allow IO callbacks */
    ssl->wfd = fd;

    ssl->IOCB_ReadCtx  = &ssl->rfd;
    ssl->IOCB_WriteCtx = &ssl->wfd;

    return SSL_SUCCESS;
}


int SSL_get_fd(const SSL* ssl)
{
    return ssl->rfd;
}


int CyaSSL_negotiate(SSL* ssl)
{
    int err;

    if (ssl->options.side == SERVER_END)
        err = SSL_accept(ssl);
    else
        err = SSL_connect(ssl);

    if (err == SSL_SUCCESS)
        return 0;
    else
        return err;
}


int SSL_write(SSL* ssl, const void* buffer, int sz)
{
    int ret;

    CYASSL_ENTER("SSL_write()");

#ifndef _WIN32
    errno = 0;
#endif

    ret = SendData(ssl, buffer, sz);

    CYASSL_LEAVE("SSL_write()", ret);

    if (ret < 0)
        return SSL_FATAL_ERROR;
    else
        return ret;
}


int SSL_read(SSL* ssl, void* buffer, int sz)
{
    int ret; 

    CYASSL_ENTER("SSL_read()");

#ifndef _WIN32
        errno = 0;
#endif

    ret = ReceiveData(ssl, (byte*)buffer, min(sz, MAX_RECORD_SIZE));

    CYASSL_LEAVE("SSL_read()", ret);

    if (ret < 0)
        return SSL_FATAL_ERROR;
    else
        return ret;
}


int SSL_shutdown(SSL* ssl)
{
    CYASSL_ENTER("SSL_shutdown()");

    if (ssl->options.quietShutdown) {
        CYASSL_MSG("quiet shutdown, no close notify sent"); 
        return 0;
    }

    /* try to send close notify, not an error if can't */
    if (!ssl->options.isClosed && !ssl->options.connReset &&
                                  !ssl->options.sentNotify) {
        ssl->error = SendAlert(ssl, alert_warning, close_notify);
        if (ssl->error < 0) {
            CYASSL_ERROR(ssl->error);
            return SSL_FATAL_ERROR;
        }
        ssl->options.sentNotify = 1;  /* don't send close_notify twice */
    }

    CYASSL_LEAVE("SSL_shutdown()", ssl->error);

    ssl->error = SSL_ERROR_SYSCALL;   /* simulate OpenSSL behavior */

    return 0;
}


int SSL_get_error(SSL* ssl, int dummy)
{
    if (ssl->error == WANT_READ)
        return SSL_ERROR_WANT_READ;         /* convert to OpenSSL type */
    else if (ssl->error == WANT_WRITE)
        return SSL_ERROR_WANT_WRITE;        /* convert to OpenSSL type */
    else if (ssl->error == ZERO_RETURN) 
        return SSL_ERROR_ZERO_RETURN;       /* convert to OpenSSL type */
    return ssl->error;
}


int SSL_want_read(SSL* ssl)
{
    if (ssl->error == WANT_READ)
        return 1;

    return 0;
}


int SSL_want_write(SSL* ssl)
{
    if (ssl->error == WANT_WRITE)
        return 1;

    return 0;
}


char* ERR_error_string(unsigned long errNumber, char* buffer)
{
    static char* msg = "Please supply a buffer for error string";

    if (buffer) {
        SetErrorString(errNumber, buffer);
        return buffer;
    }

    return msg;
}


void ERR_error_string_n(unsigned long e, char* buf, size_t len)
{
    if (len) ERR_error_string(e, buf);
}


void ERR_print_errors_fp(FILE* fp, int err)
{
    char buffer[MAX_ERROR_SZ + 1];

    SetErrorString(err, buffer);
    fprintf(fp, "%s", buffer);
}


int SSL_pending(SSL* ssl)
{
    return ssl->buffers.clearOutputBuffer.length;
}


/* owns der */
static int AddCA(SSL_CTX* ctx, buffer der)
{
    word32      ret;
    DecodedCert cert;
    Signer*     signer = 0;

    InitDecodedCert(&cert, der.buffer, ctx->heap);
    ret = ParseCert(&cert, der.length, CA_TYPE, NO_VERIFY, 0);

    if (ret == 0) {
        /* take over signer parts */
        signer = MakeSigner(ctx->heap);
        if (!signer)
            ret = MEMORY_ERROR;
        else {
            signer->publicKey  = cert.publicKey;
            signer->pubKeySize = cert.pubKeySize;
            signer->name = cert.subjectCN;
            memcpy(signer->hash, cert.subjectHash, SHA_DIGEST_SIZE);

            cert.publicKey = 0;  /* don't free here */
            cert.subjectCN = 0;

            signer->next = ctx->caList;
            ctx->caList  = signer;   /* takes ownership */
        }
    }

    FreeDecodedCert(&cert);
    XFREE(der.buffer, ctx->heap);

    if (ret == 0) return SSL_SUCCESS;
    return ret;
}


#ifndef NO_SESSION_CACHE

    /* basic config gives a cache with 33 sessions, adequate for clients and
       embedded servers

       BIG_SESSION_CACHE allows 1055 sessions, adequate for servers that aren't
       under heavy load, basically allows 200 new sessions per minute

       HUGE_SESSION_CACHE yields 65,791 sessions, for servers under heavy load,
       allows over 13,000 new sessions per minute or over 200 new sessions per
       second
    */
    #ifdef HUGE_SESSION_CACHE
        #define SESSIONS_PER_ROW 11
        #define SESSION_ROWS 5981
    #elif defined(BIG_SESSION_CACHE)
        #define SESSIONS_PER_ROW 5
        #define SESSION_ROWS 211
    #else
        #define SESSIONS_PER_ROW 3
        #define SESSION_ROWS 11
    #endif

    typedef struct SessionRow {
        int nextIdx;                           /* where to place next one   */
        int totalCount;                        /* sessions ever on this row */
        SSL_SESSION Sessions[SESSIONS_PER_ROW];
    } SessionRow;

    static SessionRow SessionCache[SESSION_ROWS];

    /* quiet compiler */
    #ifndef SINGLE_THREADED
        static CyaSSL_Mutex mutex;   /* SessionCache mutex */
    #endif

#endif /* NO_SESSION_CACHE */


#ifndef NO_FILESYSTEM

static int PemToDer(const char* fileName, int type, buffer* der, void* heap,
                    EncryptedInfo* info)
{
    long   begin    = -1;
    long   end      =  0;
    int    foundEnd =  0;
    int    ret      =  0;
    int    pkcs8    =  0;
    /* int    pkcs8Enc =  0;     pkcs8 encrypted, not full support yet */
    word32 sz       =  0;

    char  line[80];
    char  header[80];
    char  footer[80];

    FILE* file;
    byte* tmp = 0;

    if (type == CERT_TYPE) {
        strncpy(header, "-----BEGIN CERTIFICATE-----", sizeof(header));
        strncpy(footer, "-----END CERTIFICATE-----", sizeof(footer));
    } else {
        strncpy(header, "-----BEGIN RSA PRIVATE KEY-----", sizeof(header));
        strncpy(footer, "-----END RSA PRIVATE KEY-----", sizeof(header));
    }

    file = fopen(fileName, "rb");
    if (!file)
        return SSL_BAD_FILE;

    while(fgets(line, sizeof(line), file))
        if (strncmp(header, line, strlen(header)) == 0) {
            begin = ftell(file);
            break;
        }
    
    /* may have pkcs8 */
    if (begin == -1 && type == PRIVATEKEY_TYPE) {
        strncpy(header, "-----BEGIN PRIVATE KEY-----", sizeof(header));
        strncpy(footer, "-----END PRIVATE KEY-----", sizeof(header));
        
        fseek(file, 0, SEEK_SET);
        
        while(fgets(line, sizeof(line), file))
            if (strncmp(header, line, strlen(header)) == 0) {
                begin = ftell(file);
                pkcs8 = 1;
                break;
            }
        
        /* may be encrypted, not full support yet */
        /*
        if (begin == -1) {
            strncpy(header, "-----BEGIN ENCRYPTED PRIVATE KEY-----",
                    sizeof(header));
            strncpy(footer, "-----END ENCRYPTED PRIVATE KEY-----",
                    sizeof(header));
            
            fseek(file, 0, SEEK_SET);
            
            while(fgets(line, sizeof(line), file))
                if (strncmp(header, line, strlen(header)) == 0) {
                    begin = ftell(file);
                    pkcs8Enc = 1;
                    break;
                }
        }
        */
    }

#ifdef OPENSSL_EXTRA

    /* remove encrypted header if there */
    if (fgets(line, sizeof(line), file)) {
        char encHeader[] = "Proc-Type";
        if (strncmp(encHeader, line, strlen(encHeader)) == 0 &&
            fgets(line, sizeof(line), file)) {

            char* start  = strstr(line, "DES");
            char* finish = strstr(line, ",");
            if (!start)
                start = strstr(line, "AES");

            if (!info) return SSL_BAD_FILE;

            if (start && finish && (start < finish)) {
                char* newline = strstr(line, "\r");

                memcpy(info->name, start, finish - start);
                info->name[finish - start] = 0;
                memcpy(info->iv, finish + 1, sizeof(info->iv));

                if (!newline) newline = strstr(line, "\n");
                if (newline && (newline > finish)) {
                    info->ivSz = (word32)(newline - (finish + 1));
                    info->set = 1;
                }
                else
                    return SSL_BAD_FILE;
            }
            else
                return SSL_BAD_FILE;

            fgets(line, sizeof(line), file);   /* get a blank line */
            begin = ftell(file);
        }
    }

#endif /* OPENSSL_EXTRA */

    while(fgets(line, sizeof(line), file))
        if (strncmp(footer, line, strlen(footer)) == 0) {
            foundEnd = 1;
            break;
        }
        else
            end = ftell(file);

    if (begin == -1 || !foundEnd) {
        fclose(file);
        return SSL_BAD_FILE;
    }

    sz = end - begin;
    tmp = (byte*) XMALLOC(sz, heap);
    if (!tmp) {
        fclose(file);
        return MEMORY_ERROR;
    }

    fseek(file, begin, SEEK_SET);
    if (fread(tmp, sz, 1, file) != 1 || 
            (der->buffer = (byte*) XMALLOC(sz, heap)) == 0) {
        XFREE(tmp, heap);
        fclose(file);
        return FREAD_ERROR;
    }
   
    der->length = sz; 
    if (Base64Decode(tmp, sz, der->buffer, &der->length) < 0)
        ret = SSL_BAD_FILE;

    XFREE(tmp, heap);
    fclose(file);
    
    if (ret == 0 && pkcs8)
        return ToTraditional(der->buffer, der->length);
   
    /* not full support yet */
    /*
    if (ret == 0 && pkcs8Enc)
        return ToTraditionalEnc(der->buffer, der->length);
    */

    return ret;
}


static int ProcessFile(SSL_CTX* ctx, const char* file, int format, int type)
{
    buffer der; 
    EncryptedInfo info;

    der.buffer = 0;
    info.set   = 0;

    if (format != SSL_FILETYPE_ASN1 && format != SSL_FILETYPE_PEM)
        return SSL_BAD_FILETYPE;

    if (format == SSL_FILETYPE_PEM) {
        if (PemToDer(file, type == PRIVATEKEY_TYPE ? type : CERT_TYPE, &der,
                     ctx->heap, &info) < 0) {
            XFREE(der.buffer, ctx->heap);
            return SSL_BAD_FILE;
        }
    }
    else {  /* ASN1 (DER) */
        long   sz;
        FILE*  input = fopen(file, "rb");

        if (!input)
            return SSL_BAD_FILE;
        
        fseek(input, 0, SEEK_END);
        sz = ftell(input);
        fseek(input, 0, SEEK_SET);

        der.buffer = (byte*) XMALLOC(sz, ctx->heap);
        if (!der.buffer) return MEMORY_ERROR;
        der.length = sz;
        sz = (word32)fread(der.buffer, sz, 1, input);
        if (sz != 1) {
            fclose(input);
            XFREE(der.buffer, ctx->heap);
            return SSL_BAD_FILE;
        }
        fclose(input);
    }

#ifdef OPENSSL_EXTRA

    if (info.set) {
        /* decrypt */
        char password[80];
        int  passwordSz;

        byte key[AES_256_KEY_SIZE];
        byte  iv[AES_IV_SIZE];

        if (!ctx->passwd_cb) return -1;

        /* use file's salt for key derivation, hex decode first */
        if (Base16Decode(info.iv, info.ivSz, info.iv, &info.ivSz) != 0)
            return -1;

        passwordSz = ctx->passwd_cb(password, sizeof(password), 0,
                                    ctx->userdata);
        if (EVP_BytesToKey(info.name, "MD5", info.iv, (byte*)password,
                           passwordSz, 1, key, iv) <= 0)
            return -1;

        if (strncmp(info.name, "DES-CBC", 7) == 0) {
            Des des;
            Des_SetKey(&des, key, info.iv, DES_DECRYPTION);
            Des_CbcDecrypt(&des, der.buffer, der.buffer, der.length);
        }
        else if (strncmp(info.name, "DES-EDE3-CBC", 13) == 0) {
            Des3 des;
            Des3_SetKey(&des, key, info.iv, DES_DECRYPTION);
            Des3_CbcDecrypt(&des, der.buffer, der.buffer, der.length);
        }
        else if (strncmp(info.name, "AES-128-CBC", 13) == 0) {
            Aes aes;
            AesSetKey(&aes, key, AES_128_KEY_SIZE, info.iv, AES_DECRYPTION);
            AesCbcDecrypt(&aes, der.buffer, der.buffer, der.length);
        }
        else if (strncmp(info.name, "AES-192-CBC", 13) == 0) {
            Aes aes;
            AesSetKey(&aes, key, AES_192_KEY_SIZE, info.iv, AES_DECRYPTION);
            AesCbcDecrypt(&aes, der.buffer, der.buffer, der.length);
        }
        else if (strncmp(info.name, "AES-256-CBC", 13) == 0) {
            Aes aes;
            AesSetKey(&aes, key, AES_256_KEY_SIZE, info.iv, AES_DECRYPTION);
            AesCbcDecrypt(&aes, der.buffer, der.buffer, der.length);
        }
        else 
            return SSL_BAD_FILE;
    }

#endif /* OPENSSL_EXTRA */

    if (type == CA_TYPE)
        return AddCA(ctx, der);     /* takes der over */
    else if (type == CERT_TYPE)
        ctx->certificate = der;     /* takes der over */
    else if (type == PRIVATEKEY_TYPE)
        ctx->privateKey = der;      /* takes der over */
    else {
        XFREE(der.buffer, ctx->heap);
        return SSL_BAD_CERTTYPE;
    }
    
    if (type == PRIVATEKEY_TYPE) {
        /* make sure key can be used */
        RsaKey key;
        word32 idx = 0;
        
        InitRsaKey(&key, 0);
        if (RsaPrivateKeyDecode(der.buffer, &idx, &key, der.length) != 0) {
            FreeRsaKey(&key);
            return SSL_BAD_FILE;
        }
        
        FreeRsaKey(&key);
    }

    return SSL_SUCCESS;
}

/* just one for now TODO: add dir support from path */
int SSL_CTX_load_verify_locations(SSL_CTX* ctx, const char* file,
                                  const char* path)
{
    if (ProcessFile(ctx, file, SSL_FILETYPE_PEM, CA_TYPE) == SSL_SUCCESS)
        return SSL_SUCCESS;

    return SSL_FAILURE;
}


int SSL_CTX_use_certificate_file(SSL_CTX* ctx, const char* file, int type)
{
    if (ProcessFile(ctx, file, type, CERT_TYPE) == SSL_SUCCESS)
        return SSL_SUCCESS;

    return SSL_FAILURE;
}


int SSL_CTX_use_PrivateKey_file(SSL_CTX* ctx, const char* file, int type)
{
    if (ProcessFile(ctx, file, type, PRIVATEKEY_TYPE) == SSL_SUCCESS)
        return SSL_SUCCESS;

    return SSL_FAILURE;
}


int SSL_CTX_use_certificate_chain_file(SSL_CTX* ctx, const char* file)
{
    /* add first to ctx, all tested implementations support this */
   if (ProcessFile(ctx, file, SSL_FILETYPE_PEM, CERT_TYPE) == SSL_SUCCESS)
       return SSL_SUCCESS;

   return SSL_FAILURE;
}

#endif /* NO_FILESYSTEM */


void SSL_CTX_set_verify(SSL_CTX* ctx, int mode, VerifyCallback vc)
{
    if (mode & SSL_VERIFY_PEER) {
        ctx->verifyPeer = 1;
        ctx->verifyNone = 0;  /* in case perviously set */
    }

    if (mode == SSL_VERIFY_NONE) {
        ctx->verifyNone = 1;
        ctx->verifyPeer = 0;  /* in case previously set */
    }

    if (mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT)
        ctx->failNoCert = 1;
}


#ifndef NO_SESSION_CACHE

SSL_SESSION* SSL_get_session(SSL* ssl)
{
    return GetSession(ssl, 0);
}


int SSL_set_session(SSL* ssl, SSL_SESSION* session)
{
    if (session)
        return SetSession(ssl, session);

    return SSL_FAILURE;
}

#endif /* NO_SESSION_CACHE */


void SSL_load_error_strings(void)   /* compatibility only */
{}


int SSL_library_init(void)
{
    InitCyaSSL();
    return SSL_SUCCESS;
}


#ifndef NO_SESSION_CACHE

/* on by default if built in but allow user to turn off */
long SSL_CTX_set_session_cache_mode(SSL_CTX* ctx, long mode)
{
    if (mode == SSL_SESS_CACHE_OFF)
        ctx->sessionCacheOff = 1;

    if (mode == SSL_SESS_CACHE_NO_AUTO_CLEAR)
        ctx->sessionCacheFlushOff = 1;

    return SSL_SUCCESS;
}

#endif /* NO_SESSION_CACHE */


int SSL_CTX_set_cipher_list(SSL_CTX* ctx, const char* list)
{
    if (SetCipherList(ctx, list))
        return SSL_SUCCESS;
    else
        return SSL_FAILURE;
}


/* client only parts */
#ifndef NO_CYASSL_CLIENT

    SSL_METHOD* SSLv3_client_method(void)
    {
        SSL_METHOD* method = (SSL_METHOD*) XMALLOC(sizeof(SSL_METHOD), 0);
        if (method)
            InitSSL_Method(method, MakeSSLv3());
        return method;
    }

    #ifdef CYASSL_DTLS
        SSL_METHOD* DTLSv1_client_method(void)
        {
            SSL_METHOD* method = (SSL_METHOD*) XMALLOC(sizeof(SSL_METHOD), 0);
            if (method)
                InitSSL_Method(method, MakeDTLSv1());
            return method;
        }
    #endif


    /* please see note at top of README if you get an error from connect */
    int SSL_connect(SSL* ssl)
    {
        int neededState;

        CYASSL_ENTER("SSL_connect()");

        #ifndef _WIN32
            errno = 0;
        #endif

        if (ssl->options.side != CLIENT_END) {
            CYASSL_ERROR(ssl->error = SIDE_ERROR);
            return SSL_FATAL_ERROR;
        }

        #ifdef CYASSL_DTLS
            if (ssl->version.major == DTLS_MAJOR && 
                                      ssl->version.minor == DTLS_MINOR) {
                ssl->options.dtls   = 1;
                ssl->options.tls    = 1;
                ssl->options.tls1_1 = 1;
            }
        #endif

        if (ssl->buffers.outputBuffer.length > 0) {
            if ( (ssl->error = SendBuffered(ssl)) == 0) {
                ssl->options.connectState++;
                CYASSL_MSG("connect state: Advanced from buffered send");
            }
            else {
                CYASSL_ERROR(ssl->error);
                return SSL_FATAL_ERROR;
            }
        }

        switch (ssl->options.connectState) {

        case CONNECT_BEGIN :
            /* always send client hello first */
            if ( (ssl->error = SendClientHello(ssl)) != 0) {
                CYASSL_ERROR(ssl->error);
                return SSL_FATAL_ERROR;
            }
            ssl->options.connectState = CLIENT_HELLO_SENT;
            CYASSL_MSG("connect state: CLIENT_HELLO_SENT");

        case CLIENT_HELLO_SENT :
            neededState = ssl->options.resuming ? SERVER_FINISHED_COMPLETE :
                                          SERVER_HELLODONE_COMPLETE;
            #ifdef CYASSL_DTLS
                if (ssl->options.dtls && !ssl->options.resuming)
                    neededState = SERVER_HELLOVERIFYREQUEST_COMPLETE;
            #endif
            /* get response */
            while (ssl->options.serverState < neededState) {
                if ( (ssl->error = ProcessReply(ssl)) < 0) {
                    CYASSL_ERROR(ssl->error);
                    return SSL_FATAL_ERROR;
                }
                /* if resumption failed, reset needed state */
                else if (neededState == SERVER_FINISHED_COMPLETE)
                    if (!ssl->options.resuming) {
                        if (!ssl->options.dtls)
                            neededState = SERVER_HELLODONE_COMPLETE;
                        else
                            neededState = SERVER_HELLOVERIFYREQUEST_COMPLETE;
                    }
            }

            ssl->options.connectState = HELLO_AGAIN;
            CYASSL_MSG("connect state: HELLO_AGAIN");

        case HELLO_AGAIN :
            #ifdef CYASSL_DTLS
                if (ssl->options.dtls && !ssl->options.resuming) {
                    /* re-init hashes, exclude first hello and verify request */
                    InitMd5(&ssl->hashMd5);
                    InitSha(&ssl->hashSha);
                    if ( (ssl->error = SendClientHello(ssl)) != 0) {
                        CYASSL_ERROR(ssl->error);
                        return SSL_FATAL_ERROR;
                    }
                }
            #endif

            ssl->options.connectState = HELLO_AGAIN_REPLY;
            CYASSL_MSG("connect state: HELLO_AGAIN_REPLY");

        case HELLO_AGAIN_REPLY :
            #ifdef CYASSL_DTLS
                if (ssl->options.dtls) {
                    neededState = ssl->options.resuming ?
                           SERVER_FINISHED_COMPLETE : SERVER_HELLODONE_COMPLETE;
            
                    /* get response */
                    while (ssl->options.serverState < neededState) {
                        if ( (ssl->error = ProcessReply(ssl)) < 0) {
                                CYASSL_ERROR(ssl->error);
                                return SSL_FATAL_ERROR;
                        }
                        /* if resumption failed, reset needed state */
                        else if (neededState == SERVER_FINISHED_COMPLETE)
                            if (!ssl->options.resuming)
                                neededState = SERVER_HELLODONE_COMPLETE;
                    }
                }
            #endif

            ssl->options.connectState = FIRST_REPLY_DONE;
            CYASSL_MSG("connect state: FIRST_REPLY_DONE");

        case FIRST_REPLY_DONE :
            if (ssl->options.sendVerify)
                if ( (ssl->error = SendCertificate(ssl)) != 0) {
                    CYASSL_ERROR(ssl->error);
                    return SSL_FATAL_ERROR;
                }

            ssl->options.connectState = FIRST_REPLY_FIRST;
            CYASSL_MSG("connect state: FIRST_REPLY_FIRST");

        case FIRST_REPLY_FIRST :
            if (!ssl->options.resuming)
                if ( (ssl->error = SendClientKeyExchange(ssl)) != 0) {
                    CYASSL_ERROR(ssl->error);
                    return SSL_FATAL_ERROR;
                }

            ssl->options.connectState = FIRST_REPLY_SECOND;
            CYASSL_MSG("connect state: FIRST_REPLY_SECOND");

        case FIRST_REPLY_SECOND :
            if (ssl->options.sendVerify)
                if ( (ssl->error = SendCertificateVerify(ssl)) != 0) {
                    CYASSL_ERROR(ssl->error);
                    return SSL_FATAL_ERROR;
            }
            ssl->options.connectState = FIRST_REPLY_THIRD;
            CYASSL_MSG("connect state: FIRST_REPLY_THIRD");

        case FIRST_REPLY_THIRD :
            if ( (ssl->error = SendChangeCipher(ssl)) != 0) {
                CYASSL_ERROR(ssl->error);
                return SSL_FATAL_ERROR;
            }
            ssl->options.connectState = FIRST_REPLY_FOURTH;
            CYASSL_MSG("connect state: FIRST_REPLY_FOURTH");

        case FIRST_REPLY_FOURTH :
            if ( (ssl->error = SendFinished(ssl)) != 0) {
                CYASSL_ERROR(ssl->error);
                return SSL_FATAL_ERROR;
            }

            ssl->options.connectState = FINISHED_DONE;
            CYASSL_MSG("connect state: FINISHED_DONE");

        case FINISHED_DONE :
            /* get response */
            while (ssl->options.serverState < SERVER_FINISHED_COMPLETE)
                if ( (ssl->error = ProcessReply(ssl)) < 0) {
                    CYASSL_ERROR(ssl->error);
                    return SSL_FATAL_ERROR;
                }
          
            ssl->options.connectState = SECOND_REPLY_DONE;
            CYASSL_MSG("connect state: SECOND_REPLY_DONE");

        case SECOND_REPLY_DONE:
            CYASSL_LEAVE("SSL_connect()", SSL_SUCCESS);
            return SSL_SUCCESS;

        default:
            CYASSL_MSG("Unknown connect state ERROR");
            return SSL_FATAL_ERROR; /* unknown connect state */
        }
    }

#endif /* NO_CYASSL_CLIENT */


/* server only parts */
#ifndef NO_CYASSL_SERVER

    SSL_METHOD* SSLv3_server_method(void)
    {
        SSL_METHOD* method = (SSL_METHOD*) XMALLOC(sizeof(SSL_METHOD), 0);
        if (method) {
            InitSSL_Method(method, MakeSSLv3());
            method->side = SERVER_END;
        }
        return method;
    }


    #ifdef CYASSL_DTLS
        SSL_METHOD* DTLSv1_server_method(void)
        {
            SSL_METHOD* method = (SSL_METHOD*) XMALLOC(sizeof(SSL_METHOD), 0);
            if (method) {
                InitSSL_Method(method, MakeDTLSv1());
                method->side = SERVER_END;
            }
            return method;
        }
    #endif


    int SSL_accept(SSL* ssl)
    {
        CYASSL_ENTER("SSL_accept()");

        #ifndef _WIN32
            errno = 0;
        #endif

        if (ssl->options.side != SERVER_END) {
            CYASSL_ERROR(ssl->error = SIDE_ERROR);
            return SSL_FATAL_ERROR;
        }

        #ifdef CYASSL_DTLS
            if (ssl->version.major == DTLS_MAJOR &&
                                      ssl->version.minor == DTLS_MINOR) {
                ssl->options.dtls   = 1;
                ssl->options.tls    = 1;
                ssl->options.tls1_1 = 1;
            }
        #endif

        if (ssl->buffers.outputBuffer.length > 0) {
            if ( (ssl->error = SendBuffered(ssl)) == 0) {
                ssl->options.connectState++;
                CYASSL_MSG("accept state: Advanced from buffered send");
            }
            else {
                CYASSL_ERROR(ssl->error);
                return SSL_FATAL_ERROR;
            }
        }

        switch (ssl->options.acceptState) {
    
        case ACCEPT_BEGIN :
            /* get response */
            while (ssl->options.clientState < CLIENT_HELLO_COMPLETE)
                if ( (ssl->error = ProcessReply(ssl)) < 0) {
                    CYASSL_ERROR(ssl->error);
                    return SSL_FATAL_ERROR;
                }
            ssl->options.acceptState = ACCEPT_CLIENT_HELLO_DONE;
            CYASSL_MSG("accept state ACCEPT_CLIENT_HELLO_DONE");

        case ACCEPT_CLIENT_HELLO_DONE :
            #ifdef CYASSL_DTLS
                if (ssl->options.dtls && !ssl->options.resuming)
                    if ( (ssl->error = SendHelloVerifyRequest(ssl)) != 0) {
                        CYASSL_ERROR(ssl->error);
                        return SSL_FATAL_ERROR;
                    }
            #endif
            ssl->options.acceptState = HELLO_VERIFY_SENT;
            CYASSL_MSG("accept state HELLO_VERIFY_SENT");

        case HELLO_VERIFY_SENT:
            #ifdef CYASSL_DTLS
                if (ssl->options.dtls && !ssl->options.resuming) {
                    ssl->options.clientState = NULL_STATE;  /* get again */
                    /* re-init hashes, exclude first hello and verify request */
                    InitMd5(&ssl->hashMd5);
                    InitSha(&ssl->hashSha);

                    while (ssl->options.clientState < CLIENT_HELLO_COMPLETE)
                        if ( (ssl->error = ProcessReply(ssl)) < 0) {
                            CYASSL_ERROR(ssl->error);
                            return SSL_FATAL_ERROR;
                        }
                }
            #endif
            ssl->options.acceptState = ACCEPT_FIRST_REPLY_DONE;
            CYASSL_MSG("accept state ACCEPT_FIRST_REPLY_DONE");

        case ACCEPT_FIRST_REPLY_DONE :
            if ( (ssl->error = SendServerHello(ssl)) != 0) {
                CYASSL_ERROR(ssl->error);
                return SSL_FATAL_ERROR;
            }
            ssl->options.acceptState = SERVER_HELLO_SENT;
            CYASSL_MSG("accept state SERVER_HELLO_SENT");

        case SERVER_HELLO_SENT :
            if (!ssl->options.resuming) 
                if ( (ssl->error = SendCertificate(ssl)) != 0) {
                    CYASSL_ERROR(ssl->error);
                    return SSL_FATAL_ERROR;
                }
            ssl->options.acceptState = CERT_SENT;
            CYASSL_MSG("accept state CERT_SENT");

        case CERT_SENT :
            if (!ssl->options.resuming) 
                if ( (ssl->error = SendServerKeyExchange(ssl)) != 0) {
                    CYASSL_ERROR(ssl->error);
                    return SSL_FATAL_ERROR;
                }
            ssl->options.acceptState = KEY_EXCHANGE_SENT;
            CYASSL_MSG("accept state KEY_EXCHANGE_SENT");

        case KEY_EXCHANGE_SENT :
            if (!ssl->options.resuming) 
                if (ssl->options.verifyPeer)
                    if ( (ssl->error = SendCertificateRequest(ssl)) != 0) {
                        CYASSL_ERROR(ssl->error);
                        return SSL_FATAL_ERROR;
                    }
            ssl->options.acceptState = CERT_REQ_SENT;
            CYASSL_MSG("accept state CERT_REQ_SENT");

        case CERT_REQ_SENT :
            if (!ssl->options.resuming) 
                if ( (ssl->error = SendServerHelloDone(ssl)) != 0) {
                    CYASSL_ERROR(ssl->error);
                    return SSL_FATAL_ERROR;
                }
            ssl->options.acceptState = SERVER_HELLO_DONE;
            CYASSL_MSG("accept state SERVER_HELLO_DONE");

        case SERVER_HELLO_DONE :
            if (!ssl->options.resuming) {
                while (ssl->options.clientState < CLIENT_FINISHED_COMPLETE)
                    if ( (ssl->error = ProcessReply(ssl)) < 0) {
                        CYASSL_ERROR(ssl->error);
                        return SSL_FATAL_ERROR;
                    }
            }
            ssl->options.acceptState = ACCEPT_SECOND_REPLY_DONE;
            CYASSL_MSG("accept state  ACCEPT_SECOND_REPLY_DONE");
          
        case ACCEPT_SECOND_REPLY_DONE : 
            if ( (ssl->error = SendChangeCipher(ssl)) != 0) {
                CYASSL_ERROR(ssl->error);
                return SSL_FATAL_ERROR;
            }
            ssl->options.acceptState = CHANGE_CIPHER_SENT;
            CYASSL_MSG("accept state  CHANGE_CIPHER_SENT");

        case CHANGE_CIPHER_SENT : 
            if ( (ssl->error = SendFinished(ssl)) != 0) {
                CYASSL_ERROR(ssl->error);
                return SSL_FATAL_ERROR;
            }

            ssl->options.acceptState = ACCEPT_FINISHED_DONE;
            CYASSL_MSG("accept state ACCEPT_FINISHED_DONE");

        case ACCEPT_FINISHED_DONE :
            if (ssl->options.resuming)
                while (ssl->options.clientState < CLIENT_FINISHED_COMPLETE)
                    if ( (ssl->error = ProcessReply(ssl)) < 0) {
                        CYASSL_ERROR(ssl->error);
                        return SSL_FATAL_ERROR;
                    }

            ssl->options.acceptState = ACCEPT_THIRD_REPLY_DONE;
            CYASSL_MSG("accept state ACCEPT_THIRD_REPLY_DONE");

        case ACCEPT_THIRD_REPLY_DONE :
            CYASSL_LEAVE("SSL_accept()", SSL_SUCCESS);
            return SSL_SUCCESS;

        default :
            CYASSL_MSG("Unknown accept state ERROR");
            return SSL_FATAL_ERROR;
        }
    }

#endif /* NO_CYASSL_SERVER */


void InitCyaSSL(void)
{
    InitMutex(&mutex);
}


void FreeCyaSSL()
{
    FreeMutex(&mutex);
}


#ifndef NO_SESSION_CACHE


static INLINE word32 HashSession(const byte* sessionID)
{
    /* id is random, just make 32 bit number from first 4 bytes for now */
    return (sessionID[0] << 24) | (sessionID[1] << 16) | (sessionID[2] <<  8) |
            sessionID[3];
}


void SSL_flush_sessions(SSL_CTX* ctx, long tm)
{
    /* static table now, no flusing needed */
}


SSL_SESSION* GetSession(SSL* ssl, byte* masterSecret)
{
    SSL_SESSION* ret = 0;
    const byte*  id = ssl->arrays.sessionID;
    word32       row;
    int          idx;
    
    if (ssl->options.sessionCacheOff)
        return 0;

    row = HashSession(id) % SESSION_ROWS;

    LockMutex(&mutex);
   
    if (SessionCache[row].totalCount >= SESSIONS_PER_ROW)
        idx = SESSIONS_PER_ROW - 1;
    else
        idx = SessionCache[row].nextIdx - 1;

    for (; idx >= 0; idx--) {
        SSL_SESSION* current;
        
        if (idx >= SESSIONS_PER_ROW)    /* server could have restarted, idx  */
            break;                      /* would be word32(-1) and seg fault */
        
        current = &SessionCache[row].Sessions[idx];
        if (memcmp(current->sessionID, id, ID_LEN) == 0) {
            if (LowResTimer() < (current->bornOn + current->timeout)) {
                ret = current;
                if (masterSecret)
                    memcpy(masterSecret, current->masterSecret, SECRET_LEN);
            }
            break;
        }   
    }

    UnLockMutex(&mutex);
    
    return ret;
}


int SetSession(SSL* ssl, SSL_SESSION* session)
{
    if (ssl->options.sessionCacheOff)
        return SSL_FAILURE;

    if (LowResTimer() < (session->bornOn + session->timeout)) {
        ssl->session  = *session;
        ssl->options.resuming = 1;

#ifdef SESSION_CERTS
        ssl->version             = session->version;
        ssl->options.cipherSuite = session->cipherSuite;
#endif

        return SSL_SUCCESS;
    }
    return SSL_FAILURE;  /* session timed out */
}


void AddSession(SSL* ssl)
{
    word32 row, idx;

    if (ssl->options.sessionCacheOff)
        return;

    row = HashSession(ssl->arrays.sessionID) % SESSION_ROWS;

    LockMutex(&mutex);

    idx = SessionCache[row].nextIdx++;

    memcpy(SessionCache[row].Sessions[idx].masterSecret,
           ssl->arrays.masterSecret, SECRET_LEN);
    memcpy(SessionCache[row].Sessions[idx].sessionID, ssl->arrays.sessionID,
           ID_LEN);

    SessionCache[row].Sessions[idx].timeout = DEFAULT_TIMEOUT;
    SessionCache[row].Sessions[idx].bornOn  = LowResTimer();

#ifdef SESSION_CERTS
    SessionCache[row].Sessions[idx].chain.count = ssl->session.chain.count;
    memcpy(SessionCache[row].Sessions[idx].chain.certs,
           ssl->session.chain.certs, sizeof(x509_buffer) * MAX_CHAIN_DEPTH);

    SessionCache[row].Sessions[idx].version     = ssl->version;
    SessionCache[row].Sessions[idx].cipherSuite = ssl->options.cipherSuite;
#endif

    SessionCache[row].totalCount++;
    if (SessionCache[row].nextIdx == SESSIONS_PER_ROW)
        SessionCache[row].nextIdx = 0;

    UnLockMutex(&mutex);        
}


    #ifdef SESSION_STATS

    void PrintSessionStats(void)
    {
        word32 totalSessionsSeen = 0;
        word32 totalSessionsNow = 0;
        word32 rowNow;
        int    i;
        double E;               /* expected freq */
        double chiSquare = 0;
        
        for (i = 0; i < SESSION_ROWS; i++) {
            totalSessionsSeen += SessionCache[i].totalCount;

            if (SessionCache[i].totalCount >= SESSIONS_PER_ROW)
                rowNow = SESSIONS_PER_ROW;
            else if (SessionCache[i].nextIdx == 0)
                rowNow = 0;
            else
                rowNow = SessionCache[i].nextIdx;
        
            totalSessionsNow += rowNow;
        }

        printf("Total Sessions Seen = %d\n", totalSessionsSeen);
        printf("Total Sessions Now  = %d\n", totalSessionsNow);

        E = (double)totalSessionsSeen / SESSION_ROWS;

        for (i = 0; i < SESSION_ROWS; i++) {
            double diff = SessionCache[i].totalCount - E;
            diff *= diff;                /* sqaure    */
            diff /= E;                   /* normalize */

            chiSquare += diff;
        }
        printf("  chi-square = %5.1f, d.f. = %d\n", chiSquare,
                                                     SESSION_ROWS - 1);
        if (SESSION_ROWS == 11)
            printf(" .05 p value =  18.3, chi-square should be less\n");
        else if (SESSION_ROWS == 211)
            printf(".05 p value  = 244.8, chi-square should be less\n");
        else if (SESSION_ROWS == 5981)
            printf(".05 p value  = 6161.0, chi-square should be less\n");
        printf("\n");
    }

    #endif /* SESSION_STATS */

#endif /* NO_SESSION_CACHE */


/* call before SSL_connect, if verifying will add name check to
   date check and signature check */
int CyaSSL_check_domain_name(SSL* ssl, const char* dn)
{
    if (ssl->buffers.domainName.buffer)
        XFREE(ssl->buffers.domainName.buffer, ssl->heap);

    ssl->buffers.domainName.length = (word32)strlen(dn) + 1;
    ssl->buffers.domainName.buffer =
                     (byte*) XMALLOC(ssl->buffers.domainName.length, ssl->heap);

    if (ssl->buffers.domainName.buffer) {
        strncpy((char*)ssl->buffers.domainName.buffer, dn,
                ssl->buffers.domainName.length);
        return SSL_SUCCESS;
    }
    else {
        ssl->error = MEMORY_ERROR;
        return SSL_FAILURE;
    }
}


/* turn on CyaSSL zlib compression
   returns 0 for success, else error (not built in)
*/
int CyaSSL_set_compression(SSL* ssl)
{
#ifdef HAVE_LIBZ
    ssl->options.usingCompression = 1;
    return 0;
#else
    return -1;
#endif
}


#ifndef _WIN32
    #ifndef NO_WRITEV

        /* simulate writev semantics, doesn't actually do block at a time though
           because of SSL_write behavior and because front adds may be small */
        int CyaSSL_writev(SSL* ssl, const struct iovec* iov, int iovcnt)
        {
            byte  tmp[MAX_RECORD_SIZE];
            byte* buffer    = tmp;
            int   send      = 0;
            int   newBuffer = 0;
            int   idx       = 0;
            int   i;
            int   ret;

            for (i = 0; i < iovcnt; i++)
                send += iov[i].iov_len;

            if (send > sizeof(tmp)) {
                byte* tmp2 = (byte*) XMALLOC(send, ssl->heap);
                if (!tmp2)
                    return MEMORY_ERROR;
                buffer = tmp2;
                newBuffer = 1;
            }

            for (i = 0; i < iovcnt; i++) {
                memcpy(&buffer[idx], iov[i].iov_base, iov[i].iov_len);
                idx += iov[i].iov_len;
            }

            ret = SSL_write(ssl, buffer, send);

            if (newBuffer) XFREE(buffer, ssl->heap);

            return ret;
        }
    #endif
#endif


#ifdef CYASSL_CALLBACKS

    typedef struct itimerval Itimerval;

    /* don't keep calling simple functions while setting up timer and singals
       if no inlining these are the next best */

    #define AddTimes(a, b, c)                       \
        do {                                        \
            c.tv_sec  = a.tv_sec  + b.tv_sec;       \
            c.tv_usec = a.tv_usec + b.tv_usec;      \
            if (c.tv_sec >=  1000000) {             \
                c.tv_sec++;                         \
                c.tv_usec -= 1000000;               \
            }                                       \
        } while (0)


    #define SubtractTimes(a, b, c)                  \
        do {                                        \
            c.tv_sec  = a.tv_sec  - b.tv_sec;       \
            c.tv_usec = a.tv_usec - b.tv_usec;      \
            if (c.tv_sec < 0) {                     \
                c.tv_sec--;                         \
                c.tv_usec += 1000000;               \
            }                                       \
        } while (0)

    #define CmpTimes(a, b, cmp)                     \
        ((a.tv_sec  ==  b.tv_sec) ?                 \
            (a.tv_usec cmp b.tv_usec) :             \
            (a.tv_sec  cmp b.tv_sec))               \


    /* do nothing handler */
    static void myHandler(int signo)
    {
        return;
    }


    static int CyaSSL_ex_wrapper(SSL* ssl, HandShakeCallBack hsCb,
                                 TimeoutCallBack toCb, Timeval timeout)
    {
        int       ret        = -1;
        int       oldTimerOn = 0;   /* was timer already on */
        Timeval   startTime;
        Timeval   endTime;
        Timeval   totalTime;
        Itimerval myTimeout;
        Itimerval oldTimeout; /* if old timer adjust from total time to reset */
        struct sigaction act, oact;
       
        #define ERR_OUT(x) { ssl->hsInfoOn = 0; ssl->toInfoOn = 0; return x; }

        if (hsCb) {
            ssl->hsInfoOn = 1;
            InitHandShakeInfo(&ssl->handShakeInfo);
        }
        if (toCb) {
            ssl->toInfoOn = 1;
            InitTimeoutInfo(&ssl->timeoutInfo);
            
            if (gettimeofday(&startTime, 0) < 0)
                ERR_OUT(GETTIME_ERROR);

            /* use setitimer to simulate getitimer, init 0 myTimeout */
            myTimeout.it_interval.tv_sec  = 0;
            myTimeout.it_interval.tv_usec = 0;
            myTimeout.it_value.tv_sec     = 0;
            myTimeout.it_value.tv_usec    = 0;
            if (setitimer(ITIMER_REAL, &myTimeout, &oldTimeout) < 0)
                ERR_OUT(SETITIMER_ERROR);

            if (oldTimeout.it_value.tv_sec || oldTimeout.it_value.tv_usec) {
                oldTimerOn = 1;
                
                /* is old timer going to expire before ours */
                if (CmpTimes(oldTimeout.it_value, timeout, <)) { 
                    timeout.tv_sec  = oldTimeout.it_value.tv_sec;
                    timeout.tv_usec = oldTimeout.it_value.tv_usec;
                }       
            }
            myTimeout.it_value.tv_sec  = timeout.tv_sec;
            myTimeout.it_value.tv_usec = timeout.tv_usec;
            
            /* set up signal handler, don't restart socket send/recv */
            act.sa_handler = myHandler;
            sigemptyset(&act.sa_mask);
            act.sa_flags = 0;
#ifdef SA_INTERRUPT
            act.sa_flags |= SA_INTERRUPT;
#endif
            if (sigaction(SIGALRM, &act, &oact) < 0)
                ERR_OUT(SIGACT_ERROR);

            if (setitimer(ITIMER_REAL, &myTimeout, 0) < 0)
                ERR_OUT(SETITIMER_ERROR);
        }

        /* do main work */
#ifndef NO_CYASSL_CLIENT
        if (ssl->options.side == CLIENT_END)
            ret = SSL_connect(ssl);
#endif
#ifndef NO_CYASSL_SERVER
        if (ssl->options.side == SERVER_END)
            ret = SSL_accept(ssl);
#endif
       
        /* do callbacks */ 
        if (toCb) {
            if (oldTimerOn) {
                gettimeofday(&endTime, 0);
                SubtractTimes(endTime, startTime, totalTime);
                /* adjust old timer for elapsed time */
                if (CmpTimes(totalTime, oldTimeout.it_value, <))
                    SubtractTimes(oldTimeout.it_value, totalTime,
                                  oldTimeout.it_value);
                else {
                    /* reset value to interval, may be off */
                    oldTimeout.it_value.tv_sec = oldTimeout.it_interval.tv_sec;
                    oldTimeout.it_value.tv_usec =oldTimeout.it_interval.tv_usec;
                }
                /* keep iter the same whether there or not */
            }
            /* restore old handler */
            if (sigaction(SIGALRM, &oact, 0) < 0)
                ret = SIGACT_ERROR;    /* more pressing error, stomp */
            else
                /* use old settings which may turn off (expired or not there) */
                if (setitimer(ITIMER_REAL, &oldTimeout, 0) < 0)
                    ret = SETITIMER_ERROR;
            
            /* if we had a timeout call callback */
            if (ssl->timeoutInfo.timeoutName[0]) {
                ssl->timeoutInfo.timeoutValue.tv_sec  = timeout.tv_sec;
                ssl->timeoutInfo.timeoutValue.tv_usec = timeout.tv_usec;
                (toCb)(&ssl->timeoutInfo);
            }
            /* clean up */
            FreeTimeoutInfo(&ssl->timeoutInfo, ssl->heap);
            ssl->toInfoOn = 0;
        }
        if (hsCb) {
            FinishHandShakeInfo(&ssl->handShakeInfo, ssl);
            (hsCb)(&ssl->handShakeInfo);
            ssl->hsInfoOn = 0;
        }
        return ret;
    }


#ifndef NO_CYASSL_CLIENT

    int CyaSSL_connect_ex(SSL* ssl, HandShakeCallBack hsCb,
                          TimeoutCallBack toCb, Timeval timeout)
    {
        return CyaSSL_ex_wrapper(ssl, hsCb, toCb, timeout);
    }

#endif


#ifndef NO_CYASSL_SERVER

    int CyaSSL_accept_ex(SSL* ssl, HandShakeCallBack hsCb,
                         TimeoutCallBack toCb,Timeval timeout)
    {
        return CyaSSL_ex_wrapper(ssl, hsCb, toCb, timeout);
    }

#endif

#endif /* CYASSL_CALLBACKS */


#ifndef NO_PSK

    void SSL_CTX_set_psk_client_callback(SSL_CTX* ctx, psk_client_callback cb)
    {
        ctx->havePSK = 1;
        ctx->client_psk_cb = cb;
    }


    void SSL_set_psk_client_callback(SSL* ssl, psk_client_callback cb)
    {
        ssl->options.havePSK = 1;
        ssl->options.client_psk_cb = cb;

        InitSuites(&ssl->suites, ssl->version, TRUE, TRUE);
    }


    void SSL_CTX_set_psk_server_callback(SSL_CTX* ctx, psk_server_callback cb)
    {
        ctx->havePSK = 1;
        ctx->server_psk_cb = cb;
    }


    void SSL_set_psk_server_callback(SSL* ssl, psk_server_callback cb)
    {
        ssl->options.havePSK = 1;
        ssl->options.server_psk_cb = cb;

        InitSuites(&ssl->suites, ssl->version, ssl->options.haveDH, TRUE);
    }


    const char* SSL_get_psk_identity_hint(const SSL* ssl)
    {
        return ssl->arrays.server_hint;
    }


    const char* SSL_get_psk_identity(const SSL* ssl)
    {
        return ssl->arrays.client_identity;
    }


    int SSL_CTX_use_psk_identity_hint(SSL_CTX* ctx, const char* hint)
    {
        if (hint == 0)
            ctx->server_hint[0] = 0;
        else
            strncpy(ctx->server_hint, hint, MAX_PSK_ID_LEN);
        return SSL_SUCCESS;
    }


    int SSL_use_psk_identity_hint(SSL* ssl, const char* hint)
    {
        if (hint == 0)
            ssl->arrays.server_hint[0] = 0;
        else
            strncpy(ssl->arrays.server_hint, hint, MAX_PSK_ID_LEN);
        return SSL_SUCCESS;
    }

#endif /* NO_PSK */


#ifdef NO_FILESYSTEM


    static int PemToDerBuffer(const unsigned char* buff, long sz, int type,
                              buffer* der, void* heap)
    {

        char  header[80];
        char  footer[80];
        char* headerEnd;
        char* footerEnd;
        long  neededSz;

        if (type == CERT_TYPE) {
            strncpy(header, "-----BEGIN CERTIFICATE-----", sizeof(header));
            strncpy(footer, "-----END CERTIFICATE-----", sizeof(footer));
        } else {
            strncpy(header, "-----BEGIN RSA PRIVATE KEY-----", sizeof(header));
            strncpy(footer, "-----END RSA PRIVATE KEY-----", sizeof(footer));
        }

        /* find header */
        headerEnd = strstr((char*)buff, header);
        if (!headerEnd) return SSL_BAD_FILE;
        headerEnd += strlen(header);

        /* get next line */
        if (headerEnd[0] == '\n')
            headerEnd++;
        else if (headerEnd[1] == '\n')
            headerEnd += 2;
        else
            return SSL_BAD_FILE;

        /* find footer */
        footerEnd = strstr((char*)buff, footer);
        if (!footerEnd) return SSL_BAD_FILE;

        /* set up der buffer */
        neededSz = (long)(footerEnd - headerEnd);
        if (neededSz > sz || neededSz < 0) return SSL_BAD_FILE;
        der->buffer = XMALLOC(neededSz, heap);
        if (!der->buffer) return MEMORY_ERROR;
        der->length = neededSz;

        if (Base64Decode((byte*)headerEnd, neededSz, der->buffer,
                         &der->length) < 0)
            return SSL_BAD_FILE;

        return 0;
    } 


    static int ProcessBuffer(SSL_CTX* ctx, const unsigned char* buff,
                             long sz, int format, int type)
    {
        buffer der;
        der.buffer = 0;

        if (format != SSL_FILETYPE_ASN1 && format != SSL_FILETYPE_PEM)
            return SSL_BAD_FILETYPE;

        if (format == SSL_FILETYPE_PEM) {
            if (PemToDerBuffer(buff, sz, type == PRIVATEKEY_TYPE ? type :
                               CERT_TYPE, &der, ctx->heap) < 0) {
                XFREE(der.buffer, ctx->heap);
                return SSL_BAD_FILE;
            }
        }
        else {  /* ASN1 (DER) */
            der.buffer = XMALLOC(sz, ctx->heap);
            if (!der.buffer) return MEMORY_ERROR;
            memcpy(der.buffer, buff, sz);
            der.length = sz;
        }

        if (type == CA_TYPE)
            return AddCA(ctx, der);     /* takes der over */
        else if (type == CERT_TYPE)
            ctx->certificate = der;     /* takes der over */
        else if (type == PRIVATEKEY_TYPE)
            ctx->privateKey = der;      /* takes der over */
        else {
            XFREE(der.buffer, ctx->heap);
            return SSL_BAD_CERTTYPE;
        }

        return SSL_SUCCESS;
    }


    int CyaSSL_CTX_load_verify_buffer(SSL_CTX* ctx, const unsigned char* buffer,
                                      long sz)
    {
        return ProcessBuffer(ctx, buffer, sz, SSL_FILETYPE_PEM, CA_TYPE);
    }


    int CyaSSL_CTX_use_certificate_buffer(SSL_CTX* ctx,
                                 const unsigned char* buffer, long sz, int type)
    {
        return ProcessBuffer(ctx, buffer, sz, type, CERT_TYPE);
    }


    int CyaSSL_CTX_use_PrivateKey_buffer(SSL_CTX* ctx,
                                 const unsigned char* buffer, long sz, int type)
    {
        return ProcessBuffer(ctx, buffer, sz, type, PRIVATEKEY_TYPE);
    }


    int CyaSSL_CTX_use_certificate_chain_buffer(SSL_CTX* ctx,
                                 const unsigned char* buffer, long sz)
    {
        /* add first to ctx, all tested implementations support this */
        return ProcessBuffer(ctx, buffer, sz, SSL_FILETYPE_PEM, CA_TYPE);
    }

#endif /* NO_FILESYSTEM */


#if defined(OPENSSL_EXTRA) || defined(GOAHEAD_WS)


    int SSLeay_add_ssl_algorithms(void)
    {
        OpenSSL_add_all_algorithms(); 
        return SSL_SUCCESS;
    }


    long SSL_CTX_sess_set_cache_size(SSL_CTX* ctx, long sz)
    {
        /* cache size fixed at compile time in CyaSSL */
        return 0;
    }


    void SSL_CTX_set_quiet_shutdown(SSL_CTX* ctx, int mode)
    {
        if (mode)
            ctx->quietShutdown = 1;
    }


    int SSL_CTX_check_private_key(SSL_CTX* ctx)
    {
        /* TODO: check private against public for RSA match */
        return SSL_SUCCESS;
    }


    void SSL_set_bio(SSL* ssl, BIO* rd, BIO* wr)
    {
        SSL_set_rfd(ssl, rd->fd);
        SSL_set_wfd(ssl, wr->fd);

        ssl->biord = rd;
        ssl->biowr = wr;
    }


    void SSL_CTX_set_client_CA_list(SSL_CTX* ctx, STACK_OF(X509_NAME)* names)
    {
   
    }


    STACK_OF(X509_NAME)* SSL_load_client_CA_file(const char* fname)
    {
        return 0;
    }


    int SSL_CTX_set_default_verify_paths(SSL_CTX* ctx)
    {
        /* TODO:, not needed in goahead */
        return SSL_NOT_IMPLEMENTED;
    }


    void SSL_set_accept_state(SSL* ssl)
    {
        ssl->options.side = SERVER_END;
    }


    void OpenSSL_add_all_algorithms(void)
    {
        InitCyaSSL(); 
    }


    int SSLeay_add_all_algorithms(void)
    {
        OpenSSL_add_all_algorithms(); 
        return SSL_SUCCESS;
    }

    
    void SSL_CTX_set_tmp_rsa_callback(SSL_CTX* ctx, RSA*(*f)(SSL*, int, int))
    {
        /* CyaSSL verifies all these internally */   
    }


    void SSL_set_shutdown(SSL* ssl, int opt)
    {
       
    }


    long SSL_CTX_set_options(SSL_CTX* ctx, long opt)
    {
        /* goahead calls with 0, do nothing */ 
        return opt;
    }


    int SSL_set_rfd(SSL* ssl, int rfd)
    {
        ssl->rfd = rfd;      /* not used directly to allow IO callbacks */

        ssl->IOCB_ReadCtx  = &ssl->rfd;

        return SSL_SUCCESS;
    }


    int SSL_set_wfd(SSL* ssl, int wfd)
    {
        ssl->wfd = wfd;      /* not used directly to allow IO callbacks */

        ssl->IOCB_WriteCtx  = &ssl->wfd;

        return SSL_SUCCESS;
    }


    RSA* RSA_generate_key(int len, unsigned long bits, void(*f)(int,
                                                        int, void*), void* data)
    {
        /* no tmp key needed, actual generation not supported */
        return 0;
    }


    X509_NAME* X509_get_issuer_name(X509* cert)
    {
        return &cert->issuer;
    }


    X509_NAME* X509_get_subject_name(X509* cert)
    {
        return &cert->subject;
    }


    /* copy name into buffer, at most sz bytes, if buffer is null will
       malloc buffer, call responsible for freeing                     */
    char* X509_NAME_oneline(X509_NAME* name, char* buffer, int sz)
    {
        int copySz = min(sz, name->sz);
        if (!name->sz) return buffer;

        if (!buffer) {
            buffer = (char*)XMALLOC(name->sz, 0);
            if (!buffer) return buffer;
            copySz = name->sz;
        }

        if (copySz == 0)
            return buffer;

        memcpy(buffer, name->name, copySz - 1);
        buffer[copySz - 1] = 0;

        return buffer;
    }


    X509* X509_STORE_CTX_get_current_cert(X509_STORE_CTX* ctx)
    {
        return 0;
    }


    int X509_STORE_CTX_get_error(X509_STORE_CTX* ctx)
    {
        return 0;
    }


    int X509_STORE_CTX_get_error_depth(X509_STORE_CTX* ctx)
    {
        return 0;
    }


    BIO_METHOD* BIO_f_buffer(void)
    {
        static BIO_METHOD meth;
        meth.type = BIO_BUFFER;

        return &meth;
    }


    long BIO_set_write_buffer_size(BIO* bio, long size)
    {
        /* CyaSSL has internal buffer, compatibility only */
        return size; 
    }


    BIO_METHOD* BIO_f_ssl(void)
    {
        static BIO_METHOD meth;
        meth.type = BIO_SSL;

        return &meth;
    }


    BIO* BIO_new_socket(int sfd, int close)
    {
        BIO* bio = (BIO*) XMALLOC(sizeof(BIO), 0);
        if (bio) { 
            bio->type  = BIO_SOCKET;
            bio->close = close;
            bio->eof   = 0;
            bio->ssl   = 0;
            bio->fd    = sfd;
            bio->prev  = 0;
            bio->next  = 0;
        }
        return bio; 
    }


    int BIO_eof(BIO* b)
    {
        if (b->eof)
            return 1;

        return 0;        
    }


    long BIO_set_ssl(BIO* b, SSL* ssl, int close)
    {
        b->ssl   = ssl;
        b->close = close;
    /* add to ssl for bio free if SSL_free called before/instead of free_all? */

        return 0;
    }


    BIO* BIO_new(BIO_METHOD* method)
    {
        BIO* bio = (BIO*) XMALLOC(sizeof(BIO), 0);
        if (bio) {
            bio->type  = method->type;
            bio->close = 0;
            bio->eof   = 0;
            bio->ssl   = 0;
            bio->fd    = 0;
            bio->prev  = 0;
            bio->next  = 0;
        }
        return bio;
    }


#ifdef _WIN32
    #define CloseSocket(s) closesocket(s)
#else
    #define CloseSocket(s) close(s)
#endif

    int BIO_free(BIO* bio)
    {
        /* unchain?, doesn't matter in goahead since from free all */
        if (bio) {
            if (bio->close) {
                if (bio->ssl)
                    SSL_free(bio->ssl);
                if (bio->fd)
                    CloseSocket(bio->fd);
            }
            XFREE(bio, 0);
        }
        return 0;
    }


    int BIO_free_all(BIO* bio)
    {
        BIO* next = bio;

        while ( (bio = next) ) {
            next = bio->next;
            BIO_free(bio);
        }
        return 0;
    }


    int BIO_read(BIO* bio, void* buf, int len)
    {
        int  ret;
        SSL* ssl = 0;
        BIO* front = bio;

        /* already got eof, again is error */
        if (front->eof)
            return -1;

        while(bio && ((ssl = bio->ssl) == 0) )
            bio = bio->next;

        if (ssl == 0) return -1;

        ret = SSL_read(ssl, buf, len);
        if (ret == 0)
            front->eof = 1;
        else if (ret < 0) {
            int err = SSL_get_error(ssl, 0);
            if ( !(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) )
                front->eof = 1;
        }
        return ret;
    }


    int BIO_write(BIO* bio, const void* data, int len)
    {
        int  ret;
        SSL* ssl = 0;
        BIO* front = bio;

        /* already got eof, again is error */
        if (front->eof)
            return -1;

        while(bio && ((ssl = bio->ssl) == 0) )
            bio = bio->next;

        if (ssl == 0) return -1;

        ret = SSL_write(ssl, data, len);
        if (ret == 0)
            front->eof = 1;
        else if (ret < 0) {
            int err = SSL_get_error(ssl, 0);
            if ( !(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) )
                front->eof = 1;
        }

        return ret;
    }


    BIO* BIO_push(BIO* top, BIO* append)
    {
        top->next    = append;
        append->prev = top;

        return top;
    }


    int BIO_flush(BIO* bio)
    {
        /* for CyaSSL no flushing needed */
        return 1;
    }


#endif /* OPENSSL_EXTRA || GOAHEAD_WS */


#ifdef OPENSSL_EXTRA

    unsigned long SSLeay(void)
    {
        return SSLEAY_VERSION_NUMBER;
    }


    const char* SSLeay_version(int type)
    {
        static const char* version = "SSLeay CyaSSL compatibility";
        return version;
    }


    void MD5_Init(MD5_CTX* md5)
    {
        typedef char md5_test[sizeof(MD5_CTX) >= sizeof(Md5) ? 1 : -1];
        (void)sizeof(md5_test);

        InitMd5((Md5*)md5);
    }


    void MD5_Update(MD5_CTX* md5, const void* input, unsigned long sz)
    {
        Md5Update((Md5*)md5, (const byte*)input, sz);
    }


    void MD5_Final(byte* input, MD5_CTX* md5)
    {
        Md5Final((Md5*)md5, input);
    }


    void SHA_Init(SHA_CTX* sha)
    {
        typedef char sha_test[sizeof(SHA_CTX) >= sizeof(Sha) ? 1 : -1];
        (void)sizeof(sha_test);

        InitSha((Sha*)sha);
    }


    void SHA_Update(SHA_CTX* sha, const void* input, unsigned long sz)
    {
        ShaUpdate((Sha*)sha, (const byte*)input, sz);
    }


    void SHA_Final(byte* input, SHA_CTX* sha)
    {
        ShaFinal((Sha*)sha, input);
    }


    const EVP_MD* EVP_md5(void)
    {
        static const char* type = "MD5";
        return type;
    }


    const EVP_MD* EVP_sha1(void)
    {
        static const char* type = "SHA";
        return type;
    }


    void EVP_MD_CTX_init(EVP_MD_CTX* ctx)
    {
        /* do nothing */ 
    }


    int EVP_MD_CTX_cleanup(EVP_MD_CTX* ctx)
    {
        return 0;
    }    


    int EVP_DigestInit(EVP_MD_CTX* ctx, const EVP_MD* type)
    {
        if (strncmp(type, "MD5", 3) == 0) {
             ctx->macType = MD5;
             MD5_Init((MD5_CTX*)&ctx->hash);
        }
        else if (strncmp(type, "SHA", 3) == 0) {
             ctx->macType = SHA;
             SHA_Init((SHA_CTX*)&ctx->hash);
        }
        else
             return -1;

        return 0;
    }


    int EVP_DigestUpdate(EVP_MD_CTX* ctx, const void* data, size_t sz)
    {
        if (ctx->macType == MD5) 
            MD5_Update((MD5_CTX*)&ctx->hash, data, (unsigned long)sz);
        else if (ctx->macType == SHA) 
            SHA_Update((SHA_CTX*)&ctx->hash, data, (unsigned long)sz);
        else
            return -1;

        return 0;
    }


    int EVP_DigestFinal(EVP_MD_CTX* ctx, unsigned char* md, unsigned int* s)
    {
        if (ctx->macType == MD5) {
            MD5_Final(md, (MD5_CTX*)&ctx->hash);
            if (s) *s = MD5_DIGEST_SIZE;
        }
        else if (ctx->macType == SHA) {
            SHA_Final(md, (SHA_CTX*)&ctx->hash);
            if (s) *s = SHA_DIGEST_SIZE;
        }
        else
            return -1;

        return 0;
    }


    int EVP_DigestFinal_ex(EVP_MD_CTX* ctx, unsigned char* md, unsigned int* s)
    {
        return EVP_DigestFinal(ctx, md, s);
    }


    unsigned char* HMAC(const EVP_MD* evp_md, const void* key, int key_len,
        const unsigned char* d, int n, unsigned char* md, unsigned int* md_len)
    {
        Hmac hmac;

        if (!md) return 0;  /* no static buffer support */

        if (strncmp(evp_md, "MD5", 3) == 0) {
            HmacSetKey(&hmac, MD5, key, key_len);
            if (md_len) *md_len = MD5_DIGEST_SIZE;
        }
        else if (strncmp(evp_md, "SHA", 3) == 0) {
            HmacSetKey(&hmac, SHA, key, key_len);    
            if (md_len) *md_len = SHA_DIGEST_SIZE;
        }
        else
            return 0;

        HmacUpdate(&hmac, d, n);
        HmacFinal(&hmac, md);
    
        return md;
    }

    unsigned long ERR_get_error(void)
    {
        /* TODO: */
        return 0;
    }

    void ERR_clear_error(void)
    {
        /* TODO: */
    }


    int RAND_status(void)
    {
        return 1;  /* CTaoCrypt provides enough seed internally */
    }


    int RAND_bytes(unsigned char* buf, int num)
    {
        RNG rng;

        if (InitRng(&rng))
           return 0;

        RNG_GenerateBlock(&rng, buf, num);

        return 1;
    }


    int DES_key_sched(const_DES_cblock* key, DES_key_schedule* schedule)
    {
        memcpy(schedule, key, sizeof(const_DES_cblock));
        return 0;
    }


    void DES_cbc_encrypt(const unsigned char* input, unsigned char* output,
                     long length, DES_key_schedule* schedule, DES_cblock* ivec,
                     int enc)
    {
        Des des;
        Des_SetKey(&des, (const byte*)schedule, (const byte*)ivec, !enc);

        if (enc)
            Des_CbcEncrypt(&des, output, input, length);
        else
            Des_CbcDecrypt(&des, output, input, length);
    }


    /* correctly sets ivec for next call */
    void DES_ncbc_encrypt(const unsigned char* input, unsigned char* output,
                     long length, DES_key_schedule* schedule, DES_cblock* ivec,
                     int enc)
    {
        Des des;
        Des_SetKey(&des, (const byte*)schedule, (const byte*)ivec, !enc);

        if (enc)
            Des_CbcEncrypt(&des, output, input, length);
        else
            Des_CbcDecrypt(&des, output, input, length);

        memcpy(ivec, output + length - sizeof(DES_cblock), sizeof(DES_cblock));
    }


    void ERR_free_strings(void)
    {
        /* handled internally */
    }


    void ERR_remove_state(unsigned long state)
    {
        /* TODO: GetErrors().Remove(); */
    }


    void EVP_cleanup(void)
    {
        /* nothing to do here */
    }


    void CRYPTO_cleanup_all_ex_data(void)
    {
        /* nothing to do here */
    }


    long SSL_CTX_set_mode(SSL_CTX* ctx, long mode)
    {
        /* SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER is CyaSSL default mode */

        if (mode == SSL_MODE_ENABLE_PARTIAL_WRITE)
            ctx->partialWrite = 1;

        return mode;
    }


    long SSL_CTX_get_mode(SSL_CTX* ctx)
    {
        /* TODO: */
        return 0;
    }


    void SSL_CTX_set_default_read_ahead(SSL_CTX* ctx, int m)
    {
        /* TODO: maybe? */
    }


    int SSL_CTX_set_session_id_context(SSL_CTX* ctx,
                                       const unsigned char* sid_ctx,
                                       unsigned int sid_ctx_len)
    {
        /* No application specific context needed for cyaSSL */
        return SSL_SUCCESS;
    }


    long SSL_CTX_sess_get_cache_size(SSL_CTX* ctx)
    {
        /* TODO: maybe? */
        return (~0);
    }

    unsigned long ERR_get_error_line_data(const char** file, int* line,
                                          const char** data, int *flags)
    {
        /* Not implemented */
        return 0;
    }


    X509* SSL_get_peer_certificate(SSL* ssl)
    {
        if (ssl->peerCert.issuer.sz)
            return &ssl->peerCert;
        else
            return 0;
    }



    int SSL_set_ex_data(SSL* ssl, int idx, void* data)
    {
        return 0;
    }


    int SSL_get_shutdown(const SSL* ssl)
    {
        return 0;
    }


    int SSL_set_session_id_context(SSL* ssl, const unsigned char* id,
                                   unsigned int len)
    {
        return 0;
    }


    void SSL_set_connect_state(SSL* ssl)
    {
        /* client by default */ 
    }


    int SSL_session_reused(SSL* ssl)
    {
        return ssl->options.resuming;
    }


    void SSL_SESSION_free(SSL_SESSION* session)
    {
     
    }


    const char* SSL_get_version(SSL* ssl)
    {
        if (ssl->version.major == 3) {
            switch (ssl->version.minor) {
                case 0 :
                    return "SSLv3";
                case 1 :
                    return "TLSv1";
                case 2 :
                    return "TLSv1.1";
                case 3 :
                    return "TLSv1.2";
            }
        }
        return "unknown";
    }


    SSL_CIPHER*  SSL_get_current_cipher(SSL* ssl)
    {
        return &ssl->cipher;
    }


    const char* SSL_CIPHER_get_name(const SSL_CIPHER* cipher)
    {
        if (cipher) {
            switch (cipher->ssl->options.cipherSuite) {
                case SSL_RSA_WITH_RC4_128_SHA :
                    return "SSL_RSA_WITH_RC4_128_SHA";
                case SSL_RSA_WITH_RC4_128_MD5 :
                    return "SSL_RSA_WITH_RC4_128_MD5";
                case SSL_RSA_WITH_3DES_EDE_CBC_SHA :
                    return "SSL_RSA_WITH_3DES_EDE_CBC_SHA";
                case TLS_RSA_WITH_AES_128_CBC_SHA :
                    return "TLS_RSA_WITH_AES_128_CBC_SHA";
                case TLS_RSA_WITH_AES_256_CBC_SHA :
                    return "TLS_RSA_WITH_AES_256_CBC_SHA";
                case TLS_PSK_WITH_AES_128_CBC_SHA :
                    return "TLS_PSK_WITH_AES_128_CBC_SHA";
                case TLS_PSK_WITH_AES_256_CBC_SHA :
                    return "TLS_PSK_WITH_AES_256_CBC_SHA";
                case TLS_DHE_RSA_WITH_AES_128_CBC_SHA :
                    return "TLS_DHE_RSA_WITH_AES_128_CBC_SHA";
                case TLS_DHE_RSA_WITH_AES_256_CBC_SHA :
                    return "TLS_DHE_RSA_WITH_AES_256_CBC_SHA";
                case TLS_RSA_WITH_HC_128_CBC_MD5 :
                    return "TLS_RSA_WITH_HC_128_CBC_MD5";
                case TLS_RSA_WITH_HC_128_CBC_SHA :
                    return "TLS_RSA_WITH_HC_128_CBC_SHA";
                case TLS_RSA_WITH_RABBIT_CBC_SHA :
                    return "TLS_RSA_WITH_RABBIT_CBC_SHA";
            }
        }

        return "NONE";
    }


    char* SSL_CIPHER_description(SSL_CIPHER* cipher, char* buffer, int len)
    {
        return 0;
    }


    SSL_SESSION* SSL_get1_session(SSL* ssl)  /* what's ref count */
    {
        return 0;
    }


    void X509_free(X509* buf)
    {
       
    }


    void OPENSSL_free(void* buf)
    {
  
    }


    int OCSP_parse_url(char* url, char** host, char** port, char** path,
                       int* ssl)
    {
        return 0;
    }


    SSL_METHOD* SSLv2_client_method(void)
    {
        return 0;
    }


    SSL_METHOD* SSLv2_server_method(void)
    {
        return 0;
    }


    void MD4_Init(MD4_CTX* md4)
    {
        /* make sure we have a big enough buffer */
        typedef char ok[sizeof(md4->buffer) >= sizeof(Md4) ? 1 : -1];
        (void) sizeof(ok);
 
        InitMd4((Md4*)md4);    
    }


    void MD4_Update(MD4_CTX* md4, const void* data, size_t len)
    {
        Md4Update((Md4*)md4, (const byte*)data, (word32)len); 
    }


    void MD4_Final(unsigned char* digest, MD4_CTX* md4)
    {
        Md4Final((Md4*)md4, digest); 
    }



    BIO* BIO_pop(BIO* top)
    {
        return 0;
    }


    int BIO_pending(BIO* bio)
    {
        return 0;
    }



    BIO_METHOD* BIO_s_mem(void)
    {
        return 0;
    }


    BIO_METHOD* BIO_f_base64(void)
    {
        return 0;
    }


    void BIO_set_flags(BIO* bio, int flags)
    {
     
    }



    void RAND_screen(void)
    {
    
    }


    const char* RAND_file_name(char* fname, size_t len)
    {
        return 0;
    }


    int RAND_write_file(const char* fname)
    {
        return 0;
    }


    int RAND_load_file(const char* fname, long len)
    {
        /* CTaoCrypt provides enough entropy internally or will report error */
        if (len == -1)
            return 1024;
        else
            return (int)len;
    }


    int RAND_egd(const char* path)
    {
        return 0;
    }



    COMP_METHOD* COMP_zlib(void)
    {
        return 0;
    }


    COMP_METHOD* COMP_rle(void)
    {
        return 0;
    }


    int SSL_COMP_add_compression_method(int method, void* data)
    {
        return 0;
    }



    int SSL_get_ex_new_index(long idx, void* data, void* cb1, void* cb2,
                             void* cb3)
    {
        return 0;
    }


    int CRYPTO_num_locks(void)
    {
        return 0;
    }


    void CRYPTO_set_id_callback(unsigned long (*f)(void))
    {
    
    }


    void CRYPTO_set_locking_callback(void (*f)(int, int, const char*, int))
    {
      
    }


    void CRYPTO_set_dynlock_create_callback(CRYPTO_dynlock_value* (*f)(
                                                              const char*, int))
    {
     
    }


    void CRYPTO_set_dynlock_lock_callback(void (*f)(int, CRYPTO_dynlock_value*,
                                                const char*, int))
    {
     
    }


    void CRYPTO_set_dynlock_destroy_callback(void (*f)(CRYPTO_dynlock_value*,
                                                   const char*, int))
    {
      
    }



    const char* X509_verify_cert_error_string(long err)
    {
        return 0;
    }



    int X509_LOOKUP_add_dir(X509_LOOKUP* lookup, const char* dir, long len)
    {
        return 0;
    }


    int X509_LOOKUP_load_file(X509_LOOKUP* lookup, const char* file, long len)
    {
        return 0;
    }


    X509_LOOKUP_METHOD* X509_LOOKUP_hash_dir(void)
    {
        return 0;
    }


    X509_LOOKUP_METHOD* X509_LOOKUP_file(void)
    {
        return 0;
    }



    X509_LOOKUP* X509_STORE_add_lookup(X509_STORE* store, X509_LOOKUP_METHOD* m)
    {
        return 0;
    }


    X509_STORE* X509_STORE_new(void)
    {
        return 0;
    }


    int X509_STORE_get_by_subject(X509_STORE_CTX* ctx, int idx, X509_NAME* name,
                                       X509_OBJECT* obj)
    {
        return 0;
    }


    int X509_STORE_CTX_init(X509_STORE_CTX* ctx, X509_STORE* store, X509* x509,
                            STACK_OF(X509)* sk)
    {
        return 0;
    }


    void X509_STORE_CTX_cleanup(X509_STORE_CTX* ctx)
    {
 
    }



    ASN1_TIME* X509_CRL_get_lastUpdate(X509_CRL* crl)
    {
        return 0;
    }


    ASN1_TIME* X509_CRL_get_nextUpdate(X509_CRL* crl)
    {
        return 0;
    }



    EVP_PKEY* X509_get_pubkey(X509* x509)
    {
        return 0;
    }


    int X509_CRL_verify(X509_CRL* crl, EVP_PKEY* key)
    {
        return 0;
    }


    void X509_STORE_CTX_set_error(X509_STORE_CTX* ctx, int err)
    {
 
    }


    void X509_OBJECT_free_contents(X509_OBJECT* obj)
    {
  
    }


    void EVP_PKEY_free(EVP_PKEY* key)
    {
     
    }


    int X509_cmp_current_time(const ASN1_TIME* time)
    {
        return 0;
    }


    int sk_X509_REVOKED_num(X509_REVOKED* revoked)
    {
        return 0;
    }



    X509_REVOKED* X509_CRL_get_REVOKED(X509_CRL* crl)
    {
        return 0;
    }


    X509_REVOKED* sk_X509_REVOKED_value(X509_REVOKED* revoked, int value)
    {
        return 0;
    }



    ASN1_INTEGER* X509_get_serialNumber(X509* x509)
    {
        return 0;
    }



    int ASN1_TIME_print(BIO* bio, const ASN1_TIME* time)
    {
        return 0;
    }



    int ASN1_INTEGER_cmp(const ASN1_INTEGER* a, const ASN1_INTEGER* b)
    {
        return 0;
    }


    long ASN1_INTEGER_get(const ASN1_INTEGER* i)
    {
        return 0;
    }



    void* X509_STORE_CTX_get_ex_data(X509_STORE_CTX* ctx, int idx)
    {
        return 0;
    }


    int SSL_get_ex_data_X509_STORE_CTX_idx(void)
    {
        return 0;
    }


    void* SSL_get_ex_data(const SSL* ssl, int idx)
    {
        return 0;
    }


    void SSL_CTX_set_default_passwd_cb_userdata(SSL_CTX* ctx, void* userdata)
    {
        ctx->userdata = userdata;
    }


    void SSL_CTX_set_default_passwd_cb(SSL_CTX* ctx, pem_password_cb cb)
    {
        ctx->passwd_cb = cb;
    }


    int SSL_CTX_use_RSAPrivateKey_file(SSL_CTX* ctx, const char* file, int type)
    {
        if (ProcessFile(ctx, file, type, PRIVATEKEY_TYPE) == SSL_SUCCESS)
            return SSL_SUCCESS;

        return SSL_FAILURE;
    }


    long SSL_CTX_set_timeout(SSL_CTX* ctx, long to)
    {
        return 0;
    }


    void SSL_CTX_set_info_callback(SSL_CTX* ctx, void (*f)())
    {
        
    }


    unsigned long ERR_peek_error(void)
    {
        return 0;
    }


    int ERR_GET_REASON(int err)
    {
        return 0;
    }


    char* SSL_alert_type_string_long(int alert)
    {
        return 0;
    }


    char* SSL_alert_desc_string_long(int alert)
    {
        return 0;
    }


    char* SSL_state_string_long(SSL* ssl)
    {
        return 0;
    }



    void RSA_free(RSA* rsa)
    {
        
    }


    int PEM_def_callback(char* name, int num, int w, void* key)
    {
        return 0;
    }
    

    long SSL_CTX_sess_accept(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_connect(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_accept_good(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_connect_good(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_accept_renegotiate(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_connect_renegotiate(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_hits(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_cb_hits(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_cache_full(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_misses(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_timeouts(SSL_CTX* ctx)
    {
        return 0;
    }


    long SSL_CTX_sess_number(SSL_CTX* ctx)
    {
        return 0;
    }


    void DES_set_key_unchecked(const_DES_cblock* des, DES_key_schedule* key)
    {
    }


    void DES_set_odd_parity(DES_cblock* des)
    {
    }

    
    void DES_ecb_encrypt(DES_cblock* desa, DES_cblock* desb,
                         DES_key_schedule* key, int len)
    {
    }

    int BIO_printf(BIO* bio, const char* format, ...)
    {
        return 0;
    }


    int ASN1_UTCTIME_print(BIO* bio, const ASN1_UTCTIME* a)
    {
        return 0;
    }
    

    int  sk_num(X509_REVOKED* rev)
    {
        return 0;
    }


    void* sk_value(X509_REVOKED* rev, int i)
    {
        return 0;
    }


    int EVP_BytesToKey(const EVP_CIPHER* type, const EVP_MD* md,
                       const byte* salt, const byte* data, int sz, int count,
                       byte* key, byte* iv)
    {
        int keyLen = 0;
        int ivLen  = 0;

        Md5    myMD;
        byte   digest[MD5_DIGEST_SIZE];

        int j;
        int keyLeft;
        int ivLeft;
        int keyOutput = 0;

        InitMd5(&myMD);

        /* only support MD5 for now */
        if (strncmp(md, "MD5", 3)) return 0;

        /* only support CBC DES and AES for now */
        if (strncmp(type, "DES-CBC", 7) == 0) {
            keyLen = DES_KEY_SIZE;
            ivLen  = DES_IV_SIZE;
        }
        else if (strncmp(type, "DES-EDE3-CBC", 12) == 0) {
            keyLen = DES3_KEY_SIZE;
            ivLen  = DES_IV_SIZE;
        }
        else if (strncmp(type, "AES-128-CBC", 11) == 0) {
            keyLen = AES_128_KEY_SIZE;
            ivLen  = AES_IV_SIZE;
        }
        else if (strncmp(type, "AES-192-CBC", 11) == 0) {
            keyLen = AES_192_KEY_SIZE;
            ivLen  = AES_IV_SIZE;
        }
        else if (strncmp(type, "AES-256-CBC", 11) == 0) {
            keyLen = AES_256_KEY_SIZE;
            ivLen  = AES_IV_SIZE;
        }
        else
            return 0;

        keyLeft   = keyLen;
        ivLeft    = ivLen;

        while (keyOutput < (keyLen + ivLen)) {
            int digestLeft = MD5_DIGEST_SIZE;
            /* D_(i - 1) */
            if (keyOutput)                      /* first time D_0 is empty */
                Md5Update(&myMD, digest, MD5_DIGEST_SIZE);
            /* data */
            Md5Update(&myMD, data, sz);
            /* salt */
            if (salt)
                Md5Update(&myMD, salt, EVP_SALT_SIZE);
            Md5Final(&myMD, digest);
            /* count */
            for (j = 1; j < count; j++) {
                Md5Update(&myMD, digest, MD5_DIGEST_SIZE);
                Md5Final(&myMD, digest);
            }

            if (keyLeft) {
                int store = min(keyLeft, MD5_DIGEST_SIZE);
                memcpy(&key[keyLen - keyLeft], digest, store);

                keyOutput  += store;
                keyLeft    -= store;
                digestLeft -= store;
            }

            if (ivLeft && digestLeft) {
                int store = min(ivLeft, digestLeft);
                memcpy(&iv[ivLen - ivLeft], &digest[MD5_DIGEST_SIZE -
                                                    digestLeft], store);
                keyOutput += store;
                ivLeft    -= store;
            }
        }
        if (keyOutput != (keyLen + ivLen))
            return 0;
        return keyOutput;
    }

    /* stunnel 4.28 needs */
    void* SSL_CTX_get_ex_data(const SSL_CTX* ctx, int d)
    {
        return 0;
    }


    int SSL_CTX_set_ex_data(SSL_CTX* ctx, int d, void* p)
    {
        return SSL_SUCCESS;
    }


    void SSL_CTX_sess_set_get_cb(SSL_CTX* ctx, SSL_SESSION*(*f)(SSL*,
                                                    unsigned char*, int, int*))
    {
       
    }


    void SSL_CTX_sess_set_new_cb(SSL_CTX* ctx, int (*f)(SSL*, SSL_SESSION*))
    {

    }


    void SSL_CTX_sess_set_remove_cb(SSL_CTX* ctx, void (*f)(SSL_CTX*,
                                                            SSL_SESSION*))
    {

    }


    int i2d_SSL_SESSION(SSL_SESSION* sess, unsigned char** p)
    {
        return sizeof(SSL_SESSION);
    }


    SSL_SESSION* d2i_SSL_SESSION(SSL_SESSION** sess, const unsigned char** p,
                                 long i)
    {
        return *sess;
    }


    long SSL_SESSION_get_timeout(const SSL_SESSION* sess)
    {
        return sess->timeout;
    }


    long SSL_SESSION_get_time(const SSL_SESSION* sess)
    {
        return sess->bornOn;
    }


    int SSL_CTX_get_ex_new_index(long idx, void* arg, void* a, void* b, void* c)
    {
        return 0; 
    }


#endif /* OPENSSL_EXTRA */


#ifdef SESSION_CERTS


/* Get peer's certificate chain */
X509_CHAIN* CyaSSL_get_peer_chain(SSL* ssl)
{
    if (ssl)
        return &ssl->session.chain;

    return 0;
}


/* Get peer's certificate chain total count */
int CyaSSL_get_chain_count(X509_CHAIN* chain)
{
    if (chain)
        return chain->count;

    return 0;
}


/* Get peer's ASN.1 DER ceritifcate at index (idx) length in bytes */
int CyaSSL_get_chain_length(X509_CHAIN* chain, int idx)
{
    if (chain)
        return chain->certs[idx].length;

    return 0;
}


/* Get peer's ASN.1 DER ceritifcate at index (idx) */
byte* CyaSSL_get_chain_cert(X509_CHAIN* chain, int idx)
{
    if (chain)
        return chain->certs[idx].buffer;

    return 0;
}


/* Get peer's PEM ceritifcate at index (idx), output to buffer if inLen big
   enough else return error (-1), output length is in *outLen */
int  CyaSSL_get_chain_cert_pem(X509_CHAIN* chain, int idx,
                               unsigned char* buffer, int inLen, int* outLen)
{
    const char header[] = "-----BEGIN CERTIFICATE-----\n";
    const char footer[] = "-----END CERTIFICATE-----\n";

    int headerLen = sizeof(header) - 1;
    int footerLen = sizeof(footer) - 1;
    int i;

    if (!chain || !outLen || !buffer)
        return -1;

    /* don't even try if inLen too short */
    if (inLen < headerLen + footerLen + chain->certs[idx].length)
        return -1;

    /* header */
    memcpy(buffer, header, headerLen);
    i = headerLen;

    /* body */
    *outLen = inLen;  /* input to Base64Encode */
    if (Base64Encode(chain->certs[idx].buffer, chain->certs[idx].length,
                     buffer + i, (word32*)outLen) < 0)
        return -1;
    i += *outLen;

    /* footer */
    if ( (i + footerLen) > inLen)
        return -1;
    memcpy(buffer + i, footer, footerLen);
    *outLen += headerLen + footerLen; 

    return 0;
}


/* get session ID */
const byte* CyaSSL_get_sessionID(const SSL_SESSION* session)
{
    return session->sessionID;
}


#endif /* SESSION_CERTS */

