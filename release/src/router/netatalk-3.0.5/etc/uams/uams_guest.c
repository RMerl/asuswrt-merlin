/*
 *
 * (c) 2001 (see COPYING)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>

#include <atalk/logger.h>
#include <atalk/afp.h>
#include <atalk/uam.h>
#include <atalk/util.h>
#include <atalk/compat.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif /* MIN */

/*XXX in etc/papd/file.h */
struct papfile;
extern UAM_MODULE_EXPORT void append(struct papfile *, const char *, int);

/* login and login_ext are almost the same */
static int noauth_login(void *obj, struct passwd **uam_pwd,
			char *ibuf _U_, size_t ibuflen _U_, 
			char *rbuf _U_, size_t *rbuflen)
{
    struct passwd *pwent;
    char *guest, *username;

    *rbuflen = 0;
    LOG(log_info, logtype_uams, "login noauth" );

    if (uam_afpserver_option(obj, UAM_OPTION_GUEST, (void *) &guest,
			     NULL) < 0)
      return AFPERR_MISC;

    if (uam_afpserver_option(obj, UAM_OPTION_USERNAME, 
			     (void *) &username, NULL) < 0)
      return AFPERR_MISC;

    strcpy(username, guest);
    if ((pwent = getpwnam(guest)) == NULL) {
	LOG(log_error, logtype_uams, "noauth_login: getpwnam( %s ): %s",
		guest, strerror(errno) );
	return( AFPERR_BADUAM );
    }

#ifdef AFS
    if ( setpag() < 0 ) {
	LOG(log_error, logtype_uams, "noauth_login: setpag: %s", strerror(errno) );
	return( AFPERR_BADUAM );
    }
#endif /* AFS */

    *uam_pwd = pwent;
    return( AFP_OK );
}

static int noauth_login_ext(void *obj, char *uname _U_, struct passwd **uam_pwd,
                     char *ibuf, size_t ibuflen,
                     char *rbuf, size_t *rbuflen)
{
        return ( noauth_login (obj, uam_pwd, ibuf, ibuflen, rbuf, rbuflen));
}


/* Printer NoAuthUAM Login */
static int noauth_printer(char *start, char *stop, char *username, struct papfile *out)
{
    char	*data, *p, *q;
    static const char *loginok = "0\r";

    data = (char *)malloc(stop - start + 1);
    if (!data) {
	LOG(log_info, logtype_uams,"Bad Login NoAuthUAM: malloc");
	return(-1);
    }

    strlcpy(data, start, stop - start + 1);

    /* We are looking for the following format in data:
     * (username)
     *
     * Hopefully username doesn't contain a ")"
     */

    if ((p = strchr(data, '(' )) == NULL) {
	LOG(log_info, logtype_uams,"Bad Login NoAuthUAM: username not found in string");
	free(data);
	return(-1);
    }
    p++;
    if ((q = strchr(p, ')' )) == NULL) {
	LOG(log_info, logtype_uams,"Bad Login NoAuthUAM: username not found in string");
	free(data);
	return(-1);
    }
    memcpy(username, p,  MIN( UAM_USERNAMELEN, q - p ));

    /* Done copying username, clean up */
    free(data);

    if (getpwnam(username) == NULL) {
	LOG(log_info, logtype_uams, "Bad Login NoAuthUAM: %s: %s",
	       username, strerror(errno) );
	return(-1);
    }

    /* Login successful */
    append(out, loginok, strlen(loginok));
    LOG(log_info, logtype_uams, "Login NoAuthUAM: %s", username);
    return(0);
}


static int uam_setup(const char *path)
{
  if (uam_register(UAM_SERVER_LOGIN_EXT, path, "No User Authent",
                   noauth_login, NULL, NULL, noauth_login_ext) < 0)
        return -1;

  if (uam_register(UAM_SERVER_PRINTAUTH, path, "NoAuthUAM",
		noauth_printer) < 0)
	return -1;

  return 0;
}

static void uam_cleanup(void)
{
  uam_unregister(UAM_SERVER_LOGIN, "No User Authent");
  uam_unregister(UAM_SERVER_PRINTAUTH, "NoAuthUAM");
}

UAM_MODULE_EXPORT struct uam_export uams_guest = {
  UAM_MODULE_SERVER,
  UAM_MODULE_VERSION,
  uam_setup, uam_cleanup
};
