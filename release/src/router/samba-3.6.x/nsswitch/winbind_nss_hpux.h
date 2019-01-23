/*
   Unix SMB/CIFS implementation.

   Donated by HP to enable Winbindd to build on HPUX 11.x.
   Copyright (C) Jeremy Allison 2002.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _WINBIND_NSS_HPUX_H
#define _WINBIND_NSS_HPUX_H

#include <nsswitch.h>

#define NSS_STATUS_SUCCESS     NSS_SUCCESS
#define NSS_STATUS_NOTFOUND    NSS_NOTFOUND
#define NSS_STATUS_UNAVAIL     NSS_UNAVAIL
#define NSS_STATUS_TRYAGAIN    NSS_TRYAGAIN

#ifdef HAVE_SYNCH_H
#include <synch.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

typedef enum {
	NSS_SUCCESS,
	NSS_NOTFOUND,
	NSS_UNAVAIL,
	NSS_TRYAGAIN
} nss_status_t;

typedef nss_status_t NSS_STATUS;

struct nss_backend;

typedef nss_status_t (*nss_backend_op_t)(struct nss_backend *, void *args);

struct nss_backend {
	nss_backend_op_t *ops;
	int n_ops;
};
typedef struct nss_backend nss_backend_t;
typedef int nss_dbop_t;

#include <errno.h>
#include <netdb.h>
#include <limits.h>

#ifndef NSS_INCLUDE_UNSAFE
#define NSS_INCLUDE_UNSAFE      1       /* Build old, MT-unsafe interfaces, */
#endif  /* NSS_INCLUDE_UNSAFE */

enum nss_netgr_argn {
	NSS_NETGR_MACHINE,
	NSS_NETGR_USER,
	NSS_NETGR_DOMAIN,
	NSS_NETGR_N
};

enum nss_netgr_status {
	NSS_NETGR_FOUND,
	NSS_NETGR_NO,
	NSS_NETGR_NOMEM
};

typedef unsigned nss_innetgr_argc;
typedef char **nss_innetgr_argv;

struct nss_innetgr_1arg {
	nss_innetgr_argc argc;
	nss_innetgr_argv argv;
};

typedef struct {
	void *result;        /* "result" parameter to getXbyY_r() */
	char *buffer;        /* "buffer"     "             "      */
	int buflen;         /* "buflen"     "             "      */
} nss_XbyY_buf_t;

extern nss_XbyY_buf_t *_nss_XbyY_buf_alloc(int struct_size, int buffer_size);
extern void _nss_XbyY_buf_free(nss_XbyY_buf_t *);

union nss_XbyY_key {
	uid_t uid;
	gid_t gid;
	const char *name;
	int number;
	struct {
		long net;
		int type;
	} netaddr;
	struct {
		const char *addr;
		int len;
		int type;
	} hostaddr;
	struct {
		union {
			const char *name;
			int port;
		} serv;
		const char *proto;
	} serv;
	void *ether;
};

typedef struct nss_XbyY_args {
	nss_XbyY_buf_t  buf;
	int stayopen;
	/*
	 * Support for setXXXent(stayopen)
	 * Used only in hosts, protocols,
	 * networks, rpc, and services.
	 */
	int (*str2ent)(const char *instr, int instr_len, void *ent, char *buffer, int buflen);
	union nss_XbyY_key key;

	void *returnval;
	int erange;
	/*
	*  h_errno is defined as function call macro for multithreaded applications
	*  in HP-UX. *this* h_errno is not used in the HP-UX codepath of our nss
	*  modules, so let's simply rename it:
	*/
	int h_errno_unused;
	nss_status_t status;
} nss_XbyY_args_t;

#endif /* _WINBIND_NSS_HPUX_H */
