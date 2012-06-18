/*
 * Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * OS Portability header file. try to map some
 * "standard" routines into OS-specific routines.
 *
 */

#ifndef _OSPORT_H_
#define _OSPORT_H_

#if defined(SOLARIS)

# define index(x, y)        strchr(x, y)
# define bcopy(S1, S2, LEN) ((void)memmove(S2, S1, LEN))
# define bzero(S1, LEN)     ((void)memset(S1,  0, LEN))
# define bcmp(S1,S2,LEN)    ((memcmp(S2, S1, LEN)==0)?0:1)

/* pre 2.6 solaris didn't include random(), etc prototypes 
 * <stdlib.h> (as of 2.6) has the correct prototypes.
 */

# if SOLARIS < 260
#  define random(X)          ((int)rand(X))
#  define srandom(X)         ((void)srand(X))
# endif /* SOLARIS < 260 */

#endif /* defined(SOLARIS) */

#endif /* _OSPORT_H_ */
