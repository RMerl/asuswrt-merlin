/*
 * define path names
 *
 * $Id: pathnames.h,v 1.18 2005/08/25 23:59:34 paulus Exp $
 */

#ifdef HAVE_PATHS_H
#include <paths.h>

#else /* HAVE_PATHS_H */
#ifndef _PATH_VARRUN
#define _PATH_VARRUN 	"/var/run/"
#endif
#define _PATH_DEVNULL	"/dev/null"
#endif /* HAVE_PATHS_H */

#ifndef _ROOT_PATH
#define _ROOT_PATH	"/tmp"
#endif

#define _PATH_UPAPFILE 	 _ROOT_PATH "/ppp/pap-secrets"
#define _PATH_CHAPFILE 	 _ROOT_PATH "/ppp/chap-secrets"
#define _PATH_SRPFILE 	 _ROOT_PATH "/ppp/srp-secrets"
#define _PATH_SYSOPTIONS _ROOT_PATH "/ppp/options"
#define _PATH_IPUP	 _ROOT_PATH "/ppp/ip-up"
#define _PATH_IPDOWN	 _ROOT_PATH "/ppp/ip-down"
#define _PATH_IPPREUP	 _ROOT_PATH "/ppp/ip-pre-up"
#define _PATH_AUTHUP	 _ROOT_PATH "/ppp/auth-up"
#define _PATH_AUTHDOWN	 _ROOT_PATH "/ppp/auth-down"
#define _PATH_AUTHFAIL	 _ROOT_PATH "/ppp/auth-fail"
#define _PATH_TTYOPT	 _ROOT_PATH "/ppp/options."
#define _PATH_CONNERRS	 _ROOT_PATH "/var/log/ppp-connect-errors"
#define _PATH_PEERFILES	 _ROOT_PATH "/ppp/peers/"
#define _PATH_RESOLV	 _ROOT_PATH "/ppp/resolv.conf"

#define _PATH_USEROPT	 ".ppprc"
#define	_PATH_PSEUDONYM	 ".ppp_pseudonym"

#ifdef INET6
#define _PATH_IPV6UP     _ROOT_PATH "/ppp/ipv6-up"
#define _PATH_IPV6DOWN   _ROOT_PATH "/ppp/ipv6-down"
#endif

#ifdef IPX_CHANGE
#define _PATH_IPXUP	 _ROOT_PATH "/ppp/ipx-up"
#define _PATH_IPXDOWN	 _ROOT_PATH "/ppp/ipx-down"
#endif /* IPX_CHANGE */

#ifdef __STDC__
#define _PATH_PPPDB	_ROOT_PATH _PATH_VARRUN "pppd2.tdb"
#else /* __STDC__ */
#ifdef HAVE_PATHS_H
#define _PATH_PPPDB	"/var/run/pppd2.tdb"
#else
#define _PATH_PPPDB	"/var/run/pppd2.tdb"
#endif
#endif /* __STDC__ */

#ifdef PLUGIN
#ifdef __STDC__
#define _PATH_PLUGIN	"/usr/lib/pppd/"
#else /* __STDC__ */
#define _PATH_PLUGIN	"/usr/lib/pppd"
#endif /* __STDC__ */

#endif /* PLUGIN */
