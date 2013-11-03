/*
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifndef AFPD_VOLUME_H
#define AFPD_VOLUME_H 1

#include <sys/types.h>
#include <arpa/inet.h>

#include <atalk/volume.h>
#include <atalk/cnid.h>
#include <atalk/unicode.h>
#include <atalk/globals.h>

extern int              ustatfs_getvolspace (const struct vol *,
                                             VolSpace *, VolSpace *,
                                             uint32_t *);
extern void             setvoltime (AFPObj *, struct vol *);
extern int              pollvoltime (AFPObj *);

/* FP functions */
int afp_openvol      (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_getvolparams (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_setvolparams (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_getsrvrparms (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_closevol     (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);

/* netatalk functions */
extern void close_all_vol(const AFPObj *obj);
extern void closevol(const AFPObj *obj, struct vol *vol);
#endif
