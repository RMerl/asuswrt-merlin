/* @(#)auth.h	2.3 88/08/07 4.0 RPCSRC; from 1.17 88/02/08 SMI */
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
 * auth.h, Authentication interface.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * The data structures are completely opaque to the client.  The client
 * is required to pass a AUTH * to routines that create rpc
 * "sessions".
 */

#ifndef _RPC_AUTH_H

#define _RPC_AUTH_H	1
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
#endif
#include <features.h>
#include <rpc/xdr.h>

__BEGIN_DECLS

#define MAX_AUTH_BYTES	400
#define MAXNETNAMELEN	255	/* maximum length of network user's name */

/*
 * Status returned from authentication check
 */
enum auth_stat {
	AUTH_OK=0,
	/*
	 * failed at remote end
	 */
	AUTH_BADCRED=1,			/* bogus credentials (seal broken) */
	AUTH_REJECTEDCRED=2,		/* client should begin new session */
	AUTH_BADVERF=3,			/* bogus verifier (seal broken) */
	AUTH_REJECTEDVERF=4,		/* verifier expired or was replayed */
	AUTH_TOOWEAK=5,			/* rejected due to security reasons */
	/*
	 * failed locally
	*/
	AUTH_INVALIDRESP=6,		/* bogus response verifier */
	AUTH_FAILED=7			/* some unknown reason */
};

union des_block {
	struct {
		u_int32_t high;
		u_int32_t low;
	} key;
	char c[8];
};
typedef union des_block des_block;
extern bool_t xdr_des_block (XDR *__xdrs, des_block *__blkp) __THROW;

/*
 * Authentication info.  Opaque to client.
 */
struct opaque_auth {
	enum_t	oa_flavor;		/* flavor of auth */
	caddr_t	oa_base;		/* address of more auth stuff */
	u_int	oa_length;		/* not to exceed MAX_AUTH_BYTES */
};

/*
 * Auth handle, interface to client side authenticators.
 */
typedef struct AUTH AUTH;
struct AUTH {
  struct opaque_auth ah_cred;
  struct opaque_auth ah_verf;
  union des_block ah_key;
  struct auth_ops {
    void (*ah_nextverf) (AUTH *);
    int  (*ah_marshal) (AUTH *, XDR *);		/* nextverf & serialize */
    int  (*ah_validate) (AUTH *, struct opaque_auth *);
						/* validate verifier */
    int  (*ah_refresh) (AUTH *);		/* refresh credentials */
    void (*ah_destroy) (AUTH *); 	    	/* destroy this structure */
  } *ah_ops;
  caddr_t ah_private;
};


/*
 * Authentication ops.
 * The ops and the auth handle provide the interface to the authenticators.
 *
 * AUTH	*auth;
 * XDR	*xdrs;
 * struct opaque_auth verf;
 */
#define AUTH_NEXTVERF(auth)		\
		((*((auth)->ah_ops->ah_nextverf))(auth))
#define auth_nextverf(auth)		\
		((*((auth)->ah_ops->ah_nextverf))(auth))

#define AUTH_MARSHALL(auth, xdrs)	\
		((*((auth)->ah_ops->ah_marshal))(auth, xdrs))
#define auth_marshall(auth, xdrs)	\
		((*((auth)->ah_ops->ah_marshal))(auth, xdrs))

#define AUTH_VALIDATE(auth, verfp)	\
		((*((auth)->ah_ops->ah_validate))((auth), verfp))
#define auth_validate(auth, verfp)	\
		((*((auth)->ah_ops->ah_validate))((auth), verfp))

#define AUTH_REFRESH(auth)		\
		((*((auth)->ah_ops->ah_refresh))(auth))
#define auth_refresh(auth)		\
		((*((auth)->ah_ops->ah_refresh))(auth))

#define AUTH_DESTROY(auth)		\
		((*((auth)->ah_ops->ah_destroy))(auth))
#define auth_destroy(auth)		\
		((*((auth)->ah_ops->ah_destroy))(auth))


extern struct opaque_auth _null_auth;


/*
 * These are the various implementations of client side authenticators.
 */

/*
 * Unix style authentication
 * AUTH *authunix_create(machname, uid, gid, len, aup_gids)
 *	char *machname;
 *	int uid;
 *	int gid;
 *	int len;
 *	int *aup_gids;
 */
extern AUTH *authunix_create (char *__machname, __uid_t __uid, __gid_t __gid,
			      int __len, __gid_t *__aup_gids);
extern AUTH *authunix_create_default (void);
extern AUTH *authnone_create (void) __THROW;
extern AUTH *authdes_create (const char *__servername, u_int __window,
			     struct sockaddr *__syncaddr, des_block *__ckey)
     __THROW;
extern AUTH *authdes_pk_create (const char *, netobj *, u_int,
				struct sockaddr *, des_block *) __THROW;


#define AUTH_NONE	0		/* no authentication */
#define	AUTH_NULL	0		/* backward compatibility */
#define	AUTH_SYS	1		/* unix style (uid, gids) */
#define	AUTH_UNIX	AUTH_SYS
#define	AUTH_SHORT	2		/* short hand unix style */
#define AUTH_DES	3		/* des style (encrypted timestamps) */
#define AUTH_DH		AUTH_DES	/* Diffie-Hellman (this is DES) */
#define AUTH_KERB       4               /* kerberos style */

/*
 *  Netname manipulating functions
 *
 */
extern int getnetname (char *) __THROW;
extern int host2netname (char *, __const char *, __const char *) __THROW;
extern int user2netname (char *, __const uid_t, __const char *) __THROW;
extern int netname2user (__const char *, uid_t *, gid_t *, int *, gid_t *)
     __THROW;
extern int netname2host (__const char *, char *, __const int) __THROW;

/*
 *
 * These routines interface to the keyserv daemon
 *
 */
extern int key_decryptsession (char *, des_block *);
extern int key_decryptsession_pk (char *, netobj *, des_block *);
extern int key_encryptsession (char *, des_block *);
extern int key_encryptsession_pk (char *, netobj *, des_block *);
extern int key_gendes (des_block *);
extern int key_setsecret (char *);
extern int key_secretkey_is_set (void);
extern int key_get_conv (char *, des_block *);

/*
 * XDR an opaque authentication struct.
 */
extern bool_t xdr_opaque_auth (XDR *, struct opaque_auth *) __THROW;

__END_DECLS

#endif /* rpc/auth.h */
