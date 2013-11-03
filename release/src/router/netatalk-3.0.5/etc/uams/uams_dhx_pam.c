/*
 *
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * Copyright (c) 1999 Adrian Sun (asun@u.washington.edu) 
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#if defined(USE_PAM) && defined(UAM_DHX)
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
#include <arpa/inet.h>

#if defined(GNUTLS_DHX)
#include <gnutls/openssl.h>
#elif defined(OPENSSL_DHX)
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/cast.h>
#include <openssl/err.h>
#else /* OPENSSL_DHX */
#include <bn.h>
#include <dh.h>
#include <cast.h>
#include <err.h>
#endif /* OPENSSL_DHX */

#include <atalk/afp.h>
#include <atalk/uam.h>

#define KEYSIZE 16
#define PASSWDLEN 64
#define CRYPTBUFLEN  (KEYSIZE*2)
#define CRYPT2BUFLEN (KEYSIZE + PASSWDLEN)
#define CHANGEPWBUFLEN (KEYSIZE + 2*PASSWDLEN)

/* hash a number to a 16-bit quantity */
#define dhxhash(a) ((((unsigned long) (a) >> 8) ^ \
		     (unsigned long) (a)) & 0xffff)

/* the secret key */
static CAST_KEY castkey;
static struct passwd *dhxpwd;
static uint8_t randbuf[KEYSIZE];

/* diffie-hellman bits */
static unsigned char msg2_iv[] = "CJalbert";
static unsigned char msg3_iv[] = "LWallace";
static const uint8_t p[] = {0xBA, 0x28, 0x73, 0xDF, 0xB0, 0x60, 0x57, 0xD4,
			     0x3F, 0x20, 0x24, 0x74, 0x4C, 0xEE, 0xE7, 0x5B};
static const uint8_t g = 0x07;


/* Static variables used to communicate between the conversation function
 * and the server_login function
 */
static pam_handle_t *pamh = NULL; 
static char *PAM_username;
static char *PAM_password;

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
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM DHX Conversation Err -- %s",
		  strerror(errno));
    /* Log Entry */
    return PAM_CONV_ERR;
  }

  reply = (struct pam_response *) 
    calloc(num_msg, sizeof(struct pam_response));

  if (!reply) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM DHX Conversation Err -- %s",
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
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: username failure -- %s",
		  strerror(errno));
    /* Log Entry */
	goto pam_fail_conv;
      }
      break;
    case PAM_PROMPT_ECHO_OFF:
      if (!(string = COPY_STRING(PAM_password))) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: passwd failure: --: %s",
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
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Binary_Prompt -- %s",
		  strerror(errno));
    /* Log Entry */
      goto pam_fail_conv;
    }

    if (string) {  
      reply[count].resp_retcode = 0;
      reply[count].resp = string;
      string = NULL;
    }
  }

  *resp = reply;
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: PAM Success");
    /* Log Entry */
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
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM DHX Conversation Err -- %s",
		  strerror(errno));
    /* Log Entry */
    return PAM_CONV_ERR;
}

static struct pam_conv PAM_conversation = {
  &PAM_conv,
  NULL
};


static int dhx_setup(void *obj, char *ibuf, size_t ibuflen _U_, 
		     char *rbuf, size_t *rbuflen)
{
    uint16_t sessid;
    size_t i;
    BIGNUM *bn, *gbn, *pbn;
    DH *dh;

    /* get the client's public key */
    if (!(bn = BN_bin2bn((unsigned char *)ibuf, KEYSIZE, NULL))) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM No Public Key -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }

    /* get our primes */
    if (!(gbn = BN_bin2bn(&g, sizeof(g), NULL))) {
      BN_clear_free(bn);
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM No Primes: GBN -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }

    if (!(pbn = BN_bin2bn(p, sizeof(p), NULL))) {
      BN_free(gbn);
      BN_clear_free(bn);
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM No Primes: PBN -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }

    /* okay, we're ready */
    if (!(dh = DH_new())) {
      BN_free(pbn);
      BN_free(gbn);
      BN_clear_free(bn);
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM DH was equal to DH_New... Go figure... -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }

    /* generate key and make sure that we have enough space */
    dh->p = pbn;
    dh->g = gbn;
    if (DH_generate_key(dh) == 0) {
	unsigned long dherror;
	char errbuf[256];

	ERR_load_crypto_strings();
	dherror = ERR_get_error();
	ERR_error_string_n(dherror, errbuf, 256);

	LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Err Generating Key (OpenSSL error code: %u, %s)", dherror, errbuf);

	ERR_free_strings();
	goto pam_fail;
    }
    if (BN_num_bytes(dh->pub_key) > KEYSIZE) {
	LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Err Generating Key -- Not enough Space? -- %s", strerror(errno));
	goto pam_fail;
    }

    /* figure out the key. store the key in rbuf for now. */
    i = DH_compute_key((unsigned char *)rbuf, bn, dh);
    
    /* set the key */
    CAST_set_key(&castkey, i, (unsigned char *)rbuf);
    
    /* session id. it's just a hashed version of the object pointer. */
    sessid = dhxhash(obj);
    memcpy(rbuf, &sessid, sizeof(sessid));
    rbuf += sizeof(sessid);
    *rbuflen += sizeof(sessid);
    
    /* public key */
    BN_bn2bin(dh->pub_key, (unsigned char *)rbuf); 
    rbuf += KEYSIZE;
    *rbuflen += KEYSIZE;

    /* buffer to be encrypted */
    i = sizeof(randbuf);
    if (uam_afpserver_option(obj, UAM_OPTION_RANDNUM, (void *) randbuf,
			     &i) < 0) {
      *rbuflen = 0;
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Buffer Encryption Err. -- %s",
		  strerror(errno));
    /* Log Entry */
      goto pam_fail;
    }    
    memcpy(rbuf, &randbuf, sizeof(randbuf));

    /* get the signature. it's always 16 bytes. */
#if 0
    if (uam_afpserver_option(obj, UAM_OPTION_SIGNATURE, 
			     (void *) &buf, NULL) < 0) {
      *rbuflen = 0;
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Signature Retieval Failure -- %s",
		  strerror(errno));
    /* Log Entry */
      goto pam_fail;
    }
    memcpy(rbuf + KEYSIZE, buf, KEYSIZE); 
#else /* 0 */
    memset(rbuf + KEYSIZE, 0, KEYSIZE); 
#endif /* 0 */

    /* encrypt using cast */
    CAST_cbc_encrypt((unsigned char *)rbuf, (unsigned char *)rbuf, CRYPTBUFLEN, &castkey, msg2_iv, 
		     CAST_ENCRYPT);
    *rbuflen += CRYPTBUFLEN;
    BN_free(bn);
    DH_free(dh);
    return AFPERR_AUTHCONT;

pam_fail:
    BN_free(bn);
    DH_free(dh);
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Fail - Cast Encryption -- %s",
		  strerror(errno));
    /* Log Entry */
    return AFPERR_PARAM;
}

/* -------------------------------- */
static int login(void *obj, char *username, int ulen,  struct passwd **uam_pwd _U_,
		     char *ibuf, size_t ibuflen,
		     char *rbuf, size_t *rbuflen)
{
    if (( dhxpwd = uam_getname(obj, username, ulen)) == NULL ) {
        LOG(log_info, logtype_uams, "uams_dhx_pam.c: unknown username [%s]", username);
        return AFPERR_NOTAUTH;
    }

    PAM_username = username;
    LOG(log_info, logtype_uams, "dhx login: %s", username);
    return dhx_setup(obj, ibuf, ibuflen, rbuf, rbuflen);
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
        LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: uam_afpserver_option didn't meet uam_option_username  -- %s",
		  strerror(errno));
        return AFPERR_PARAM;
    }

    len = (unsigned char) *ibuf++;
    if ( len > ulen ) {
        LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Signature Retieval Failure -- %s",
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
    int len;
    size_t ulen;
    uint16_t  temp16;

    *rbuflen = 0;

    /* grab some of the options */
    if (uam_afpserver_option(obj, UAM_OPTION_USERNAME, (void *) &username, &ulen) < 0) {
        LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: uam_afpserver_option didn't meet uam_option_username  -- %s",
		  strerror(errno));
        return AFPERR_PARAM;
    }

    if (*uname != 3)
        return AFPERR_PARAM;
    uname++;
    memcpy(&temp16, uname, sizeof(temp16));
    len = ntohs(temp16);

    if ( !len || len > ulen ) {
        LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Signature Retrieval Failure -- %s",
		  strerror(errno));
	return AFPERR_PARAM;
    }
    memcpy(username, uname +2, len );
    username[ len ] = '\0';

    return (login(obj, username, ulen, uam_pwd, ibuf, ibuflen, rbuf, rbuflen));
}

/* -------------------------------- */

static int pam_logincont(void *obj, struct passwd **uam_pwd,
			 char *ibuf, size_t ibuflen _U_, 
			 char *rbuf, size_t *rbuflen)
{
    const char *hostname;
    BIGNUM *bn1, *bn2, *bn3;
    uint16_t sessid;
    int err, PAM_error;

    *rbuflen = 0;

    /* check for session id */
    memcpy(&sessid, ibuf, sizeof(sessid));
    if (sessid != dhxhash(obj)) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM Session ID - DHXHash Mismatch -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }
    ibuf += sizeof(sessid);
    
    if (uam_afpserver_option(obj, UAM_OPTION_CLIENTNAME,
			     (void *) &hostname, NULL) < 0)
	{
	LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: unable to retrieve client hostname");
	hostname = NULL;
	}

    CAST_cbc_encrypt((unsigned char *)ibuf, (unsigned char *)rbuf, CRYPT2BUFLEN, &castkey,
		     msg3_iv, CAST_DECRYPT);
    memset(&castkey, 0, sizeof(castkey));

    /* check to make sure that the random number is the same. we
     * get sent back an incremented random number. */
    if (!(bn1 = BN_bin2bn((unsigned char *)rbuf, KEYSIZE, NULL)))
      return AFPERR_PARAM;

    if (!(bn2 = BN_bin2bn(randbuf, sizeof(randbuf), NULL))) {
      BN_free(bn1);
      return AFPERR_PARAM;
    }
      
    /* zero out the random number */
    memset(rbuf, 0, sizeof(randbuf));
    memset(randbuf, 0, sizeof(randbuf));
    rbuf += KEYSIZE;

    if (!(bn3 = BN_new())) {
      BN_free(bn2);
      BN_free(bn1);
      return AFPERR_PARAM;
    }

    BN_sub(bn3, bn1, bn2);
    BN_free(bn2);
    BN_free(bn1);

    /* okay. is it one more? */
    if (!BN_is_one(bn3)) {
      BN_free(bn3);
      return AFPERR_PARAM;
    }
    BN_free(bn3);

    /* Set these things up for the conv function */
    rbuf[PASSWDLEN] = '\0';
    PAM_password = rbuf;

    err = AFPERR_NOTAUTH;
    PAM_error = pam_start("netatalk", PAM_username, &PAM_conversation,
			  &pamh);
    if (PAM_error != PAM_SUCCESS) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: PAM_Error: %s",
		  pam_strerror(pamh,PAM_error));
    /* Log Entry */
      goto logincont_err;
    }

    /* solaris craps out if PAM_TTY and PAM_RHOST aren't set. */
    pam_set_item(pamh, PAM_TTY, "afpd");
    pam_set_item(pamh, PAM_RHOST, hostname);
    PAM_error = pam_authenticate(pamh,0);
    if (PAM_error != PAM_SUCCESS) {
      if (PAM_error == PAM_MAXTRIES) 
	err = AFPERR_PWDEXPR;
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: PAM_Error: %s",
		  pam_strerror(pamh, PAM_error));
    /* Log Entry */
      goto logincont_err;
    }      

    PAM_error = pam_acct_mgmt(pamh, 0);
    if (PAM_error != PAM_SUCCESS ) {
      /* Log Entry */
      LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: PAM_Error: %s",
	  pam_strerror(pamh, PAM_error));
      /* Log Entry */
      if (PAM_error == PAM_NEW_AUTHTOK_REQD)	/* password expired */
	err = AFPERR_PWDEXPR;
#ifdef PAM_AUTHTOKEN_REQD
      else if (PAM_error == PAM_AUTHTOKEN_REQD) 
	err = AFPERR_PWDCHNG;
#endif
      else
        goto logincont_err;
    }

#ifndef PAM_CRED_ESTABLISH
#define PAM_CRED_ESTABLISH PAM_ESTABLISH_CRED
#endif
    PAM_error = pam_setcred(pamh, PAM_CRED_ESTABLISH);
    if (PAM_error != PAM_SUCCESS) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: PAM_Error: %s",
		  pam_strerror(pamh, PAM_error));
    /* Log Entry */
      goto logincont_err;
    }

    PAM_error = pam_open_session(pamh, 0);
    if (PAM_error != PAM_SUCCESS) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: PAM_Error: %s",
		  pam_strerror(pamh, PAM_error));
    /* Log Entry */
      goto logincont_err;
    }

    memset(rbuf, 0, PASSWDLEN); /* zero out the password */
    *uam_pwd = dhxpwd;
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: PAM Auth OK!");
    /* Log Entry */
    if ( err == AFPERR_PWDEXPR)
	return err;
    return AFP_OK;

logincont_err:
    pam_end(pamh, PAM_error);
    pamh = NULL;
    memset(rbuf, 0, CRYPT2BUFLEN);
    return err;
}

/* logout */
static void pam_logout(void) {
    pam_close_session(pamh, 0);
    pam_end(pamh, 0);
    pamh = NULL;
}


/* change pw for dhx needs a couple passes to get everything all
 * right. basically, it's like the login/logincont sequence */
static int pam_changepw(void *obj, char *username,
			struct passwd *pwd _U_, char *ibuf, size_t ibuflen,
			char *rbuf, size_t *rbuflen)
{
    BIGNUM *bn1, *bn2, *bn3;

    char *hostname;
    pam_handle_t *lpamh;
    uid_t uid;
    uint16_t sessid;
    int PAM_error;

    if (ibuflen < sizeof(sessid)) {
      return AFPERR_PARAM;
    }

    /* grab the id */
    memcpy(&sessid, ibuf, sizeof(sessid));
    ibuf += sizeof(sessid);
    
    if (!sessid) {  /* no sessid -> initialization phase */
      PAM_username = username;
      ibuflen -= sizeof(sessid);
      return dhx_setup(obj, ibuf, ibuflen, rbuf, rbuflen);
    }


    /* otherwise, it's like logincont but different. */

    /* check out the session id */
    if (sessid != dhxhash(obj)) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Session ID not Equal to DHX Hash -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }

    /* we need this for pam */
    if (uam_afpserver_option(obj, UAM_OPTION_HOSTNAME,
			     (void *) &hostname, NULL) < 0) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Hostname Null?? -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_MISC;
    }

    /* grab the client's nonce, old password, and new password. */
    CAST_cbc_encrypt((unsigned char *)ibuf, (unsigned char *)ibuf, CHANGEPWBUFLEN, &castkey,
		     msg3_iv, CAST_DECRYPT);
    memset(&castkey, 0, sizeof(castkey));

    /* check to make sure that the random number is the same. we
     * get sent back an incremented random number. */
    if (!(bn1 = BN_bin2bn((unsigned char *)ibuf, KEYSIZE, NULL))) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Random Number Not the same or not incremented-- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }

    if (!(bn2 = BN_bin2bn(randbuf, sizeof(randbuf), NULL))) {
      BN_free(bn1);
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Random Number Not the same or not incremented -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }
      
    /* zero out the random number */
    memset(rbuf, 0, sizeof(randbuf));
    memset(randbuf, 0, sizeof(randbuf));

    if (!(bn3 = BN_new())) {
      BN_free(bn2);
      BN_free(bn1);
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Random Number did not Zero -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }

    BN_sub(bn3, bn1, bn2);
    BN_free(bn2);
    BN_free(bn1);

    /* okay. is it one more? */
#if 0
    if (!BN_is_one(bn3)) {
      BN_free(bn3);
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: After Random Number not Zero, is it one more? -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }
#endif
    BN_free(bn3);

    /* Set these things up for the conv function. the old password
     * is at the end. */
    ibuf += KEYSIZE;
    ibuf[PASSWDLEN + PASSWDLEN] = '\0';
    PAM_password = ibuf + PASSWDLEN;

    PAM_error = pam_start("netatalk", username, &PAM_conversation,
			  &lpamh);
    if (PAM_error != PAM_SUCCESS) {
    /* Log Entry */
           LOG(log_info, logtype_uams, "uams_dhx_pam.c :PAM: Needless to say, PAM_error is != to PAM_SUCCESS -- %s",
		  strerror(errno));
    /* Log Entry */
      return AFPERR_PARAM;
    }
    pam_set_item(lpamh, PAM_TTY, "afpd");
    pam_set_item(lpamh, PAM_RHOST, hostname);

    /* we might need to do this as root */
    uid = geteuid();
    seteuid(0);
    PAM_error = pam_authenticate(lpamh, 0);
    if (PAM_error != PAM_SUCCESS) {
      seteuid(uid);
      pam_end(lpamh, PAM_error);
      return AFPERR_NOTAUTH;
    }

    /* clear out old passwd */
    memset(ibuf + PASSWDLEN, 0, PASSWDLEN);

    /* new password */
    PAM_password = ibuf;
    ibuf[PASSWDLEN] = '\0';

    /* this really does need to be done as root */
    PAM_error = pam_chauthtok(lpamh, 0);
    seteuid(uid); /* un-root ourselves. */
    memset(ibuf, 0, PASSWDLEN);
    if (PAM_error != PAM_SUCCESS) {
      pam_end(lpamh, PAM_error);
      return AFPERR_ACCESS;
    }

    pam_end(lpamh, 0);
    return AFP_OK;
}


static int uam_setup(const char *path)
{
  if (uam_register(UAM_SERVER_LOGIN_EXT, path, "DHCAST128", pam_login, 
		   pam_logincont, pam_logout, pam_login_ext) < 0)
    return -1;

  if (uam_register(UAM_SERVER_CHANGEPW, path, "DHCAST128", 
		   pam_changepw) < 0) {
    uam_unregister(UAM_SERVER_LOGIN, "DHCAST128");
    return -1;
  }

  /*uam_register(UAM_SERVER_PRINTAUTH, path, "DHCAST128",
    pam_printer);*/

  return 0;
}

static void uam_cleanup(void)
{
  uam_unregister(UAM_SERVER_LOGIN, "DHCAST128");
  uam_unregister(UAM_SERVER_CHANGEPW, "DHCAST128");
  /*uam_unregister(UAM_SERVER_PRINTAUTH, "DHCAST128"); */
}

UAM_MODULE_EXPORT struct uam_export uams_dhx = {
  UAM_MODULE_SERVER,
  UAM_MODULE_VERSION,
  uam_setup, uam_cleanup
};

UAM_MODULE_EXPORT struct uam_export uams_dhx_pam = {
  UAM_MODULE_SERVER,
  UAM_MODULE_VERSION,
  uam_setup, uam_cleanup
};

#endif /* USE_PAM && UAM_DHX */

