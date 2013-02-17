#ifndef KRB5_UTIL_H
#define KRB5_UTIL_H

#include <krb5.h>

#ifdef HAVE_LIBTIRPC
#include <rpc/auth_gss.h>
#else
#include "gss_oids.h"
#endif

/*
 * List of principals from our keytab that we
 * will try to use to obtain credentials
 * (known as a principal list entry (ple))
 */
struct gssd_k5_kt_princ {
	struct gssd_k5_kt_princ *next;
	krb5_principal princ;
	char *ccname;
	char *realm;
	krb5_timestamp endtime;
};


int gssd_setup_krb5_user_gss_ccache(uid_t uid, char *servername,
				     char *dirname);
int  gssd_get_krb5_machine_cred_list(char ***list);
void gssd_free_krb5_machine_cred_list(char **list);
void gssd_setup_krb5_machine_gss_ccache(char *servername);
void gssd_destroy_krb5_machine_creds(void);
int  gssd_refresh_krb5_machine_credential(char *hostname,
					  struct gssd_k5_kt_princ *ple);
char *gssd_k5_err_msg(krb5_context context, krb5_error_code code);
void gssd_k5_get_default_realm(char **def_realm);

#ifdef HAVE_SET_ALLOWABLE_ENCTYPES
int limit_krb5_enctypes(struct rpc_gss_sec *sec, uid_t uid);
#endif

/*
 * Hide away some of the MIT vs. Heimdal differences
 * here with macros...
 */

#ifdef HAVE_KRB5
#define k5_free_unparsed_name(ctx, name)	krb5_free_unparsed_name((ctx), (name))
#define k5_free_default_realm(ctx, realm)	krb5_free_default_realm((ctx), (realm))
#define k5_free_kt_entry(ctx, kte)		krb5_free_keytab_entry_contents((ctx),(kte))
#else	/* Heimdal */
#define k5_free_unparsed_name(ctx, name)	free(name)
#define k5_free_default_realm(ctx, realm)	free(realm)
#define k5_free_kt_entry(ctx, kte)		krb5_kt_free_entry((ctx),(kte))
#undef USE_GSS_KRB5_CCACHE_NAME
#define USE_GSS_KRB5_CCACHE_NAME 1
#endif

#endif /* KRB5_UTIL_H */
