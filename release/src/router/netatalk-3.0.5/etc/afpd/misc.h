#ifndef AFPD_MISC_H
#define AFPD_MISC_H 1

#include <atalk/globals.h>

/* FP functions */
/* messages.c */
int	afp_getsrvrmesg (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);

/* afs.c */
#ifdef AFS
int	afp_getdiracl (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int	afp_setdiracl (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
#else /* AFS */
#define afp_getdiracl	NULL
#define afp_setdiracl	NULL
#endif /* AFS */

#if defined( AFS ) && defined( UAM_AFSKRB )
int	afp_afschangepw (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
#else /* AFS && UAM_AFSKRB */
#define afp_afschangepw	NULL
#endif /* AFS && UAM_AFSKRB */

#endif
