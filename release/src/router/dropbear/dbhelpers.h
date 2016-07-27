#ifndef DROPBEAR_DBHELPERS_H_
#define DROPBEAR_DBHELPERS_H_

/* This header defines some things that are also used by libtomcrypt/math. 
   We avoid including normal include.h since that can result in conflicting 
   definitinos - only include config.h */
#include "config.h"

#ifdef __GNUC__
#define ATTRIB_PRINTF(fmt,args) __attribute__((format(printf, fmt, args))) 
#define ATTRIB_NORETURN __attribute__((noreturn))
#define ATTRIB_SENTINEL __attribute__((sentinel))
#else
#define ATTRIB_PRINTF(fmt,args)
#define ATTRIB_NORETURN
#define ATTRIB_SENTINEL
#endif

void m_burn(void* data, unsigned int len);

#endif /* DROPBEAR_DBHELPERS_H_ */
