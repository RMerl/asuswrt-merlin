#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifndef _WRAPPERS_H
#define _WRAPPERS_H

#if !defined(HAVE_USLEEP)
#include "usleep.h"
#endif /* HAVE_USLEEP */

#if !defined(HAVE_GETOPT_LONG)
#include "getopt.h"
#endif /* HAVE_GETOPT_LONG */

#include "ether.h"

#endif /* _WRAPPERS_H */
