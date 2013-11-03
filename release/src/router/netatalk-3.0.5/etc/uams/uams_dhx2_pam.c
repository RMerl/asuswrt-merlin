/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * Copyright (c) 1999 Adrian Sun (asun@u.washington.edu)
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#if defined (USE_PAM) && defined (UAM_DHX2)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atalk/logger.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <errno.h>
#ifdef HAVE_SECURITY_PAM_APPL_H
#include <security/pam_appl.h>
#endif
#ifdef HAVE_PAM_PAM_APPL_H
#include <pam/pam_appl.h>
#endif


#ifdef HAVE_LIBGCRYPT
#include <gcrypt.h>
#endif /* HAVE_LIBGCRYPT */

#include <atalk/afp.h>
#include <atalk/uam.h>
#include <atalk/globals.h>

/* Number of bits for p which we generate. Everybode out there uses 512, so we beet them */
#define PRIMEBITS 1024

/* hash a number to a 16-bit quantity */
#define dhxhash(a) ((((unsigned long) (a) >> 8) ^   \
                     (unsigned long) (a)) & 0xffff)

/* Some parameters need be maintained across calls */
static gcry_mpi_t p, g, Ra;
static gcry_mpi_t serverNonce;
static char *K_MD5hash = NULL;
static int K_hash_len;
static uint16_t ID;

/* The initialization vectors for CAST128 are fixed by Apple. */
static unsigned char dhx_c2siv[] = { 'L', 'W', 'a', 'l', 'l', 'a', 'c', 'e' };
static unsigned char dhx_s2civ[] = { 'C', 'J', 'a', 'l', 'b', 'e', 'r', 't' };

/* Static variables used to communicate between the conversation function
 * and the server_login function */
static pam_handle_t *pamh = NULL;
static char *PAM_username;
static char *PAM_password;
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
static int dh_params_generate (unsigned int bits) {

    int result, times = 0, qbits;
    gcry_mpi_t *factors = NULL;
    gcry_error_t err;

    /* Version check should be the very first call because it
       makes sure that important subsystems are intialized. */
    if (!gcry_check_version (GCRYPT_VERSION)) {
        LOG(log_error, logtype_uams, "PAM DHX2: libgcrypt versions mismatch. Need: %s", GCRYPT_VERSION);
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
            gcry_mpi_release(p);
            gcry_prime_release_factors (factors);
        }
        err = gcry_prime_generate(&p, bits, qbits, &factors, NULL, NULL,
                                  GCRY_STRONG_RANDOM, GCRY_PRIME_FLAG_SPECIAL_FACTOR);
        if (err != 0) {
            result = AFPERR_MISC;
            goto error;
        }
        err = gcry_prime_check(p, 0);
        times++;
    } while (err != 0 && times < 10);

    if (err != 0) {
        result = AFPERR_MISC;
        goto error;
    }

    /* generate the group generator. */
    err = gcry_prime_group_generator(&g, p, factors, NULL);
    if (err != 0) {
        result = AFPERR_MISC;
        goto error;
    }

    gcry_prime_release_factors(factors);

    return 0;

error:
    gcry_prime_release_factors(factors);

    return result;
}


/* PAM conversation function
 * Here we assume (for now, at least) that echo on means login name, and
 * echo off means password.
 */
static int PAM_conv (int num_msg,
#ifdef LINUX
                     const struct pam_message **msg,
#else
                     struct pam_message **msg,
#endif
                     struct pam_response **resp,
                     void *appdata_ptr _U_) {
    int count = 0;
    struct pam_response *reply;

#define COPY_STRING(s) (s) ? strdup(s) : NULL

    errno = 0;

    if (num_msg < 1) {
        /* Log Entry */
        LOG(log_info, logtype_uams, "PAM DHX2 Conversation Err -- %s",
            strerror(errno));
        /* Log Entry */
        return PAM_CONV_ERR;
    }

    reply = (struct pam_response *)
        calloc(num_msg, sizeof(struct pam_response));

    if (!reply) {
        /* Log Entry */
        LOG(log_info, logtype_uams, "PAM DHX2: Conversation Err -- %s",
            strerror(errno));
        /* Log Entry */
        return PAM_CONV_ERR;
    }

    for (count = 0; count < num_msg; count++) {
        char *string = NULL;

        switch (msg[count]->msg_style) {
        case PAM_PROMPT_ECHO_ON:
            if (!(string = COPY_STRING(PAM_username))) {
                /* Log Entry */
                LOG(log_info, logtype_uams, "PAM DHX2: username failure -- %s",
                    strerror(errno));
                /* Log Entry */
                goto pam_fail_conv;
            }
            break;
        case PAM_PROMPT_ECHO_OFF:
            if (!(string = COPY_STRING(PAM_password))) {
                /* Log Entry */
                LOG(log_info, logtype_uams, "PAM DHX2: passwd failure: --: %s",
                    strerror(errno));
                /* Log Entry */
                goto pam_fail_conv;
            }
            break;
        case PAM_TEXT_INFO:
#ifdef PAM_BINARY_PROMPT
        case PAM_BINARY_PROMPT:
#endif /* PAM_BINARY_PROMPT */
            /* ignore it... */
            break;
        case PAM_ERROR_MSG:
        default:
            LOG(log_info, logtype_uams, "PAM DHX2: Binary_Prompt -- %s", strerror(errno));
            goto pam_fail_conv;
        }

        if (string) {
            reply[count].resp_retcode = 0;
            reply[count].resp = string;
            string = NULL;
        }
    }

    *resp = reply;
    LOG(log_info, logtype_uams, "PAM DHX2: PAM Success");
    return PAM_SUCCESS;

pam_fail_conv:
    for (count = 0; count < num_msg; count++) {
        if (!reply[count].resp)
            continue;
        switch (msg[count]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON:
            free(reply[count].resp);
            break;
        }
    }
    free(reply);
    /* Log Entry */
    LOG(log_info, logtype_uams, "PAM DHX2: Conversation Err -- %s",
        strerror(errno));
    /* Log Entry */
    return PAM_CONV_ERR;
}

static struct pam_conv PAM_conversation = {
    &PAM_conv,
    NULL
};


static int dhx2_setup(void *obj, char *ibuf _U_, size_t ibuflen _U_,
                      char *rbuf, size_t *rbuflen)
{
    int ret;
    size_t nwritten;
    gcry_mpi_t Ma;
    char *Ra_binary = NULL;
    uint16_t uint16;

    *rbuflen = 0;

    Ra = gcry_mpi_new(0);
    Ma = gcry_mpi_new(0);

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
    /* We will need Ra later, but mustn't forget to release it ! */
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

    PAM_username = username;
    LOG(log_info, logtype_uams, "DHX2 login: %s", username);
    return dhx2_setup(obj, ibuf, ibuflen, rbuf, rbuflen);
}

/* -------------------------------- */
/* dhx login: things are done in a slightly bizarre order to avoid
 * having to clean things up if there's an error. */
static int pam_login(void *obj, struct passwd **uam_pwd,
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
static int pam_login_ext(void *obj, char *uname, struct passwd **uam_pwd,
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
static int logincont1(void *obj _U_, char *ibuf, size_t ibuflen, char *rbuf, size_t *rbuflen)
{
    int ret;
    size_t nwritten;
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
    gcry_mpi_release(clientNonce);
    return ret;
}

/**
 * Try to authenticate via PAM as "adminauthuser"
 **/
static int loginasroot(const char *adminauthuser, const char **hostname, int status)
{
    int PAM_error;

    if ((PAM_error = pam_end(pamh, status)) != PAM_SUCCESS)
        goto exit;
    pamh = NULL;

    if ((PAM_error = pam_start("netatalk", adminauthuser, &PAM_conversation, &pamh)) != PAM_SUCCESS) {
        LOG(log_info, logtype_uams, "DHX2: PAM_Error: %s", pam_strerror(pamh,PAM_error));
        goto exit;
    }

    /* solaris craps out if PAM_TTY and PAM_RHOST aren't set. */
    pam_set_item(pamh, PAM_TTY, "afpd");
    pam_set_item(pamh, PAM_RHOST, *hostname);
    if ((PAM_error = pam_authenticate(pamh, 0)) != PAM_SUCCESS)
        goto exit;

    LOG(log_warning, logtype_uams, "DHX2: Authenticated as \"%s\"", adminauthuser);

exit:
    return PAM_error;
}

static int logincont2(void *obj_in, struct passwd **uam_pwd,
                      char *ibuf, size_t ibuflen,
                      char *rbuf _U_, size_t *rbuflen)
{
    AFPObj *obj = obj_in;
    int ret = AFPERR_MISC;
    int PAM_error;
    const char *hostname = NULL;
    gcry_mpi_t retServerNonce;
    gcry_cipher_hd_t ctx;
    gcry_error_t ctxerror;
    char *utfpass = NULL;

    *rbuflen = 0;

    /* Packet size should be: Session ID + ServerNonce + Passwd buffer (evantually +10 extra bytes, see Apples Docs) */
    if ((ibuflen != 2 + 16 + 256) && (ibuflen != 2 + 16 + 256 + 10)) {
        LOG(log_error, logtype_uams, "DHX2: Paket length not correct: %u. Should be 274 or 284.", ibuflen);
        ret = AFPERR_PARAM;
        goto error_noctx;
    }

    retServerNonce = gcry_mpi_new(0);

    /* For PAM */
    uam_afpserver_option(obj, UAM_OPTION_CLIENTNAME, (void *) &hostname, NULL);

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
    ibuf += 16;

    /* ---- Start authentication with PAM --- */

    /* The password is in legacy Mac encoding, convert it to host encoding */
    if (convert_string_allocate(CH_MAC, CH_UNIX, ibuf, -1, &utfpass) == (size_t)-1) {
        LOG(log_error, logtype_uams, "DHX2: conversion error");
        goto error_ctx;
    }
    PAM_password = utfpass;

#ifdef DEBUG
    LOG(log_maxdebug, logtype_default, "DHX2: password: %s", PAM_password);
#endif

    /* Set these things up for the conv function */

    ret = AFPERR_NOTAUTH;
    PAM_error = pam_start("netatalk", PAM_username, &PAM_conversation, &pamh);
    if (PAM_error != PAM_SUCCESS) {
        LOG(log_info, logtype_uams, "DHX2: PAM_Error: %s", pam_strerror(pamh,PAM_error));
        goto error_ctx;
    }

    /* solaris craps out if PAM_TTY and PAM_RHOST aren't set. */
    pam_set_item(pamh, PAM_TTY, "afpd");
    pam_set_item(pamh, PAM_RHOST, hostname);
    pam_set_item(pamh, PAM_RUSER, PAM_username);

    PAM_error = pam_authenticate(pamh, 0);
    if (PAM_error != PAM_SUCCESS) {
        if (PAM_error == PAM_MAXTRIES)
            ret = AFPERR_PWDEXPR;
        LOG(log_info, logtype_uams, "DHX2: PAM_Error: %s", pam_strerror(pamh, PAM_error));

        if (!obj->options.adminauthuser)
            goto error_ctx;
        if (loginasroot(obj->options.adminauthuser, &hostname, PAM_error) != PAM_SUCCESS) {
            goto error_ctx;
        }
    }

    PAM_error = pam_acct_mgmt(pamh, 0);
    if (PAM_error != PAM_SUCCESS ) {
        LOG(log_info, logtype_uams, "DHX2: PAM_Error: %s",
            pam_strerror(pamh, PAM_error));
        if (PAM_error == PAM_NEW_AUTHTOK_REQD)    /* password expired */
            ret = AFPERR_PWDEXPR;
#ifdef PAM_AUTHTOKEN_REQD
        else if (PAM_error == PAM_AUTHTOKEN_REQD)
            ret = AFPERR_PWDCHNG;
#endif
        goto error_ctx;
    }

#ifndef PAM_CRED_ESTABLISH
#define PAM_CRED_ESTABLISH PAM_ESTABLISH_CRED
#endif
    PAM_error = pam_setcred(pamh, PAM_CRED_ESTABLISH);
    if (PAM_error != PAM_SUCCESS) {
        LOG(log_info, logtype_uams, "DHX2: PAM_Error: %s",
            pam_strerror(pamh, PAM_error));
        goto error_ctx;
    }

    PAM_error = pam_open_session(pamh, 0);
    if (PAM_error != PAM_SUCCESS) {
        LOG(log_info, logtype_uams, "DHX2: PAM_Error: %s",
            pam_strerror(pamh, PAM_error));
        goto error_ctx;
    }

    memset(ibuf, 0, 256); /* zero out the password */
    if (utfpass)
        memset(utfpass, 0, strlen(utfpass));
    *uam_pwd = dhxpwd;
    LOG(log_info, logtype_uams, "DHX2: PAM Auth OK!");

    ret = AFP_OK;

error_ctx:
    gcry_cipher_close(ctx);
error_noctx:
    if (utfpass) free(utfpass);
    free(K_MD5hash);
    K_MD5hash=NULL;
    gcry_mpi_release(serverNonce);
    gcry_mpi_release(retServerNonce);
    return ret;
}

static int pam_logincont(void *obj, struct passwd **uam_pwd,
                         char *ibuf, size_t ibuflen,
                         char *rbuf, size_t *rbuflen)
{
    uint16_t retID;
    int ret;

    /* check for session id */
    memcpy(&retID, ibuf, sizeof(uint16_t));
    retID = ntohs(retID);
    if (retID == ID)
        ret = logincont1(obj, ibuf, ibuflen, rbuf, rbuflen);
    else if (retID == ID+1)
        ret = logincont2(obj, uam_pwd, ibuf,ibuflen, rbuf, rbuflen);
    else {
        LOG(log_info, logtype_uams, "DHX2: Session ID Mismatch");
        ret = AFPERR_PARAM;
    }
    return ret;
}


/* logout */
static void pam_logout(void) {
    pam_close_session(pamh, 0);
    pam_end(pamh, 0);
    pamh = NULL;
}

/****************************
 * --- Change pwd stuff --- */

static int changepw_1(void *obj, char *uname,
                      char *ibuf, size_t ibuflen, char *rbuf, size_t *rbuflen)
{
    *rbuflen = 0;

    /* Remember it now, use it in changepw_3 */
    PAM_username = uname;
    return( dhx2_setup(obj, ibuf, ibuflen, rbuf, rbuflen) );
}

static int changepw_2(void *obj,
                      char *ibuf, size_t ibuflen, char *rbuf, size_t *rbuflen)
{
    return( logincont1(obj, ibuf, ibuflen, rbuf, rbuflen) );
}

static int changepw_3(void *obj _U_,
                      char *ibuf, size_t ibuflen _U_,
                      char *rbuf _U_, size_t *rbuflen _U_)
{
    int ret;
    int PAM_error;
    uid_t uid;
    pam_handle_t *lpamh;
    const char *hostname = NULL;
    gcry_mpi_t retServerNonce;
    gcry_cipher_hd_t ctx;
    gcry_error_t ctxerror;

    *rbuflen = 0;

    LOG(log_error, logtype_uams, "DHX2 ChangePW: packet 3 processing");

    /* Packet size should be: Session ID + ServerNonce + 2*Passwd buffer */
    if (ibuflen != 2 + 16 + 2*256) {
        LOG(log_error, logtype_uams, "DHX2: Paket length not correct");
        ret = AFPERR_PARAM;
        goto error_noctx;
    }

    retServerNonce = gcry_mpi_new(0);

    /* For PAM */
    uam_afpserver_option(obj, UAM_OPTION_CLIENTNAME, (void *) &hostname, NULL);

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

    /* Finally: decrypt client's md5_K(serverNonce+1, 2*password, C2SIV) inplace */
    ctxerror = gcry_cipher_decrypt(ctx, ibuf, 16+2*256, NULL, 0);
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
    ibuf += 16;

    /* ---- Start pwd changing with PAM --- */
    ibuf[255] = '\0';       /* For safety */
    ibuf[511] = '\0';

    /* check if new and old password are equal */
    if (memcmp(ibuf, ibuf + 256, 255) == 0) {
        LOG(log_info, logtype_uams, "DHX2 Chgpwd: new and old password are equal");
        ret = AFPERR_PWDSAME;
        goto error_ctx;
    }

    /* Set these things up for the conv function. PAM_username was set in changepw_1 */
    PAM_password = ibuf + 256;
    PAM_error = pam_start("netatalk", PAM_username, &PAM_conversation, &lpamh);
    if (PAM_error != PAM_SUCCESS) {
        LOG(log_info, logtype_uams, "DHX2 Chgpwd: PAM error in pam_start");
        ret = AFPERR_PARAM;
        goto error_ctx;
    }
    pam_set_item(lpamh, PAM_TTY, "afpd");
    uam_afpserver_option(obj, UAM_OPTION_CLIENTNAME, (void *) &hostname, NULL);
    pam_set_item(lpamh, PAM_RHOST, hostname);
    uid = geteuid();
    seteuid(0);
    PAM_error = pam_authenticate(lpamh,0);
    if (PAM_error != PAM_SUCCESS) {
        LOG(log_info, logtype_uams, "DHX2 Chgpwd: error authenticating with PAM");
        seteuid(uid);
        pam_end(lpamh, PAM_error);
        ret = AFPERR_NOTAUTH;
        goto error_ctx;
    }
    PAM_password = ibuf;
    PAM_error = pam_chauthtok(lpamh, 0);
    seteuid(uid); /* un-root ourselves. */
    memset(ibuf, 0, 512);
    if (PAM_error != PAM_SUCCESS) {
        LOG(log_info, logtype_uams, "DHX2 Chgpwd: error changing pw with PAM");
        pam_end(lpamh, PAM_error);
        ret = AFPERR_ACCESS;
        goto error_ctx;
    }
    pam_end(lpamh, 0);
    ret = AFP_OK;

error_ctx:
    gcry_cipher_close(ctx);
error_noctx:
    free(K_MD5hash);
    K_MD5hash=NULL;
    gcry_mpi_release(serverNonce);
    gcry_mpi_release(retServerNonce);
    return ret;
}

static int dhx2_changepw(void *obj _U_, char *uname,
                         struct passwd *pwd _U_, char *ibuf, size_t ibuflen _U_,
                         char *rbuf _U_, size_t *rbuflen _U_)
{
    /* We use this to serialize the three incoming FPChangePassword calls */
    static int dhx2_changepw_status = 1;

    int ret = AFPERR_NOTAUTH;  /* gcc can't figure out it's always initialized */

    switch (dhx2_changepw_status) {
    case 1:
        ret = changepw_1( obj, uname, ibuf, ibuflen, rbuf, rbuflen);
        if ( ret == AFPERR_AUTHCONT)
            dhx2_changepw_status = 2;
        break;
    case 2:
        ret = changepw_2( obj, ibuf, ibuflen, rbuf, rbuflen);
        if ( ret == AFPERR_AUTHCONT)
            dhx2_changepw_status = 3;
        else
            dhx2_changepw_status = 1;
        break;
    case 3:
        ret = changepw_3( obj, ibuf, ibuflen, rbuf, rbuflen);
        dhx2_changepw_status = 1; /* Whether is was succesfull or not: we
                                     restart anyway !*/
        break;
    }
    return ret;
}

static int uam_setup(const char *path)
{
    if (uam_register(UAM_SERVER_LOGIN_EXT, path, "DHX2", pam_login,
                     pam_logincont, pam_logout, pam_login_ext) < 0)
        return -1;
    if (uam_register(UAM_SERVER_CHANGEPW, path, "DHX2", dhx2_changepw) < 0)
        return -1;

    LOG(log_debug, logtype_uams, "DHX2: generating mersenne primes");
    /* Generate p and g for DH */
    if (dh_params_generate(PRIMEBITS) != 0) {
        LOG(log_error, logtype_uams, "DHX2: Couldn't generate p and g");
        return -1;
    }

    return 0;
}

static void uam_cleanup(void)
{
    uam_unregister(UAM_SERVER_LOGIN, "DHX2");
    uam_unregister(UAM_SERVER_CHANGEPW, "DHX2");

    LOG(log_debug, logtype_uams, "DHX2: uam_cleanup");

    gcry_mpi_release(p);
    gcry_mpi_release(g);
}


UAM_MODULE_EXPORT struct uam_export uams_dhx2 = {
    UAM_MODULE_SERVER,
    UAM_MODULE_VERSION,
    uam_setup, uam_cleanup
};


UAM_MODULE_EXPORT struct uam_export uams_dhx2_pam = {
    UAM_MODULE_SERVER,
    UAM_MODULE_VERSION,
    uam_setup, uam_cleanup
};

#endif /* USE_PAM && UAM_DHX2 */
