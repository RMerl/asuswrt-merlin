/* Copyright (c) 1999 Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifndef UAM_H
#define UAM_H 1

#include <pwd.h>
#include <stdarg.h>

#ifdef TRU64
#include <sia.h>
#include <siad.h>
#endif /* TRU64 */

/* just a label for exported bits */
#ifndef UAM_MODULE_EXPORT
#define UAM_MODULE_EXPORT 
#endif

/* type of uam */
#define UAM_MODULE_SERVER   	 1
#define UAM_MODULE_CLIENT  	 2

/* in case something drastic has to change */
#define UAM_MODULE_VERSION       1

/* things for which we can have uams */
#define UAM_SERVER_LOGIN         (1 << 0)
#define UAM_SERVER_CHANGEPW      (1 << 1)
#define UAM_SERVER_PRINTAUTH     (1 << 2) 
#define UAM_SERVER_LOGIN_EXT     (1 << 3)

/* options */
#define UAM_OPTION_USERNAME     (1 << 0) /* get space for username */
#define UAM_OPTION_GUEST        (1 << 1) /* get guest user */
#define UAM_OPTION_PASSWDOPT    (1 << 2) /* get the password file */
#define UAM_OPTION_SIGNATURE    (1 << 3) /* get server signature */
#define UAM_OPTION_RANDNUM      (1 << 4) /* request a random number */
#define UAM_OPTION_HOSTNAME     (1 << 5) /* get host name */
#define UAM_OPTION_COOKIE       (1 << 6) /* cookie handle */
#define UAM_OPTION_CLIENTNAME   (1 << 8) /* get client IP address */
#define UAM_OPTION_KRB5SERVICE  (1 << 9) /* service name for krb5 principal */
#define UAM_OPTION_MACCHARSET   (1 << 10) /* mac charset handle */
#define UAM_OPTION_UNIXCHARSET  (1 << 11) /* unix charset handle */
#define UAM_OPTION_SESSIONINFO  (1 << 12) /* unix charset handle */
#define UAM_OPTION_KRB5REALM    (1 << 13) /* krb realm */
#define UAM_OPTION_FQDN         (1 << 14) /* fully qualified name */

/* some password options. you pass these in the length parameter and
 * get back the corresponding option. not all of these are implemented. */
#define UAM_PASSWD_FILENAME     (1 << 0)
#define UAM_PASSWD_MINLENGTH    (1 << 1)
#define UAM_PASSWD_EXPIRETIME   (1 << 3) /* not implemented yet. */

/* max lenght of username  */
#define UAM_USERNAMELEN 	255

/* i'm doing things this way because os x server's dynamic linker
 * support is braindead. it also allows me to do a little versioning. */
struct uam_export {
  int uam_type, uam_version;
  int (*uam_setup)(const char *);
  void (*uam_cleanup)(void);
};

#define SESSIONKEY_LEN  64
#define SESSIONTOKEN_LEN 8

struct session_info {
  void    *sessionkey;          /* random session key */
  size_t  sessionkey_len;
  void    *cryptedkey;		/* kerberos/gssapi crypted key */
  size_t  cryptedkey_len;
  void    *sessiontoken;        /* session token sent to the client on FPGetSessionToken*/
  size_t  sessiontoken_len;
  void    *clientid;          /* whole buffer cotaining eg idlen, id and boottime */
  size_t  clientid_len;
};

/* register and unregister uams with these functions */
extern UAM_MODULE_EXPORT int uam_register (const int, const char *, const char *, ...);
extern UAM_MODULE_EXPORT void uam_unregister (const int, const char *);

/* helper functions */
extern UAM_MODULE_EXPORT struct passwd *uam_getname (void*, char *, const int);
extern UAM_MODULE_EXPORT int uam_checkuser (const struct passwd *);

/* afp helper functions */
extern UAM_MODULE_EXPORT int uam_afp_read (void *, char *, size_t *,
			     int (*)(void *, void *, const int));
extern UAM_MODULE_EXPORT int uam_afpserver_option (void *, const int, void *, size_t *);

#ifdef TRU64
extern void uam_afp_getcmdline (int *, char ***);
extern int uam_sia_validate_user (sia_collect_func_t *, int, char **,
                                     char *, char *, char *, int, char *,
                                     char *);
#endif /* TRU64 */

#endif
