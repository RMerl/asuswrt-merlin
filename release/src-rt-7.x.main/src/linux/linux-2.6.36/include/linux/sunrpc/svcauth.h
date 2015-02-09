/*
 * linux/include/linux/sunrpc/svcauth.h
 *
 * RPC server-side authentication stuff.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef _LINUX_SUNRPC_SVCAUTH_H_
#define _LINUX_SUNRPC_SVCAUTH_H_

#ifdef __KERNEL__

#include <linux/string.h>
#include <linux/sunrpc/msg_prot.h>
#include <linux/sunrpc/cache.h>
#include <linux/hash.h>

#define SVC_CRED_NGROUPS	32
struct svc_cred {
	uid_t			cr_uid;
	gid_t			cr_gid;
	struct group_info	*cr_group_info;
};

struct svc_rqst;		/* forward decl */
struct in6_addr;

/* Authentication is done in the context of a domain.
 *
 * Currently, the nfs server uses the auth_domain to stand
 * for the "client" listed in /etc/exports.
 *
 * More generally, a domain might represent a group of clients using
 * a common mechanism for authentication and having a common mapping
 * between local identity (uid) and network identity.  All clients
 * in a domain have similar general access rights.  Each domain can
 * contain multiple principals which will have different specific right
 * based on normal Discretionary Access Control.
 *
 * A domain is created by an authentication flavour module based on name
 * only.  Userspace then fills in detail on demand.
 *
 * In the case of auth_unix and auth_null, the auth_domain is also
 * associated with entries in another cache representing the mapping
 * of ip addresses to the given client.
 */
struct auth_domain {
	struct kref		ref;
	struct hlist_node	hash;
	char			*name;
	struct auth_ops		*flavour;
};

/*
 * Each authentication flavour registers an auth_ops
 * structure.
 * name is simply the name.
 * flavour gives the auth flavour. It determines where the flavour is registered
 * accept() is given a request and should verify it.
 *   It should inspect the authenticator and verifier, and possibly the data.
 *    If there is a problem with the authentication *authp should be set.
 *    The return value of accept() can indicate:
 *      OK - authorised. client and credential are set in rqstp.
 *           reqbuf points to arguments
 *           resbuf points to good place for results.  verfier
 *             is (probably) already in place.  Certainly space is
 *	       reserved for it.
 *      DROP - simply drop the request. It may have been deferred
 *      GARBAGE - rpc garbage_args error
 *      SYSERR - rpc system_err error
 *      DENIED - authp holds reason for denial.
 *      COMPLETE - the reply is encoded already and ready to be sent; no
 *		further processing is necessary.  (This is used for processing
 *		null procedure calls which are used to set up encryption
 *		contexts.)
 *
 *   accept is passed the proc number so that it can accept NULL rpc requests
 *   even if it cannot authenticate the client (as is sometimes appropriate).
 *
 * release() is given a request after the procedure has been run.
 *  It should sign/encrypt the results if needed
 * It should return:
 *    OK - the resbuf is ready to be sent
 *    DROP - the reply should be quitely dropped
 *    DENIED - authp holds a reason for MSG_DENIED
 *    SYSERR - rpc system_err
 *
 * domain_release()
 *   This call releases a domain.
 * set_client()
 *   Givens a pending request (struct svc_rqst), finds and assigns
 *   an appropriate 'auth_domain' as the client.
 */
struct auth_ops {
	char *	name;
	struct module *owner;
	int	flavour;
	int	(*accept)(struct svc_rqst *rq, __be32 *authp);
	int	(*release)(struct svc_rqst *rq);
	void	(*domain_release)(struct auth_domain *);
	int	(*set_client)(struct svc_rqst *rq);
};

#define	SVC_GARBAGE	1
#define	SVC_SYSERR	2
#define	SVC_VALID	3
#define	SVC_NEGATIVE	4
#define	SVC_OK		5
#define	SVC_DROP	6
#define	SVC_DENIED	7
#define	SVC_PENDING	8
#define	SVC_COMPLETE	9


extern int	svc_authenticate(struct svc_rqst *rqstp, __be32 *authp);
extern int	svc_authorise(struct svc_rqst *rqstp);
extern int	svc_set_client(struct svc_rqst *rqstp);
extern int	svc_auth_register(rpc_authflavor_t flavor, struct auth_ops *aops);
extern void	svc_auth_unregister(rpc_authflavor_t flavor);

extern struct auth_domain *unix_domain_find(char *name);
extern void auth_domain_put(struct auth_domain *item);
extern int auth_unix_add_addr(struct in6_addr *addr, struct auth_domain *dom);
extern struct auth_domain *auth_domain_lookup(char *name, struct auth_domain *new);
extern struct auth_domain *auth_domain_find(char *name);
extern struct auth_domain *auth_unix_lookup(struct in6_addr *addr);
extern int auth_unix_forget_old(struct auth_domain *dom);
extern void svcauth_unix_purge(void);
extern void svcauth_unix_info_release(void *);
extern int svcauth_unix_set_client(struct svc_rqst *rqstp);

static inline unsigned long hash_str(char *name, int bits)
{
	unsigned long hash = 0;
	unsigned long l = 0;
	int len = 0;
	unsigned char c;
	do {
		if (unlikely(!(c = *name++))) {
			c = (char)len; len = -1;
		}
		l = (l << 8) | c;
		len++;
		if ((len & (BITS_PER_LONG/8-1))==0)
			hash = hash_long(hash^l, BITS_PER_LONG);
	} while (len);
	return hash >> (BITS_PER_LONG - bits);
}

static inline unsigned long hash_mem(char *buf, int length, int bits)
{
	unsigned long hash = 0;
	unsigned long l = 0;
	int len = 0;
	unsigned char c;
	do {
		if (len == length) {
			c = (char)len; len = -1;
		} else
			c = *buf++;
		l = (l << 8) | c;
		len++;
		if ((len & (BITS_PER_LONG/8-1))==0)
			hash = hash_long(hash^l, BITS_PER_LONG);
	} while (len);
	return hash >> (BITS_PER_LONG - bits);
}

#endif /* __KERNEL__ */

#endif /* _LINUX_SUNRPC_SVCAUTH_H_ */
