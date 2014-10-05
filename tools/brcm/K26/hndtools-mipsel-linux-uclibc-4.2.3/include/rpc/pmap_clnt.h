/* @(#)pmap_clnt.h	2.1 88/07/29 4.0 RPCSRC; from 1.11 88/02/08 SMI */
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
 * pmap_clnt.h
 * Supplies C routines to get to portmap services.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#ifndef _RPC_PMAP_CLNT_H
#define _RPC_PMAP_CLNT_H	1

#include <features.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/clnt.h>

__BEGIN_DECLS

typedef bool_t (*resultproc_t) (caddr_t resp, struct sockaddr_in *raddr);

/*
 * Usage:
 *	success = pmap_set(program, version, protocol, port);
 *	success = pmap_unset(program, version);
 *	port = pmap_getport(address, program, version, protocol);
 *	head = pmap_getmaps(address);
 *	clnt_stat = pmap_rmtcall(address, program, version, procedure,
 *		xdrargs, argsp, xdrres, resp, tout, port_ptr)
 *		(works for udp only.)
 * 	clnt_stat = clnt_broadcast(program, version, procedure,
 *		xdrargs, argsp,	xdrres, resp, eachresult)
 *		(like pmap_rmtcall, except the call is broadcasted to all
 *		locally connected nets.  For each valid response received,
 *		the procedure eachresult is called.  Its form is:
 *	done = eachresult(resp, raddr)
 *		bool_t done;
 *		caddr_t resp;
 *		struct sockaddr_in raddr;
 *		where resp points to the results of the call and raddr is the
 *		address if the responder to the broadcast.
 */

extern bool_t pmap_set (__const u_long __program, __const u_long __vers,
			int __protocol, u_short __port) __THROW;
extern bool_t pmap_unset (__const u_long __program, __const u_long __vers)
     __THROW;
extern struct pmaplist *pmap_getmaps (struct sockaddr_in *__address) __THROW;
extern enum clnt_stat pmap_rmtcall (struct sockaddr_in *__addr,
				    __const u_long __prog,
				    __const u_long __vers,
				    __const u_long __proc,
				    xdrproc_t __xdrargs,
				    caddr_t __argsp, xdrproc_t __xdrres,
				    caddr_t __resp, struct timeval __tout,
				    u_long *__port_ptr) __THROW;
extern enum clnt_stat clnt_broadcast (__const u_long __prog,
				      __const u_long __vers,
				      __const u_long __proc, xdrproc_t __xargs,
				      caddr_t __argsp, xdrproc_t __xresults,
				      caddr_t __resultsp,
				      resultproc_t __eachresult) __THROW;
extern u_short pmap_getport (struct sockaddr_in *__address,
			     __const u_long __program,
			     __const u_long __version, u_int __protocol)
     __THROW;

__END_DECLS

#endif /* rpc/pmap_clnt.h */
