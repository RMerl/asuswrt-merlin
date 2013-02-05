/*
 *  Adapted in part from MIT Kerberos 5-1.2.1 slave/kprop.c and from
 *  http://docs.sun.com/?p=/doc/816-1331/6m7oo9sms&a=view
 *
 *  Copyright (c) 2002-2004 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  Andy Adamson <andros@umich.edu>
 *  J. Bruce Fields <bfields@umich.edu>
 *  Marius Aamodt Eriksen <marius@umich.edu>
 *  Kevin Coffman <kwc@umich.edu>
 */

/*
 * slave/kprop.c
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * Copyright 1994 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
  krb5_util.c

  Copyright (c) 2004 The Regents of the University of Michigan.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the University nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/param.h>
#include <rpc/rpc.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <gssapi/gssapi.h>
#ifdef USE_PRIVATE_KRB5_FUNCTIONS
#include <gssapi/gssapi_krb5.h>
#endif
#include <krb5.h>
#include <rpc/auth_gss.h>

#include "gssd.h"
#include "err_util.h"
#include "gss_util.h"
#include "krb5_util.h"

/* Global list of principals/cache file names for machine credentials */
struct gssd_k5_kt_princ *gssd_k5_kt_princ_list = NULL;

/*==========================*/
/*===  Internal routines ===*/
/*==========================*/

static int select_krb5_ccache(const struct dirent *d);
static int gssd_find_existing_krb5_ccache(uid_t uid, char *dirname,
		struct dirent **d);
static int gssd_get_single_krb5_cred(krb5_context context,
		krb5_keytab kt, struct gssd_k5_kt_princ *ple);
static int query_krb5_ccache(const char* cred_cache, char **ret_princname,
		char **ret_realm);

/*
 * Called from the scandir function to weed out potential krb5
 * credentials cache files
 *
 * Returns:
 *	0 => don't select this one
 *	1 => select this one
 */
static int
select_krb5_ccache(const struct dirent *d)
{
	/*
	 * Note: We used to check d->d_type for DT_REG here,
	 * but apparenlty reiser4 always has DT_UNKNOWN.
	 * Check for IS_REG after stat() call instead.
	 */
	if (strstr(d->d_name, GSSD_DEFAULT_CRED_PREFIX))
		return 1;
	else
		return 0;
}

/*
 * Look in directory "dirname" for files that look like they
 * are Kerberos Credential Cache files for a given UID.  Return
 * non-zero and the dirent pointer for the entry most likely to be
 * what we want. Otherwise, return zero and no dirent pointer.
 * The caller is responsible for freeing the dirent if one is returned.
 *
 * Returns:
 *	0 => could not find an existing entry
 *	1 => found an existing entry
 */
static int
gssd_find_existing_krb5_ccache(uid_t uid, char *dirname, struct dirent **d)
{
	struct dirent **namelist;
	int n;
	int i;
	int found = 0;
	struct dirent *best_match_dir = NULL;
	struct stat best_match_stat, tmp_stat;
	char buf[1030];
	char *princname = NULL;
	char *realm = NULL;
	int score, best_match_score = 0;

	memset(&best_match_stat, 0, sizeof(best_match_stat));
	*d = NULL;
	n = scandir(dirname, &namelist, select_krb5_ccache, 0);
	if (n < 0) {
		printerr(1, "Error doing scandir on directory '%s': %s\n",
			dirname, strerror(errno));
	}
	else if (n > 0) {
		char statname[1024];
		for (i = 0; i < n; i++) {
			snprintf(statname, sizeof(statname),
				 "%s/%s", dirname, namelist[i]->d_name);
			printerr(3, "CC file '%s' being considered, "
				 "with preferred realm '%s'\n",
				 statname, preferred_realm ?
					preferred_realm : "<none selected>");
			snprintf(buf, sizeof(buf), "FILE:%s/%s", dirname, 
					namelist[i]->d_name);
			if (lstat(statname, &tmp_stat)) {
				printerr(0, "Error doing stat on file '%s'\n",
					 statname);
				free(namelist[i]);
				continue;
			}
			/* Only pick caches owned by the user (uid) */
			if (tmp_stat.st_uid != uid) {
				printerr(3, "CC file '%s' owned by %u, not %u\n",
					 statname, tmp_stat.st_uid, uid);
				free(namelist[i]);
				continue;
			}
			if (!S_ISREG(tmp_stat.st_mode)) {
				printerr(3, "CC file '%s' is not a regular file\n",
					 statname);
				free(namelist[i]);
				continue;
			}
			if (!query_krb5_ccache(buf, &princname, &realm)) {
				printerr(3, "CC file '%s' is expired or corrupt\n",
					 statname);
				free(namelist[i]);
				continue;
			}

			score = 0;
			if (preferred_realm &&
					strcmp(realm, preferred_realm) == 0) 
				score++;

			printerr(3, "CC file '%s'(%s@%s) passed all checks and"
				    " has mtime of %u\n",
				 statname, princname, realm, 
				 tmp_stat.st_mtime);
			/*
			 * if more than one match is found, return the most
			 * recent (the one with the latest mtime), and
			 * don't free the dirent
			 */
			if (!found) {
				best_match_dir = namelist[i];
				best_match_stat = tmp_stat;
				best_match_score = score;
				found++;
			}
			else {
				/*
				 * If current score is higher than best match 
				 * score, we use the current match. Otherwise,
				 * if the current match has an mtime later
				 * than the one we are looking at, then use
				 * the current match.  Otherwise, we still
				 * have the best match.
				 */
				if (best_match_score < score ||
				    (best_match_score == score && 
				       tmp_stat.st_mtime >
					    best_match_stat.st_mtime)) {
					free(best_match_dir);
					best_match_dir = namelist[i];
					best_match_stat = tmp_stat;
					best_match_score = score;
				}
				else {
					free(namelist[i]);
				}
				printerr(3, "CC file '%s/%s' is our "
					    "current best match "
					    "with mtime of %u\n",
					 dirname, best_match_dir->d_name,
					 best_match_stat.st_mtime);
			}
			free(princname);
			free(realm);
		}
		free(namelist);
	}
	if (found)
	{
		*d = best_match_dir;
	}
	return found;
}


#ifdef HAVE_SET_ALLOWABLE_ENCTYPES
/*
 * this routine obtains a credentials handle via gss_acquire_cred()
 * then calls gss_krb5_set_allowable_enctypes() to limit the encryption
 * types negotiated.
 *
 * XXX Should call some function to determine the enctypes supported
 * by the kernel. (Only need to do that once!)
 *
 * Returns:
 *	0 => all went well
 *     -1 => there was an error
 */

int
limit_krb5_enctypes(struct rpc_gss_sec *sec, uid_t uid)
{
	u_int maj_stat, min_stat;
	gss_cred_id_t credh;
	gss_OID_set_desc  desired_mechs;
	krb5_enctype enctypes[] = { ENCTYPE_DES_CBC_CRC,
				    ENCTYPE_DES_CBC_MD5,
				    ENCTYPE_DES_CBC_MD4 };
	int num_enctypes = sizeof(enctypes) / sizeof(enctypes[0]);

	/* We only care about getting a krb5 cred */
	desired_mechs.count = 1;
	desired_mechs.elements = &krb5oid;

	maj_stat = gss_acquire_cred(&min_stat, NULL, 0,
				    &desired_mechs, GSS_C_INITIATE,
				    &credh, NULL, NULL);

	if (maj_stat != GSS_S_COMPLETE) {
		if (get_verbosity() > 0)
			pgsserr("gss_acquire_cred",
				maj_stat, min_stat, &krb5oid);
		return -1;
	}

	maj_stat = gss_set_allowable_enctypes(&min_stat, credh, &krb5oid,
					     num_enctypes, &enctypes);
	if (maj_stat != GSS_S_COMPLETE) {
		pgsserr("gss_set_allowable_enctypes",
			maj_stat, min_stat, &krb5oid);
		gss_release_cred(&min_stat, &credh);
		return -1;
	}
	sec->cred = credh;

	return 0;
}
#endif	/* HAVE_SET_ALLOWABLE_ENCTYPES */

/*
 * Obtain credentials via a key in the keytab given
 * a keytab handle and a gssd_k5_kt_princ structure.
 * Checks to see if current credentials are expired,
 * if not, uses the keytab to obtain new credentials.
 *
 * Returns:
 *	0 => success (or credentials have not expired)
 *	nonzero => error
 */
static int
gssd_get_single_krb5_cred(krb5_context context,
			  krb5_keytab kt,
			  struct gssd_k5_kt_princ *ple)
{
#if HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ADDRESSLESS
	krb5_get_init_creds_opt *init_opts = NULL;
#else
	krb5_get_init_creds_opt options;
#endif
	krb5_get_init_creds_opt *opts;
	krb5_creds my_creds;
	krb5_ccache ccache = NULL;
	char kt_name[BUFSIZ];
	char cc_name[BUFSIZ];
	int code;
	time_t now = time(0);
	char *cache_type;
	char *pname = NULL;
	char *k5err = NULL;

	memset(&my_creds, 0, sizeof(my_creds));

	if (ple->ccname && ple->endtime > now) {
		printerr(2, "INFO: Credentials in CC '%s' are good until %d\n",
			 ple->ccname, ple->endtime);
		code = 0;
		goto out;
	}

	if ((code = krb5_kt_get_name(context, kt, kt_name, BUFSIZ))) {
		printerr(0, "ERROR: Unable to get keytab name in "
			    "gssd_get_single_krb5_cred\n");
		goto out;
	}

	if ((krb5_unparse_name(context, ple->princ, &pname)))
		pname = NULL;

#if HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ADDRESSLESS
	code = krb5_get_init_creds_opt_alloc(context, &init_opts);
	if (code) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s allocating gic options\n", k5err);
		goto out;
	}
	if (krb5_get_init_creds_opt_set_addressless(context, init_opts, 1))
		printerr(1, "WARNING: Unable to set option for addressless "
			 "tickets.  May have problems behind a NAT.\n");
#ifdef TEST_SHORT_LIFETIME
	/* set a short lifetime (for debugging only!) */
	printerr(0, "WARNING: Using (debug) short machine cred lifetime!\n");
	krb5_get_init_creds_opt_set_tkt_life(init_opts, 5*60);
#endif
	opts = init_opts;

#else	/* HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ADDRESSLESS */

	krb5_get_init_creds_opt_init(&options);
	krb5_get_init_creds_opt_set_address_list(&options, NULL);
#ifdef TEST_SHORT_LIFETIME
	/* set a short lifetime (for debugging only!) */
	printerr(0, "WARNING: Using (debug) short machine cred lifetime!\n");
	krb5_get_init_creds_opt_set_tkt_life(&options, 5*60);
#endif
	opts = &options;
#endif

	if ((code = krb5_get_init_creds_keytab(context, &my_creds, ple->princ,
					       kt, 0, NULL, opts))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(1, "WARNING: %s while getting initial ticket for "
			 "principal '%s' using keytab '%s'\n", k5err,
			 pname ? pname : "<unparsable>", kt_name);
		goto out;
	}

	/*
	 * Initialize cache file which we're going to be using
	 */

	if (use_memcache)
	    cache_type = "MEMORY";
	else
	    cache_type = "FILE";
	snprintf(cc_name, sizeof(cc_name), "%s:%s/%s%s_%s",
		cache_type,
		ccachesearch[0], GSSD_DEFAULT_CRED_PREFIX,
		GSSD_DEFAULT_MACHINE_CRED_SUFFIX, ple->realm);
	ple->endtime = my_creds.times.endtime;
	if (ple->ccname != NULL)
		free(ple->ccname);
	ple->ccname = strdup(cc_name);
	if (ple->ccname == NULL) {
		printerr(0, "ERROR: no storage to duplicate credentials "
			    "cache name '%s'\n", cc_name);
		code = ENOMEM;
		goto out;
	}
	if ((code = krb5_cc_resolve(context, cc_name, &ccache))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s while opening credential cache '%s'\n",
			 k5err, cc_name);
		goto out;
	}
	if ((code = krb5_cc_initialize(context, ccache, ple->princ))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s while initializing credential "
			 "cache '%s'\n", k5err, cc_name);
		goto out;
	}
	if ((code = krb5_cc_store_cred(context, ccache, &my_creds))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s while storing credentials in '%s'\n",
			 k5err, cc_name);
		goto out;
	}

	code = 0;
	printerr(2, "Successfully obtained machine credentials for "
		 "principal '%s' stored in ccache '%s'\n", pname, cc_name);
  out:
#if HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ADDRESSLESS
	if (init_opts)
		krb5_get_init_creds_opt_free(context, init_opts);
#endif
	if (pname)
		k5_free_unparsed_name(context, pname);
	if (ccache)
		krb5_cc_close(context, ccache);
	krb5_free_cred_contents(context, &my_creds);
	free(k5err);
	return (code);
}

/*
 * Depending on the version of Kerberos, we either need to use
 * a private function, or simply set the environment variable.
 */
static void
gssd_set_krb5_ccache_name(char *ccname)
{
#ifdef USE_GSS_KRB5_CCACHE_NAME
	u_int	maj_stat, min_stat;

	printerr(2, "using gss_krb5_ccache_name to select krb5 ccache %s\n",
		 ccname);
	maj_stat = gss_krb5_ccache_name(&min_stat, ccname, NULL);
	if (maj_stat != GSS_S_COMPLETE) {
		printerr(0, "WARNING: gss_krb5_ccache_name with "
			"name '%s' failed (%s)\n",
			ccname, error_message(min_stat));
	}
#else
	/*
	 * Set the KRB5CCNAME environment variable to tell the krb5 code
	 * which credentials cache to use.  (Instead of using the private
	 * function above for which there is no generic gssapi
	 * equivalent.)
	 */
	printerr(2, "using environment variable to select krb5 ccache %s\n",
		 ccname);
	setenv("KRB5CCNAME", ccname, 1);
#endif
}

/*
 * Given a principal, find a matching ple structure
 */
static struct gssd_k5_kt_princ *
find_ple_by_princ(krb5_context context, krb5_principal princ)
{
	struct gssd_k5_kt_princ *ple;

	for (ple = gssd_k5_kt_princ_list; ple != NULL; ple = ple->next) {
		if (krb5_principal_compare(context, ple->princ, princ))
			return ple;
	}
	/* no match found */
	return NULL;
}

/*
 * Create, initialize, and add a new ple structure to the global list
 */
static struct gssd_k5_kt_princ *
new_ple(krb5_context context, krb5_principal princ)
{
	struct gssd_k5_kt_princ *ple = NULL, *p;
	krb5_error_code code;
	char *default_realm;
	int is_default_realm = 0;

	ple = malloc(sizeof(struct gssd_k5_kt_princ));
	if (ple == NULL)
		goto outerr;
	memset(ple, 0, sizeof(*ple));

#ifdef HAVE_KRB5
	ple->realm = strndup(princ->realm.data,
			     princ->realm.length);
#else
	ple->realm = strdup(princ->realm);
#endif
	if (ple->realm == NULL)
		goto outerr;
	code = krb5_copy_principal(context, princ, &ple->princ);
	if (code)
		goto outerr;

	/*
	 * Add new entry onto the list (if this is the default
	 * realm, always add to the front of the list)
	 */

	code = krb5_get_default_realm(context, &default_realm);
	if (code == 0) {
		if (strcmp(ple->realm, default_realm) == 0)
			is_default_realm = 1;
		k5_free_default_realm(context, default_realm);
	}

	if (is_default_realm) {
		ple->next = gssd_k5_kt_princ_list;
		gssd_k5_kt_princ_list = ple;
	} else {
		p = gssd_k5_kt_princ_list;
		while (p != NULL && p->next != NULL)
			p = p->next;
		if (p == NULL)
			gssd_k5_kt_princ_list = ple;
		else
			p->next = ple;
	}

	return ple;
outerr:
	if (ple) {
		if (ple->realm)
			free(ple->realm);
		free(ple);
	}
	return NULL;
}

/*
 * Given a principal, find an existing ple structure, or create one
 */
static struct gssd_k5_kt_princ *
get_ple_by_princ(krb5_context context, krb5_principal princ)
{
	struct gssd_k5_kt_princ *ple;

	/* Need to serialize list if we ever become multi-threaded! */

	ple = find_ple_by_princ(context, princ);
	if (ple == NULL) {
		ple = new_ple(context, princ);
	}

	return ple;
}

/*
 * Given a (possibly unqualified) hostname,
 * return the fully qualified (lower-case!) hostname
 */
static int
get_full_hostname(const char *inhost, char *outhost, int outhostlen)
{
	struct addrinfo *addrs = NULL;
	struct addrinfo hints;
	int retval;
	char *c;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_CANONNAME;

	/* Get full target hostname */
	retval = getaddrinfo(inhost, NULL, &hints, &addrs);
	if (retval) {
		printerr(1, "%s while getting full hostname for '%s'\n",
			 gai_strerror(retval), inhost);
		goto out;
	}
	strncpy(outhost, addrs->ai_canonname, outhostlen);
	freeaddrinfo(addrs);
	for (c = outhost; *c != '\0'; c++)
	    *c = tolower(*c);

	printerr(3, "Full hostname for '%s' is '%s'\n", inhost, outhost);
	retval = 0;
out:
	return retval;
}

/* 
 * If principal matches the given realm and service name,
 * and has *any* instance (hostname), return 1.
 * Otherwise return 0, indicating no match.
 */
static int
realm_and_service_match(krb5_context context, krb5_principal p,
			const char *realm, const char *service)
{
#ifdef HAVE_KRB5
	/* Must have two components */
	if (p->length != 2)
		return 0;
	if ((strlen(realm) == p->realm.length)
	    && (strncmp(realm, p->realm.data, p->realm.length) == 0)
	    && (strlen(service) == p->data[0].length)
	    && (strncmp(service, p->data[0].data, p->data[0].length) == 0))
		return 1;
#else
	const char *name, *inst;

	if (p->name.name_string.len != 2)
		return 0;
	name = krb5_principal_get_comp_string(context, p, 0);
	inst = krb5_principal_get_comp_string(context, p, 1);
	if (name == NULL || inst == NULL)
		return 0;
	if ((strcmp(realm, p->realm) == 0)
	    && (strcmp(service, name) == 0))
		return 1;
#endif
	return 0;
}

/*
 * Search the given keytab file looking for an entry with the given
 * service name and realm, ignoring hostname (instance).
 *
 * Returns:
 *	0 => No error
 *	non-zero => An error occurred
 *
 * If a keytab entry is found, "found" is set to one, and the keytab
 * entry is returned in "kte".  Otherwise, "found" is zero, and the
 * value of "kte" is unpredictable.
 */
static int
gssd_search_krb5_keytab(krb5_context context, krb5_keytab kt,
			const char *realm, const char *service,
			int *found, krb5_keytab_entry *kte)
{
	krb5_kt_cursor cursor;
	krb5_error_code code;
	struct gssd_k5_kt_princ *ple;
	int retval = -1;
	char kt_name[BUFSIZ];
	char *pname;
	char *k5err = NULL;

	if (found == NULL) {
		retval = EINVAL;
		goto out;
	}
	*found = 0;

	/*
	 * Look through each entry in the keytab file and determine
	 * if we might want to use it as machine credentials.  If so,
	 * save info in the global principal list (gssd_k5_kt_princ_list).
	 */
	if ((code = krb5_kt_get_name(context, kt, kt_name, BUFSIZ))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s attempting to get keytab name\n", k5err);
		retval = code;
		goto out;
	}
	if ((code = krb5_kt_start_seq_get(context, kt, &cursor))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s while beginning keytab scan "
			    "for keytab '%s'\n", k5err, kt_name);
		retval = code;
		goto out;
	}

	while ((code = krb5_kt_next_entry(context, kt, kte, &cursor)) == 0) {
		if ((code = krb5_unparse_name(context, kte->principal,
					      &pname))) {
			k5err = gssd_k5_err_msg(context, code);
			printerr(0, "WARNING: Skipping keytab entry because "
				 "we failed to unparse principal name: %s\n",
				 k5err);
			k5_free_kt_entry(context, kte);
			continue;
		}
		printerr(4, "Processing keytab entry for principal '%s'\n",
			 pname);
		/* Use the first matching keytab entry found */
	        if ((realm_and_service_match(context, kte->principal, realm,
					     service))) {
			printerr(4, "We WILL use this entry (%s)\n", pname);
			ple = get_ple_by_princ(context, kte->principal);
			/*
			 * Return, don't free, keytab entry if
			 * we were successful!
			 */
			if (ple == NULL) {
				retval = ENOMEM;
				k5_free_kt_entry(context, kte);
			} else {
				retval = 0;
				*found = 1;
			}
			k5_free_unparsed_name(context, pname);
			break;
		}
		else {
			printerr(4, "We will NOT use this entry (%s)\n",
				pname);
		}
		k5_free_unparsed_name(context, pname);
		k5_free_kt_entry(context, kte);
	}

	if ((code = krb5_kt_end_seq_get(context, kt, &cursor))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "WARNING: %s while ending keytab scan for "
			    "keytab '%s'\n", k5err, kt_name);
	}

	retval = 0;
  out:
	free(k5err);
	return retval;
}

/*
 * Find a keytab entry to use for a given target hostname.
 * Tries to find the most appropriate keytab to use given the
 * name of the host we are trying to connect with.
 */
static int
find_keytab_entry(krb5_context context, krb5_keytab kt, const char *hostname,
		  krb5_keytab_entry *kte)
{
	krb5_error_code code;
	const char *svcnames[] = { "root", "nfs", "host", NULL };
	char **realmnames = NULL;
	char myhostname[NI_MAXHOST], targethostname[NI_MAXHOST];
	int i, j, retval;
	char *default_realm = NULL;
	char *realm;
	char *k5err = NULL;
	int tried_all = 0, tried_default = 0;
	krb5_principal princ;


	/* Get full target hostname */
	retval = get_full_hostname(hostname, targethostname,
				   sizeof(targethostname));
	if (retval)
		goto out;

	/* Get full local hostname */
	retval = gethostname(myhostname, sizeof(myhostname));
	if (retval) {
		k5err = gssd_k5_err_msg(context, retval);
		printerr(1, "%s while getting local hostname\n", k5err);
		goto out;
	}
	retval = get_full_hostname(myhostname, myhostname, sizeof(myhostname));
	if (retval)
		goto out;

	code = krb5_get_default_realm(context, &default_realm);
	if (code) {
		retval = code;
		k5err = gssd_k5_err_msg(context, code);
		printerr(1, "%s while getting default realm name\n", k5err);
		goto out;
	}

	/*
	 * Get the realm name(s) for the target hostname.
	 * In reality, this function currently only returns a
	 * single realm, but we code with the assumption that
	 * someday it may actually return a list.
	 */
	code = krb5_get_host_realm(context, targethostname, &realmnames);
	if (code) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s while getting realm(s) for host '%s'\n",
			 k5err, targethostname);
		retval = code;
		goto out;
	}

	/*
	 * Try the "appropriate" realm first, and if nothing found for that
	 * realm, try the default realm (if it hasn't already been tried).
	 */
	i = 0;
	realm = realmnames[i];
	while (1) {
		if (realm == NULL) {
			tried_all = 1;
			if (!tried_default)
				realm = default_realm;
		}
		if (tried_all && tried_default)
			break;
		if (strcmp(realm, default_realm) == 0)
			tried_default = 1;
		for (j = 0; svcnames[j] != NULL; j++) {
			code = krb5_build_principal_ext(context, &princ,
							strlen(realm),
							realm,
							strlen(svcnames[j]),
							svcnames[j],
							strlen(myhostname),
							myhostname,
							NULL);
			if (code) {
				k5err = gssd_k5_err_msg(context, code);
				printerr(1, "%s while building principal for "
					 "'%s/%s@%s'\n", k5err, svcnames[j],
					 myhostname, realm);
				continue;
			}
			code = krb5_kt_get_entry(context, kt, princ, 0, 0, kte);
			krb5_free_principal(context, princ);
			if (code) {
				k5err = gssd_k5_err_msg(context, code);
				printerr(3, "%s while getting keytab entry for "
					 "'%s/%s@%s'\n", k5err, svcnames[j],
					 myhostname, realm);
			} else {
				printerr(3, "Success getting keytab entry for "
					 "'%s/%s@%s'\n",
					 svcnames[j], myhostname, realm);
				retval = 0;
				goto out;
			}
			retval = code;
		}
		/*
		 * Nothing found with our hostname instance, now look for
		 * names with any instance (they must have an instance)
		 */
		for (j = 0; svcnames[j] != NULL; j++) {
			int found = 0;
			code = gssd_search_krb5_keytab(context, kt, realm,
						       svcnames[j], &found, kte);
			if (!code && found) {
				printerr(3, "Success getting keytab entry for "
					 "%s/*@%s\n", svcnames[j], realm);
				retval = 0;
				goto out;
			}
		}
		if (!tried_all) {
			i++;
			realm = realmnames[i];
		}
	}
out:
	if (default_realm)
		k5_free_default_realm(context, default_realm);
	if (realmnames)
		krb5_free_host_realm(context, realmnames);
	free(k5err);
	return retval;
}


static inline int data_is_equal(krb5_data d1, krb5_data d2)
{
	return (d1.length == d2.length
		&& memcmp(d1.data, d2.data, d1.length) == 0);
}

static int
check_for_tgt(krb5_context context, krb5_ccache ccache,
	      krb5_principal principal)
{
	krb5_error_code ret;
	krb5_creds creds;
	krb5_cc_cursor cur;
	int found = 0;

	ret = krb5_cc_start_seq_get(context, ccache, &cur);
	if (ret) 
		return 0;

	while (!found &&
		(ret = krb5_cc_next_cred(context, ccache, &cur, &creds)) == 0) {
		if (creds.server->length == 2 &&
				data_is_equal(creds.server->realm,
					      principal->realm) &&
				creds.server->data[0].length == 6 &&
				memcmp(creds.server->data[0].data,
						"krbtgt", 6) == 0 &&
				data_is_equal(creds.server->data[1],
					      principal->realm) &&
				creds.times.endtime > time(NULL))
			found = 1;
		krb5_free_cred_contents(context, &creds);
	}
	krb5_cc_end_seq_get(context, ccache, &cur);

	return found;
}

static int
query_krb5_ccache(const char* cred_cache, char **ret_princname,
		  char **ret_realm)
{
	krb5_error_code ret;
	krb5_context context;
	krb5_ccache ccache;
	krb5_principal principal;
	int found = 0;
	char *str = NULL;
	char *princstring;

	ret = krb5_init_context(&context);
	if (ret) 
		return 0;

	if(!cred_cache || krb5_cc_resolve(context, cred_cache, &ccache))
		goto err_cache;

	if (krb5_cc_set_flags(context, ccache, 0))
		goto err_princ;

	ret = krb5_cc_get_principal(context, ccache, &principal);
	if (ret) 
		goto err_princ;

	found = check_for_tgt(context, ccache, principal);
	if (found) {
		ret = krb5_unparse_name(context, principal, &princstring);
		if (ret == 0) {
		    if ((str = strchr(princstring, '@')) != NULL) {
			    *str = '\0';
			    *ret_princname = strdup(princstring);
			    *ret_realm = strdup(str+1);
		    }
		    k5_free_unparsed_name(context, princstring);
		} else {
			found = 0;
		}
	}
	krb5_free_principal(context, principal);
err_princ:
	krb5_cc_set_flags(context, ccache,  KRB5_TC_OPENCLOSE);
	krb5_cc_close(context, ccache);
err_cache:
	krb5_free_context(context);
	return found;
}

/*==========================*/
/*===  External routines ===*/
/*==========================*/

/*
 * Attempt to find the best match for a credentials cache file
 * given only a UID.  We really need more information, but we
 * do the best we can.
 *
 * Returns:
 *	0 => a ccache was found
 *	1 => no ccache was found
 */
int
gssd_setup_krb5_user_gss_ccache(uid_t uid, char *servername, char *dirname)
{
	char			buf[MAX_NETOBJ_SZ];
	struct dirent		*d;

	printerr(2, "getting credentials for client with uid %u for "
		    "server %s\n", uid, servername);
	memset(buf, 0, sizeof(buf));
	if (gssd_find_existing_krb5_ccache(uid, dirname, &d)) {
		snprintf(buf, sizeof(buf), "FILE:%s/%s", dirname, d->d_name);
		free(d);
	}
	else
		return 1;
	printerr(2, "using %s as credentials cache for client with "
		    "uid %u for server %s\n", buf, uid, servername);
	gssd_set_krb5_ccache_name(buf);
	return 0;
}

/*
 * Let the gss code know where to find the machine credentials ccache.
 *
 * Returns:
 *	void
 */
void
gssd_setup_krb5_machine_gss_ccache(char *ccname)
{
	printerr(2, "using %s as credentials cache for machine creds\n",
		 ccname);
	gssd_set_krb5_ccache_name(ccname);
}

/*
 * Return an array of pointers to names of credential cache files
 * which can be used to try to create gss contexts with a server.
 *
 * Returns:
 *	0 => list is attached
 *	nonzero => error
 */
int
gssd_get_krb5_machine_cred_list(char ***list)
{
	char **l;
	int listinc = 10;
	int listsize = listinc;
	int i = 0;
	int retval;
	struct gssd_k5_kt_princ *ple;

	/* Assume failure */
	retval = -1;
	*list = (char **) NULL;

	if ((l = (char **) malloc(listsize * sizeof(char *))) == NULL) {
		retval = ENOMEM;
		goto out;
	}

	/* Need to serialize list if we ever become multi-threaded! */

	for (ple = gssd_k5_kt_princ_list; ple; ple = ple->next) {
		if (ple->ccname) {
			/* Make sure cred is up-to-date before returning it */
			retval = gssd_refresh_krb5_machine_credential(NULL, ple);
			if (retval)
				continue;
			if (i + 1 > listsize) {
				listsize += listinc;
				l = (char **)
					realloc(l, listsize * sizeof(char *));
				if (l == NULL) {
					retval = ENOMEM;
					goto out;
				}
			}
			if ((l[i++] = strdup(ple->ccname)) == NULL) {
				retval = ENOMEM;
				goto out;
			}
		}
	}
	if (i > 0) {
		l[i] = NULL;
		*list = l;
		retval = 0;
		goto out;
	}
  out:
	return retval;
}

/*
 * Frees the list of names returned in get_krb5_machine_cred_list()
 */
void
gssd_free_krb5_machine_cred_list(char **list)
{
	char **n;

	if (list == NULL)
		return;
	for (n = list; n && *n; n++) {
		free(*n);
	}
	free(list);
}

/*
 * Called upon exit.  Destroys machine credentials.
 */
void
gssd_destroy_krb5_machine_creds(void)
{
	krb5_context context;
	krb5_error_code code = 0;
	krb5_ccache ccache;
	struct gssd_k5_kt_princ *ple;
	char *k5err = NULL;

	code = krb5_init_context(&context);
	if (code) {
		k5err = gssd_k5_err_msg(NULL, code);
		printerr(0, "ERROR: %s while initializing krb5\n", k5err);
		goto out;
	}

	for (ple = gssd_k5_kt_princ_list; ple; ple = ple->next) {
		if (!ple->ccname)
			continue;
		if ((code = krb5_cc_resolve(context, ple->ccname, &ccache))) {
			k5err = gssd_k5_err_msg(context, code);
			printerr(0, "WARNING: %s while resolving credential "
				    "cache '%s' for destruction\n", k5err,
				    ple->ccname);
			continue;
		}

		if ((code = krb5_cc_destroy(context, ccache))) {
			k5err = gssd_k5_err_msg(context, code);
			printerr(0, "WARNING: %s while destroying credential "
				    "cache '%s'\n", k5err, ple->ccname);
		}
	}
  out:
	free(k5err);
	krb5_free_context(context);
}

/*
 * Obtain (or refresh if necessary) Kerberos machine credentials
 */
int
gssd_refresh_krb5_machine_credential(char *hostname,
				     struct gssd_k5_kt_princ *ple)
{
	krb5_error_code code = 0;
	krb5_context context;
	krb5_keytab kt = NULL;;
	int retval = 0;
	char *k5err = NULL;

	if (hostname == NULL && ple == NULL)
		return EINVAL;

	code = krb5_init_context(&context);
	if (code) {
		k5err = gssd_k5_err_msg(NULL, code);
		printerr(0, "ERROR: %s: %s while initializing krb5 context\n",
			 __func__, k5err);
		retval = code;
		goto out;
	}

	if ((code = krb5_kt_resolve(context, keytabfile, &kt))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s: %s while resolving keytab '%s'\n",
			 __func__, k5err, keytabfile);
		goto out;
	}

	if (ple == NULL) {
		krb5_keytab_entry kte;

		code = find_keytab_entry(context, kt, hostname, &kte);
		if (code) {
			printerr(0, "ERROR: %s: no usable keytab entry found "
				 "in keytab %s for connection with host %s\n",
				 __FUNCTION__, keytabfile, hostname);
			retval = code;
			goto out;
		}

		ple = get_ple_by_princ(context, kte.principal);
		k5_free_kt_entry(context, &kte);
		if (ple == NULL) {
			char *pname;
			if ((krb5_unparse_name(context, kte.principal, &pname))) {
				pname = NULL;
			}
			printerr(0, "ERROR: %s: Could not locate or create "
				 "ple struct for principal %s for connection "
				 "with host %s\n",
				 __FUNCTION__, pname ? pname : "<unparsable>",
				 hostname);
			if (pname) k5_free_unparsed_name(context, pname);
			goto out;
		}
	}
	retval = gssd_get_single_krb5_cred(context, kt, ple);
out:
	if (kt)
		krb5_kt_close(context, kt);
	krb5_free_context(context);
	free(k5err);
	return retval;
}

/*
 * A common routine for getting the Kerberos error message
 */
char *
gssd_k5_err_msg(krb5_context context, krb5_error_code code)
{
	const char *origmsg;
	char *msg = NULL;

#if HAVE_KRB5_GET_ERROR_MESSAGE
	if (context != NULL) {
		origmsg = krb5_get_error_message(context, code);
		msg = strdup(origmsg);
		krb5_free_error_message(context, origmsg);
	}
#endif
	if (msg != NULL)
		return msg;
#if HAVE_KRB5
	return strdup(error_message(code));
#else
	if (context != NULL)
		return strdup(krb5_get_err_text(context, code));
	else
		return strdup(error_message(code));
#endif
}

/*
 * Return default Kerberos realm
 */
void
gssd_k5_get_default_realm(char **def_realm)
{
	krb5_context context;

	if (krb5_init_context(&context))
		return;

	krb5_get_default_realm(context, def_realm);

	krb5_free_context(context);
}
