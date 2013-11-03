/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * Copyright (c) 1999 Adrian Sun (asun@u.washington.edu) 
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <atalk/standards.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif /* ! HAVE_CRYPT_H */
#include <sys/time.h>
#include <time.h>
#include <pwd.h>
#include <arpa/inet.h>

#ifdef SHADOWPW
#include <shadow.h>
#endif /* SHADOWPW */

#if defined(GNUTLS_DHX)
#include <gnutls/openssl.h>
#elif defined(OPENSSL_DHX)
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/cast.h>
#else /* OPENSSL_DHX */
#include <bn.h>
#include <dh.h>
#include <cast.h>
#endif /* OPENSSL_DHX */

#include <atalk/logger.h>
#include <atalk/afp.h>
#include <atalk/uam.h>

#define KEYSIZE 16
#define PASSWDLEN 64
#define CRYPTBUFLEN  (KEYSIZE*2)
#define CRYPT2BUFLEN (KEYSIZE + PASSWDLEN)

/* hash a number to a 16-bit quantity */
#define dhxhash(a) ((((unsigned long) (a) >> 8) ^ \
		     (unsigned long) (a)) & 0xffff)

/* the secret key */
static CAST_KEY castkey;
static struct passwd *dhxpwd;
static uint8_t randbuf[16];

#ifdef TRU64
#include <sia.h>
#include <siad.h>

static const char *clientname;
#endif /* TRU64 */

/* dhx passwd */
static int pwd_login(void *obj, char *username, int ulen, struct passwd **uam_pwd _U_,
			char *ibuf, size_t ibuflen _U_,
			char *rbuf, size_t *rbuflen)
{
    unsigned char iv[] = "CJalbert";
    uint8_t p[] = {0xBA, 0x28, 0x73, 0xDF, 0xB0, 0x60, 0x57, 0xD4,
		    0x3F, 0x20, 0x24, 0x74, 0x4C, 0xEE, 0xE7, 0x5B };
    uint8_t g = 0x07;
#ifdef SHADOWPW
    struct spwd *sp;
#endif /* SHADOWPW */
    BIGNUM *bn, *gbn, *pbn;
    uint16_t sessid;
    size_t i;
    DH *dh;

#ifdef TRU64
    int rnd_seed[256];
    for (i = 0; i < 256; i++)
        rnd_seed[i] = random();
    RAND_seed(rnd_seed, sizeof(rnd_seed));
#endif /* TRU64 */

    *rbuflen = 0;

#ifdef TRU64
    if( uam_afpserver_option( obj, UAM_OPTION_CLIENTNAME,
                              (void *) &clientname, NULL ) < 0 )
        return AFPERR_PARAM;
#endif /* TRU64 */

    if (( dhxpwd = uam_getname(obj, username, ulen)) == NULL ) {
        return AFPERR_NOTAUTH;
    }
    
    LOG(log_info, logtype_uams, "dhx login: %s", username);
    if (uam_checkuser(dhxpwd) < 0)
      return AFPERR_NOTAUTH;

#ifdef SHADOWPW
    if (( sp = getspnam( dhxpwd->pw_name )) == NULL ) {
	LOG(log_info, logtype_uams, "no shadow passwd entry for %s", username);
	return AFPERR_NOTAUTH;
    }
    dhxpwd->pw_passwd = sp->sp_pwdp;
#endif /* SHADOWPW */

    if (!dhxpwd->pw_passwd)
      return AFPERR_NOTAUTH;

    /* get the client's public key */
    if (!(bn = BN_bin2bn((unsigned char *)ibuf, KEYSIZE, NULL))) {
      return AFPERR_PARAM;
    }

    /* get our primes */
    if (!(gbn = BN_bin2bn(&g, sizeof(g), NULL))) {
      BN_free(bn);
      return AFPERR_PARAM;
    }

    if (!(pbn = BN_bin2bn(p, sizeof(p), NULL))) {
      BN_free(gbn);
      BN_free(bn);
      return AFPERR_PARAM;
    }

    /* okay, we're ready */
    if (!(dh = DH_new())) {
      BN_free(pbn);
      BN_free(gbn);
      BN_free(bn);
      return AFPERR_PARAM;
    }

    /* generate key and make sure we have enough space */
    dh->p = pbn;
    dh->g = gbn;
    if (!DH_generate_key(dh) || (BN_num_bytes(dh->pub_key) > KEYSIZE)) {
      goto passwd_fail;
    }

    /* figure out the key. use rbuf as a temporary buffer. */
    i = DH_compute_key((unsigned char *)rbuf, bn, dh);
    
    /* set the key */
    CAST_set_key(&castkey, i, (unsigned char *)rbuf);
    
    /* session id. it's just a hashed version of the object pointer. */
    sessid = dhxhash(obj);
    memcpy(rbuf, &sessid, sizeof(sessid));
    rbuf += sizeof(sessid);
    *rbuflen += sizeof(sessid);
    
    /* send our public key */
    BN_bn2bin(dh->pub_key, (unsigned char *)rbuf); 
    rbuf += KEYSIZE;
    *rbuflen += KEYSIZE;

    /* buffer to be encrypted */
    i = sizeof(randbuf);
    if (uam_afpserver_option(obj, UAM_OPTION_RANDNUM, (void *) randbuf,
			     &i) < 0) {
      *rbuflen = 0;
      goto passwd_fail;
    }    
    memcpy(rbuf, &randbuf, sizeof(randbuf));

#if 0
    /* get the signature. it's always 16 bytes. */
    if (uam_afpserver_option(obj, UAM_OPTION_SIGNATURE, 
			     (void *) &name, NULL) < 0) {
      *rbuflen = 0;
      goto passwd_fail;
    }
    memcpy(rbuf + KEYSIZE, name, KEYSIZE); 
#else /* 0 */
    memset(rbuf + KEYSIZE, 0, KEYSIZE);
#endif /* 0 */

    /* encrypt using cast */
    CAST_cbc_encrypt((unsigned char *)rbuf, (unsigned char *)rbuf, CRYPTBUFLEN, &castkey, iv, CAST_ENCRYPT);
    *rbuflen += CRYPTBUFLEN;
    BN_free(bn);
    DH_free(dh);
    return AFPERR_AUTHCONT;

passwd_fail:
    BN_free(bn);
    DH_free(dh);
    return AFPERR_PARAM;
}

/* cleartxt login */
static int passwd_login(void *obj, struct passwd **uam_pwd,
			char *ibuf, size_t ibuflen,
			char *rbuf, size_t *rbuflen)
{
    char *username;
    size_t len, ulen;

    *rbuflen = 0;

    if (uam_afpserver_option(obj, UAM_OPTION_USERNAME,
			     (void *) &username, &ulen) < 0)
	return AFPERR_MISC;

    if (ibuflen < 2) {
	return( AFPERR_PARAM );
    }

    len = (unsigned char) *ibuf++;
    ibuflen--;
    if (!len || len > ibuflen || len > ulen ) {
	return( AFPERR_PARAM );
    }
    memcpy(username, ibuf, len );
    ibuf += len;
    ibuflen -=len;
    username[ len ] = '\0';

    if ((unsigned long) ibuf & 1) { /* pad character */
	++ibuf;
	ibuflen--;
    }
    return (pwd_login(obj, username, ulen, uam_pwd, ibuf, ibuflen, rbuf, rbuflen));
    
}

/* cleartxt login ext 
 * uname format :
    byte      3
    2 bytes   len (network order)
    len bytes utf8 name
*/
static int passwd_login_ext(void *obj, char *uname, struct passwd **uam_pwd,
			char *ibuf, size_t ibuflen,
			char *rbuf, size_t *rbuflen)
{
    char       *username;
    size_t     len, ulen;
    uint16_t  temp16;

    *rbuflen = 0;
    
    if (uam_afpserver_option(obj, UAM_OPTION_USERNAME,
			     (void *) &username, &ulen) < 0)
	return AFPERR_MISC;

    if (*uname != 3)
	return AFPERR_PARAM;
    uname++;
    memcpy(&temp16, uname, sizeof(temp16));
    len = ntohs(temp16);
    if (!len || len > ulen ) {
	return( AFPERR_PARAM );
    }
    memcpy(username, uname +2, len );
    username[ len ] = '\0';
    return (pwd_login(obj, username, ulen, uam_pwd, ibuf, ibuflen, rbuf, rbuflen));
}
			
static int passwd_logincont(void *obj, struct passwd **uam_pwd,
			    char *ibuf, size_t ibuflen _U_, 
			    char *rbuf, size_t *rbuflen)
{
#ifdef SHADOWPW
    struct spwd *sp;
#endif /* SHADOWPW */
    unsigned char iv[] = "LWallace";
    BIGNUM *bn1, *bn2, *bn3;
    uint16_t sessid;
    char *p;
    int err = AFPERR_NOTAUTH;

    *rbuflen = 0;

    /* check for session id */
    memcpy(&sessid, ibuf, sizeof(sessid));
    if (sessid != dhxhash(obj))
      return AFPERR_PARAM;
    ibuf += sizeof(sessid);
   
    /* use rbuf as scratch space */
    CAST_cbc_encrypt((unsigned char *)ibuf, (unsigned char *)rbuf, CRYPT2BUFLEN, &castkey,
		     iv, CAST_DECRYPT);
    
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

    rbuf[PASSWDLEN] = '\0';
#ifdef TRU64
    {
        int ac;
        char **av;
        char hostname[256];

        uam_afp_getcmdline( &ac, &av );
        sprintf( hostname, "%s@%s", dhxpwd->pw_name, clientname );

        if( uam_sia_validate_user( NULL, ac, av, hostname, dhxpwd->pw_name,
                                   NULL, FALSE, NULL, rbuf ) != SIASUCCESS )
            return AFPERR_NOTAUTH;

        memset( rbuf, 0, PASSWDLEN );
        *uam_pwd = dhxpwd;
        return AFP_OK;
    }
#else /* TRU64 */
    p = crypt( rbuf, dhxpwd->pw_passwd );
    memset(rbuf, 0, PASSWDLEN);
    if ( strcmp( p, dhxpwd->pw_passwd ) == 0 ) {
      *uam_pwd = dhxpwd;
      err = AFP_OK;
    }
#ifdef SHADOWPW
    if (( sp = getspnam( dhxpwd->pw_name )) == NULL ) {
	LOG(log_info, logtype_uams, "no shadow passwd entry for %s", dhxpwd->pw_name);
	return (AFPERR_NOTAUTH);
    }

    /* check for expired password */
    if (sp && sp->sp_max != -1 && sp->sp_lstchg) {
        time_t now = time(NULL) / (60*60*24);
        int32_t expire_days = sp->sp_lstchg - now + sp->sp_max;
        if ( expire_days < 0 ) {
                LOG(log_info, logtype_uams, "password for user %s expired", dhxpwd->pw_name);
		err = AFPERR_PWDEXPR;
        }
    }
#endif /* SHADOWPW */
    return err;
#endif /* TRU64 */

    return AFPERR_NOTAUTH;
}


static int uam_setup(const char *path)
{
  if (uam_register(UAM_SERVER_LOGIN_EXT, path, "DHCAST128",
		   passwd_login, passwd_logincont, NULL, passwd_login_ext) < 0)
    return -1;
  /*uam_register(UAM_SERVER_PRINTAUTH, path, "DHCAST128",
    passwd_printer);*/

  return 0;
}

static void uam_cleanup(void)
{
  uam_unregister(UAM_SERVER_LOGIN, "DHCAST128");
  /*uam_unregister(UAM_SERVER_PRINTAUTH, "DHCAST128"); */
}

UAM_MODULE_EXPORT struct uam_export uams_dhx = {
  UAM_MODULE_SERVER,
  UAM_MODULE_VERSION,
  uam_setup, uam_cleanup
};

UAM_MODULE_EXPORT struct uam_export uams_dhx_passwd = {
  UAM_MODULE_SERVER,
  UAM_MODULE_VERSION,
  uam_setup, uam_cleanup
};
