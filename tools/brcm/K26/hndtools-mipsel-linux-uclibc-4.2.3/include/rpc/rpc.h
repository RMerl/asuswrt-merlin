/* @(#)rpc.h	2.3 88/08/10 4.0 RPCSRC; from 1.9 88/02/08 SMI */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * rpc.h, Just includes the billions of rpc header files necessary to
 * do remote procedure calling.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#ifndef _RPC_RPC_H
#define _RPC_RPC_H 1

#ifdef _LIBC
/* Some adjustments to make the libc source from glibc
 * compile more easily with uClibc... */
#ifndef __FORCE_GLIBC
#define __FORCE_GLIBC
#endif
#ifndef _GNU_SOUCE
#define _GNU_SOUCE
#endif
#define _(X)	X
#include <features.h>
#endif

#include <rpc/types.h>		/* some typedefs */
#include <netinet/in.h>

/* external data representation interfaces */
#include <rpc/xdr.h>		/* generic (de)serializer */

/* Client side only authentication */
#include <rpc/auth.h>		/* generic authenticator (client side) */

/* Client side (mostly) remote procedure call */
#include <rpc/clnt.h>		/* generic rpc stuff */

/* semi-private protocol headers */
#include <rpc/rpc_msg.h>	/* protocol for rpc messages */
#include <rpc/auth_unix.h>	/* protocol for unix style cred */
#include <rpc/auth_des.h>	/* protocol for des style cred */

/* Server side only remote procedure callee */
#include <rpc/svc.h>		/* service manager and multiplexer */
#include <rpc/svc_auth.h>	/* service side authenticator */

/*
 * COMMENT OUT THE NEXT INCLUDE IF RUNNING ON SUN OS OR ON A VERSION
 * OF UNIX BASED ON NFSSRC.  These systems will already have the structures
 * defined by <rpc/netdb.h> included in <netdb.h>.
 */
/* routines for parsing /etc/rpc */
#include <rpc/netdb.h>		/* structures and routines to parse /etc/rpc */

__BEGIN_DECLS

/* Global variables, protected for multi-threaded applications.  */
extern fd_set *__rpc_thread_svc_fdset (void) __attribute__ ((__const__));
#define svc_fdset (*__rpc_thread_svc_fdset ())

extern struct rpc_createerr *__rpc_thread_createerr (void)
     __attribute__ ((__const__));
#define get_rpc_createerr() (*__rpc_thread_createerr ())
/* The people who "engineered" RPC should bee punished for naming the
   data structure and the variable the same.  We cannot always define the
   macro 'rpc_createerr' because this would prevent people from defining
   object of type 'struct rpc_createerr'.  So we leave it up to the user
   to select transparent replacement also of this variable.  */
#ifdef _RPC_MT_VARS
# define rpc_createerr (*__rpc_thread_createerr ())
#endif

extern struct pollfd **__rpc_thread_svc_pollfd (void)
     __attribute__ ((__const__));
#define svc_pollfd (*__rpc_thread_svc_pollfd ())

extern int *__rpc_thread_svc_max_pollfd (void) __attribute__ ((__const__));
#define svc_max_pollfd (*__rpc_thread_svc_max_pollfd ())

__END_DECLS

#endif /* rpc/rpc.h */
