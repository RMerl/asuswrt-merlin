/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifndef AFPD_AUTH_H
#define AFPD_AUTH_H 1

#include <limits.h>

#include <atalk/globals.h>

struct afp_versions {
    char	*av_name;
    int		av_number;
};

static const struct afp_versions  afp_versions[] = {
    { "AFP2.2", 22 },
    { "AFPX03", 30 },
    { "AFP3.1", 31 },
    { "AFP3.2", 32 },
    { "AFP3.3", 33 }
};

/* for GetUserInfo */
#define USERIBIT_USER  (1 << 0)
#define USERIBIT_GROUP (1 << 1)
#define USERIBIT_UUID  (1 << 2)
#define USERIBIT_ALL   (USERIBIT_USER | USERIBIT_GROUP | USERIBIT_UUID)

extern uid_t    uuid;
#if defined( sun ) && !defined( __svr4__ ) || defined( ultrix )
extern int	*groups;
#else /*sun __svr4__ ultrix*/
extern gid_t	*groups;
#endif /*sun __svr4__ ultrix*/
extern int	ngroups;

/* FP functions */
int afp_login (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_login_ext (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_logincont (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_changepw (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_logout (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_getuserinfo (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_getsession (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_disconnect (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_zzz (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);

#endif /* auth.h */
