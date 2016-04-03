/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBIPSET_SESSION_H
#define LIBIPSET_SESSION_H

#include <stdbool.h>				/* bool */
#include <stdint.h>				/* uintxx_t */
#include <stdio.h>				/* printf */

#include <libipset/linux_ip_set.h>		/* enum ipset_cmd */

/* Report and output buffer sizes */
#define IPSET_ERRORBUFLEN		1024
#define IPSET_OUTBUFLEN			8192

struct ipset_session;
struct ipset_data;
struct ipset_handle;

#ifdef __cplusplus
extern "C" {
#endif

extern struct ipset_data *
	ipset_session_data(const struct ipset_session *session);
extern struct ipset_handle *
	ipset_session_handle(const struct ipset_session *session);
extern const struct ipset_type *
	ipset_saved_type(const struct ipset_session *session);
extern void ipset_session_lineno(struct ipset_session *session,
				 uint32_t lineno);

enum ipset_err_type {
	IPSET_ERROR,
	IPSET_WARNING,
};

extern int ipset_session_report(struct ipset_session *session,
				enum ipset_err_type type,
				const char *fmt, ...);

#define ipset_err(session, fmt, args...) \
	ipset_session_report(session, IPSET_ERROR, fmt , ## args)

#define ipset_warn(session, fmt, args...) \
	ipset_session_report(session, IPSET_WARNING, fmt , ## args)

#define ipset_errptr(session, fmt, args...) ({				\
	ipset_session_report(session, IPSET_ERROR, fmt , ## args);	\
	NULL;								\
})

extern void ipset_session_report_reset(struct ipset_session *session);
extern const char *ipset_session_error(const struct ipset_session *session);
extern const char *ipset_session_warning(const struct ipset_session *session);

#define ipset_session_data_set(session, opt, value)	\
	ipset_data_set(ipset_session_data(session), opt, value)
#define ipset_session_data_get(session, opt)		\
	ipset_data_get(ipset_session_data(session), opt)

/* Environment option flags */
enum ipset_envopt {
	IPSET_ENV_BIT_SORTED	= 0,
	IPSET_ENV_SORTED	= (1 << IPSET_ENV_BIT_SORTED),
	IPSET_ENV_BIT_QUIET	= 1,
	IPSET_ENV_QUIET		= (1 << IPSET_ENV_BIT_QUIET),
	IPSET_ENV_BIT_RESOLVE	= 2,
	IPSET_ENV_RESOLVE	= (1 << IPSET_ENV_BIT_RESOLVE),
	IPSET_ENV_BIT_EXIST	= 3,
	IPSET_ENV_EXIST		= (1 << IPSET_ENV_BIT_EXIST),
	IPSET_ENV_BIT_LIST_SETNAME = 4,
	IPSET_ENV_LIST_SETNAME	= (1 << IPSET_ENV_BIT_LIST_SETNAME),
	IPSET_ENV_BIT_LIST_HEADER = 5,
	IPSET_ENV_LIST_HEADER	= (1 << IPSET_ENV_BIT_LIST_HEADER),
};

extern int ipset_envopt_parse(struct ipset_session *session,
			      int env, const char *str);
extern bool ipset_envopt_test(struct ipset_session *session,
			      enum ipset_envopt env);

enum ipset_output_mode {
	IPSET_LIST_NONE,
	IPSET_LIST_PLAIN,
	IPSET_LIST_SAVE,
	IPSET_LIST_XML,
};

extern int ipset_session_output(struct ipset_session *session,
				enum ipset_output_mode mode);

extern int ipset_commit(struct ipset_session *session);
extern int ipset_cmd(struct ipset_session *session, enum ipset_cmd cmd,
		     uint32_t lineno);

typedef int (*ipset_outfn)(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

extern int ipset_session_outfn(struct ipset_session *session,
			       ipset_outfn outfn);
extern struct ipset_session *ipset_session_init(ipset_outfn outfn);
extern int ipset_session_fini(struct ipset_session *session);

extern void ipset_debug_msg(const char *dir, void *buffer, int len);

#ifdef __cplusplus
}
#endif

#endif /* LIBIPSET_SESSION_H */
