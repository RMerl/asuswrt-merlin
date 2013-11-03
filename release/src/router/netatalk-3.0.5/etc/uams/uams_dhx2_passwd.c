/*
 *
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * Copyright (c) 1999 Adrian Sun (asun@u.washington.edu)
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef UAM_DHX2

#include <atalk/standards.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <arpa/inet.h>
#include <unistd.h>

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include <sys/time.h>
#include <time.h>

#ifdef SHADOWPW
#include <shadow.h>
#endif

#ifdef HAVE_LIBGCRYPT
#include <gcrypt.h>
#endif

#include <atalk/afp.h>
#include <atalk/uam.h>
#include <atalk/logger.h>

/* Number of bits for p which we generate. Everybode out there uses 512, so we beet them */
#define PRIMEBITS 1024

/* hash a number to a 16-bit quantity */
#define dhxhash(a) ((((unsigned long) (a) >> 8) ^   \
                     (unsigned long) (a)) & 0xffff)

/* Some parameters need be maintained across calls */
static gcry_mpi_t p, Ra;
static gcry_mpi_t serverNonce;
static char *K_MD5hash = NULL;
static int K_hash_len;
static uint16_t ID;

/* The initialization vectors for CAST128 are fixed by Apple. */
static unsigned char dhx_c2siv[] = { 'L', 'W', 'a', 'l', 'l', 'a', 'c', 'e' };
static unsigned char dhx_s2civ[] = { 'C', 'J', 'a', 'l', 'b', 'e', 'r', 't' };

/* Static variables used to communicate between the conversation function
 * and the server_login function */
static struct passwd *dhxpwd;

/*********************************************************
 * Crypto helper func to generate p and g for use in DH.
 * libgcrypt doesn't provide one directly.
 * Algorithm taken from GNUTLS:gnutls_dh_primes.c
 *********************************************************/

/**
 * This function will generate a new pair of prime and generator for use in
 * the Diffie-Hellman key exchange.
 * The bits value should be one of 768, 1024, 2048, 3072 or 4096.
 **/

static int
dh_params_generate (gcry_mpi_t *ret_p, gcry_mpi_t *ret_g, unsigned int bits) {

    int result, times = 0, qbits;

    gcry_mpi_t g = NULL, prime = NULL;
    gcry_mpi_t *factors = NULL;
    gcry_error_t err;

    /* Version check should be the very first call because it
       makes sure that important subsystems are intialized. */
    if (!gcry_check_version (GCRYPT_VERSION)) {
        LOG(log_info, logtype_uams, "PAM DHX2: libgcrypt versions mismatch. Need: %s", GCRYPT_VERSION);
        result = AFPERR_MISC;
        goto error;
    }

    if (bits < 256)
        qbits = bits / 2;
    else
        qbits = (bits / 40) + 105;

    if (qbits & 1) /* better have an even number */
        qbits++;

    /* find a prime number of size bits. */
    do {
        if (times) {
            gcry_mpi_release (prime);
            gcry_prime_release_factors (factors);
        }
        err = gcry_prime_generate (&prime, bits, qbits, &factors, NULL, NULL,
                                   GCRY_STRONG_RANDOM, GCRY_PRIME_FLAG_SPECIAL_FACTOR);
        if (err != 0) {
            result = AFPERR_MISC;
            goto error;
        }
        err = gcry_prime_check (prime, 0);
        times++;
    } while (err != 0 && times < 10);

    if (err != 0) {
        result = AFPERR_MISC;
        goto error;
    }

    /* generate the group generator. */
    err = gcry_prime_group_generator (&g, prime, factors, NULL);
    if (err != 0) {
        result = AFPERR_MISC;
        goto error;
    }

    gcry_prime_release_factors (factors);
    factors = NULL;

    if (ret_g)
        *ret_g = g;
    else
        gcry_mpi_release (g);
    if (ret_p)
        *ret_p = prime;
    else
        gcry_mpi_release (prime);

    return 0;

error:
    gcry_prime_release_factors (factors);
    gcry_mpi_release (g);
    gcry_mpi_release (prime);

    return result;
}

static int dhx2_setup(void *obj, char *ibuf _U_, size_t ibuflen _U_,
                      char *rbuf, size_t *rbuflen)
{
    int ret;
    size_t nwritten;
    gcry_mpi_t g, Ma;
    char *Ra_binary = NULL;
#ifdef SHADOWPW
    struct spwd *sp;
#endif /* SHADOWPW */
    uint16_t uint16;

    *rbuflen = 0;

    /* Initialize passwd/shadow */
#ifdef SHADOWPW
    if (( sp = getspnam( dhxpwd->pw_name )) == NULL ) {
        LOG(log_info, logtype_uams, "DHX2: no shadow passwd entry for this user");
        return AFPERR_NOTAUTH;
    }
    dhxpwd->pw_passwd = sp->sp_pwdp;
#endif /* SHADOWPW */

    if (!dhxpwd->pw_passwd)
        return AFPERR_NOTAUTH;

    /* Initialize DH params */

    p = gcry_mpi_new(0);
    g = gcry_mpi_new(0);
    Ra = gcry_mpi_new(0);
    Ma = gcry_mpi_new(0);

    /* Generate p and g for DH */
    ret = dh_params_generate( &p, &g, PRIMEBITS);
    if (ret != 0) {
        LOG(log_info, logtype_uams, "DHX2: Couldn't generate p and g");
        ret = AFPERR_MISC;
        goto error;
    }

    /* Generate our random number Ra. */
    Ra_binary = calloc(1, PRIMEBITS/8);
    if (Ra_binary == NULL) {
        ret = AFPERR_MISC;
        goto error;
    }
    gcry_randomize(Ra_binary, PRIMEBITS/8, GCRY_STRONG_RANDOM);
    gcry_mpi_scan(&Ra, GCRYMPI_FMT_USG, Ra_binary, PRIMEBITS/8, NULL);
    free(Ra_binary);
    Ra_binary = NULL;

    /* Ma = g^Ra mod p. This is our "public" key */
    gcry_mpi_powm(Ma, g, Ra, p);

    /* ------- DH Init done ------ */
    /* Start building reply packet */

    /* Session ID first */
    ID = dhxhash(obj);
    uint16 = htons(ID);
    memcpy(rbuf, &uint16, sizeof(uint16_t));
    rbuf += 2;
    *rbuflen += 2;

    /* g is next */
    gcry_mpi_print( GCRYMPI_FMT_USG, (unsigned char *)rbuf, 4, &nwritten, g);
    if (nwritten < 4) {
        memmove( rbuf+4-nwritten, rbuf, nwritten);
        memset( rbuf, 0, 4-nwritten);
    }
    rbuf += 4;
    *rbuflen += 4;

    /* len = length of p = PRIMEBITS/8 */
    uint16 = htons((uint16_t) PRIMEBITS/8);
    memcpy(rbuf, &uint16, sizeof(uint16_t));
    rbuf += 2;
    *rbuflen += 2;

    /* p */
    gcry_mpi_print( GCRYMPI_FMT_USG, (unsigned char *)rbuf, PRIMEBITS/8, NULL, p);
    rbuf += PRIMEBITS/8;
    *rbuflen += PRIMEBITS/8;

    /* Ma */
    gcry_mpi_print( GCRYMPI_FMT_USG, (unsigned char *)rbuf, PRIMEBITS/8, &nwritten, Ma);
    if (nwritten < PRIMEBITS/8) {
        memmove(rbuf + (PRIMEBITS/8) - nwritten, rbuf, nwritten);
        memset(rbuf, 0, (PRIMEBITS/8) - nwritten);
    }
    rbuf += PRIMEBITS/8;
    *rbuflen += PRIMEBITS/8;

    ret = AFPERR_AUTHCONT;

error:              /* We exit here anyway */
    /* We will only need p and Ra later, but mustn't forget to release it ! */
    gcry_mpi_release(g);
    gcry_mpi_release(Ma);
    return ret;
}

/* -------------------------------- */
static int login(void *obj, char *username, int ulen,  struct passwd **uam_pwd _U_,
                 char *ibuf, size_t ibuflen,
                 char *rbuf, size_t *rbuflen)
{
    if (( dhxpwd = uam_getname(obj, username, ulen)) == NULL ) {
        LOG(log_info, logtype_uams, "DHX2: unknown username");
        return AFPERR_NOTAUTH;
    }

    LOG(log_info, logtype_uams, "DHX2 login: %s", username);
    return dhx2_setup(obj, ibuf, ibuflen, rbuf, rbuflen);
}

/* -------------------------------- */
/* dhx login: things are done in a slightly bizarre order to avoid
 * having to clean things up if there's an error. */
static int passwd_login(void *obj, struct passwd **uam_pwd,
                        char *ibuf, size_t ibuflen,
                        char *rbuf, size_t *rbuflen)
{
    char *username;
    size_t len, ulen;

    *rbuflen = 0;

    /* grab some of the options */
    if (uam_afpserver_option(obj, UAM_OPTION_USERNAME, (void *) &username, &ulen) < 0) {
        LOG(log_info, logtype_uams, "DHX2: uam_afpserver_option didn't meet uam_option_username  -- %s",
            strerror(errno));
        return AFPERR_PARAM;
    }

    len = (unsigned char) *ibuf++;
    if ( len > ulen ) {
        LOG(log_info, logtype_uams, "DHX2: Signature Retieval Failure -- %s",
            strerror(errno));
        return AFPERR_PARAM;
    }

    memcpy(username, ibuf, len );
    ibuf += len;
    username[ len ] = '\0';

    if ((unsigned long) ibuf & 1) /* pad to even boundary */
        ++ibuf;

    return (login(obj, username, ulen, uam_pwd, ibuf, ibuflen, rbuf, rbuflen));
}

/* ----------------------------- */
static int passwd_login_ext(void *obj, char *uname, struct passwd **uam_pwd,
                            char *ibuf, size_t ibuflen,
                            char *rbuf, size_t *rbuflen)
{
    char *username;
    size_t len, ulen;
    uint16_t  temp16;

    *rbuflen = 0;

    /* grab some of the options */
    if (uam_afpserver_option(obj, UAM_OPTION_USERNAME, (void *) &username, &ulen) < 0) {
        LOG(log_info, logtype_uams, "DHX2: uam_afpserver_option didn't meet uam_option_username  -- %s",
            strerror(errno));
        return AFPERR_PARAM;
    }

    if (*uname != 3)
        return AFPERR_PARAM;
    uname++;
    memcpy(&temp16, uname, sizeof(temp16));
    len = ntohs(temp16);

    if ( !len || len > ulen ) {
        LOG(log_info, logtype_uams, "DHX2: Signature Retrieval Failure -- %s",
            strerror(errno));
        return AFPERR_PARAM;
    }
    memcpy(username, uname +2, len );
    username[ len ] = '\0';

    return (login(obj, username, ulen, uam_pwd, ibuf, ibuflen, rbuf, rbuflen));
}

/* -------------------------------- */

static int logincont1(void *obj _U_, struct passwd **uam_pwd _U_,
                      char *ibuf, size_t ibuflen,
                      char *rbuf, size_t *rbuflen)
{
    size_t nwritten;
    int ret;
    gcry_mpi_t Mb, K, clientNonce;
    unsigned char *K_bin = NULL;
    char serverNonce_bin[16];
    gcry_cipher_hd_t ctx;
    gcry_error_t ctxerror;
    uint16_t uint16;

    *rbuflen = 0;

    Mb = gcry_mpi_new(0);
    K = gcry_mpi_new(0);
    clientNonce = gcry_mpi_new(0);
    serverNonce = gcry_mpi_new(0);

    /* Packet size should be: Session ID + Ma + Encrypted client nonce */
    if (ibuflen != 2 + PRIMEBITS/8 + 16) {
        LOG(log_error, logtype_uams, "DHX2: Paket length not correct");
        ret = AFPERR_PARAM;
        goto error_noctx;
    }

    /* Skip session id */
    ibuf += 2;

    /* Extract Mb, client's "public" key */
    gcry_mpi_scan(&Mb, GCRYMPI_FMT_USG, ibuf, PRIMEBITS/8, NULL);
    ibuf += PRIMEBITS/8;

    /* Now finally generate the Key: K = Mb^Ra mod p */
    gcry_mpi_powm(K, Mb, Ra, p);

    /* We need K in binary form in order to ... */
    K_bin = calloc(1, PRIMEBITS/8);
    if (K_bin == NULL) {
        ret = AFPERR_MISC;
        goto error_noctx;
    }
    gcry_mpi_print(GCRYMPI_FMT_USG, K_bin, PRIMEBITS/8, &nwritten, K);
    if (nwritten < PRIMEBITS/8) {
        memmove(K_bin + PRIMEBITS/8 - nwritten, K_bin, nwritten);
        memset(K_bin, 0, PRIMEBITS/8 - nwritten);
    }

    /* ... generate the MD5 hash of K. K_MD5hash is what we actually use ! */
    K_MD5hash = calloc(1, K_hash_len = gcry_md_get_algo_dlen(GCRY_MD_MD5));
    if (K_MD5hash == NULL) {
        ret = AFPERR_MISC;
        free(K_bin);
        K_bin = NULL;
        goto error_noctx;
    }
    gcry_md_hash_buffer(GCRY_MD_MD5, K_MD5hash, K_bin, PRIMEBITS/8);
    free(K_bin);
    K_bin = NULL;

    /* FIXME: To support the Reconnect UAM, we need to store this key somewhere */

    /* Set up our encryption context. */
    ctxerror = gcry_cipher_open( &ctx, GCRY_CIPHER_CAST5, GCRY_CIPHER_MODE_CBC, 0);
    if (gcry_err_code(ctxerror) != GPG_ERR_NO_ERROR) {
        ret = AFPERR_MISC;
        goto error_ctx;
    }
    /* Set key */
    ctxerror = gcry_cipher_setkey(ctx, K_MD5hash, K_hash_len);
    if (gcry_err_code(ctxerror) != GPG_ERR_NO_ERROR) {
        ret = AFPERR_MISC;
        goto error_ctx;
    }
    /* Set the initialization vector for client->server transfer. */
    ctxerror = gcry_cipher_setiv(ctx, dhx_c2siv, sizeof(dhx_c2siv));
    if (gcry_err_code(ctxerror) != GPG_ERR_NO_ERROR) {
        ret = AFPERR_MISC;
        goto error_ctx;
    }
    /* Finally: decrypt client's md5_K(client nonce, C2SIV) inplace */
    ctxerror = gcry_cipher_decrypt(ctx, ibuf, 16, NULL, 0);
    if (gcry_err_code(ctxerror) != GPG_ERR_NO_ERROR) {
        ret = AFPERR_MISC;
        goto error_ctx;
    }
    /* Pull out clients nonce */
    gcry_mpi_scan(&clientNonce, GCRYMPI_FMT_USG, ibuf, 16, NULL);
    /* Increment nonce */
    gcry_mpi_add_ui(clientNonce, clientNonce, 1);

    /* Generate our nonce and remember it for Logincont2 */
    gcry_create_nonce(serverNonce_bin, 16); /* We'll use this here */
    gcry_mpi_scan(&serverNonce, GCRYMPI_FMT_USG, serverNonce_bin, 16, NULL); /* For use in Logincont2 */

    /* ---- Start building reply packet ---- */

    /* Session ID + 1 first */
    uint16 = htons(ID+1);
    memcpy(rbuf, &uint16, sizeof(uint16_t));
    rbuf += 2;
    *rbuflen += 2;

    /* Client nonce + 1 */
    gcry_mpi_print(GCRYMPI_FMT_USG, (unsigned char *)rbuf, PRIMEBITS/8, NULL, clientNonce);
    /* Server nonce */
    memcpy(rbuf+16, serverNonce_bin, 16);

    /* Set the initialization vector for server->client transfer. */
    ctxerror = gcry_cipher_setiv(ctx, dhx_s2civ, sizeof(dhx_s2civ));
    if (gcry_err_code(ctxerror) != GPG_ERR_NO_ERROR) {
        ret = AFPERR_MISC;
        goto error_ctx;
    }
    /* Encrypt md5_K(clientNonce+1, serverNonce) inplace */
    ctxerror = gcry_cipher_encrypt(ctx, rbuf, 32, NULL, 0);
    if (gcry_err_code(ctxerror) != GPG_ERR_NO_ERROR) {
        ret = AFPERR_MISC;
        goto error_ctx;
    }
    rbuf += 32;
    *rbuflen += 32;

    ret = AFPERR_AUTHCONT;
    goto exit;

error_ctx:
    gcry_cipher_close(ctx);
error_noctx:
    gcry_mpi_release(serverNonce);
    free(K_MD5hash);
    K_MD5hash=NULL;
exit:
    gcry_mpi_release(K);
    gcry_mpi_release(Mb);
    gcry_mpi_release(Ra);
    gcry_mpi_release(p);
    gcry_mpi_release(clientNonce);
    return ret;
}

static int logincont2(void *obj _U_, struct passwd **uam_pwd,
                      char *ibuf, size_t ibuflen,
                      char *rbuf _U_, size_t *rbuflen)
{
#ifdef SHADOWPW
    struct spwd *sp;
#endif /* SHADOWPW */
    int ret;
    char *p;
    gcry_mpi_t retServerNonce;
    gcry_cipher_hd_t ctx;
    gcry_error_t ctxerror;

    *rbuflen = 0;
    retServerNonce = gcry_mpi_new(0);

    /* Packet size should be: Session ID + ServerNonce + Passwd buffer (evantually +10 extra bytes, see Apples Docs)*/
    if ((ibuflen != 2 + 16 + 256) && (ibuflen != 2 + 16 + 256 + 10)) {
        LOG(log_error, logtype_uams, "DHX2: Paket length not correct: %d. Should be 274 or 284.", ibuflen);
        ret = AFPERR_PARAM;
        goto error_noctx;
    }

    /* Set up our encryption context. */
    ctxerror = gcry_cipher_open( &ctx, GCRY_CIPHER_CAST5, GCRY_CIPHER_MODE_CBC, 0);
    if (gcry_err_code(ctxerror) != GPG_ERR_NO_ERROR) {
        ret = AFPERR_MISC;
        goto error_ctx;
    }
    /* Set key */
    ctxerror = gcry_cipher_setkey(ctx, K_MD5hash, K_hash_len);
    if (gcry_err_code(ctxerror) != GPG_ERR_NO_ERROR) {
        ret = AFPERR_MISC;
        goto error_ctx;
    }
    /* Set the initialization vector for client->server transfer. */
    ctxerror = gcry_cipher_setiv(ctx, dhx_c2siv, sizeof(dhx_c2siv));
    if (gcry_err_code(ctxerror) != GPG_ERR_NO_ERROR) {
        ret = AFPERR_MISC;
        goto error_ctx;
    }

    /* Skip Session ID */
    ibuf += 2;

    /* Finally: decrypt client's md5_K(serverNonce+1, passwor, C2SIV) inplace */
    ctxerror = gcry_cipher_decrypt(ctx, ibuf, 16+256, NULL, 0);
    if (gcry_err_code(ctxerror) != GPG_ERR_NO_ERROR) {
        ret = AFPERR_MISC;
        goto error_ctx;
    }
    /* Pull out nonce. Should be serverNonce+1 */
    gcry_mpi_scan(&retServerNonce, GCRYMPI_FMT_USG, ibuf, 16, NULL);
    gcry_mpi_sub_ui(retServerNonce, retServerNonce, 1);
    if ( gcry_mpi_cmp( serverNonce, retServerNonce) != 0) {
        /* We're hacked!  */
        ret = AFPERR_NOTAUTH;
        goto error_ctx;
    }
    ibuf += 16;         /* ibuf now point to passwd in cleartext */

    /* ---- Start authentication --- */
    ret = AFPERR_NOTAUTH;

    p = crypt( ibuf, dhxpwd->pw_passwd );
    memset(ibuf, 0, 255);
    if ( strcmp( p, dhxpwd->pw_passwd ) == 0 ) {
        *uam_pwd = dhxpwd;
        ret = AFP_OK;
    }

#ifdef SHADOWPW
    if (( sp = getspnam( dhxpwd->pw_name )) == NULL ) {
        LOG(log_info, logtype_uams, "no shadow passwd entry for %s", dhxpwd->pw_name);
        ret = AFPERR_NOTAUTH;
        goto exit;
    }

    /* check for expired password */
    if (sp && sp->sp_max != -1 && sp->sp_lstchg) {
        time_t now = time(NULL) / (60*60*24);
        int32_t expire_days = sp->sp_lstchg - now + sp->sp_max;
        if ( expire_days < 0 ) {
            LOG(log_info, logtype_uams, "password for user %s expired", dhxpwd->pw_name);
            ret = AFPERR_PWDEXPR;
            goto exit;
        }
    }
#endif /* SHADOWPW */

error_ctx:
    gcry_cipher_close(ctx);
error_noctx:
exit:
    free(K_MD5hash);
    K_MD5hash=NULL;
    gcry_mpi_release(serverNonce);
    gcry_mpi_release(retServerNonce);
    return ret;
}

static int passwd_logincont(void *obj, struct passwd **uam_pwd,
                            char *ibuf, size_t ibuflen,
                            char *rbuf, size_t *rbuflen)
{
    uint16_t retID;
    int ret;

    /* check for session id */
    memcpy(&retID, ibuf, sizeof(uint16_t));
    retID = ntohs(retID);
    if (retID == ID)
        ret = logincont1(obj, uam_pwd, ibuf, ibuflen, rbuf, rbuflen);
    else if (retID == ID+1)
        ret = logincont2(obj, uam_pwd, ibuf,ibuflen, rbuf, rbuflen);
    else {
        LOG(log_info, logtype_uams, "DHX2: Session ID Mismatch");
        ret = AFPERR_PARAM;
    }
    return ret;
}

static int uam_setup(const char *path)
{
    if (uam_register(UAM_SERVER_LOGIN_EXT, path, "DHX2", passwd_login,
                     passwd_logincont, NULL, passwd_login_ext) < 0)
        return -1;
    return 0;
}

static void uam_cleanup(void)
{
    uam_unregister(UAM_SERVER_LOGIN, "DHX2");
}


UAM_MODULE_EXPORT struct uam_export uams_dhx2 = {
    UAM_MODULE_SERVER,
    UAM_MODULE_VERSION,
    uam_setup, uam_cleanup
};


UAM_MODULE_EXPORT struct uam_export uams_dhx2_passwd = {
    UAM_MODULE_SERVER,
    UAM_MODULE_VERSION,
    uam_setup, uam_cleanup
};

#endif /* UAM_DHX2 */

