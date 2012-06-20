#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifndef _USLEEP_H
#define _USLEEP_H

#if !defined(HAVE_USLEEP)
void usleep (unsigned long usec);
#endif /* HAVE_USLEEP */

#endif /* _USLEEP_H */
