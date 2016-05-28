/*
* CIFS user-space helper.
* Copyright (C) Igor Mammedov (niallain@gmail.com) 2007
* Copyright (C) Jeff Layton (jlayton@redhat.com) 2009
*
* Used by /sbin/request-key for handling
* cifs upcall for kerberos authorization of access to share and
* cifs upcall for DFS srver name resolving (IPv4/IPv6 aware).
* You should have keyutils installed and add something like the
* following lines to /etc/request-key.conf file:

create cifs.spnego * * /usr/local/sbin/cifs.upcall %k
create dns_resolver * * /usr/local/sbin/cifs.upcall %k

* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "includes.h"
#include <keyutils.h>
#include <getopt.h>

#include "cifs_spnego.h"

#define	CIFS_DEFAULT_KRB5_DIR		"/tmp"
#define	CIFS_DEFAULT_KRB5_PREFIX	"krb5cc_"

#define	MAX_CCNAME_LEN			PATH_MAX + 5

static const DATA_BLOB data_blob_null = { NULL, 0 };

const char *CIFSSPNEGO_VERSION = "1.3";
static const char *prog = "cifs.upcall";
typedef enum _sectype {
	NONE = 0,
	KRB5,
	MS_KRB5
} sectype_t;

static inline int
k5_data_equal(krb5_data d1, krb5_data d2, unsigned int length)
{
	if (!length)
		length = d1.length;

	return (d1.length == length &&
		d1.length == d2.length &&
		memcmp(d1.data, d2.data, length) == 0);

}

/* does the ccache have a valid TGT? */
static time_t
get_tgt_time(const char *ccname) {
	krb5_context context;
	krb5_ccache ccache;
	krb5_cc_cursor cur;
	krb5_creds creds;
	krb5_principal principal;
	krb5_data tgt = { .data =	"krbtgt",
			  .length =	6 };
	time_t credtime = 0;

	if (krb5_init_context(&context)) {
		syslog(LOG_DEBUG, "%s: unable to init krb5 context", __func__);
		return 0;
	}

	if (krb5_cc_resolve(context, ccname, &ccache)) {
		syslog(LOG_DEBUG, "%s: unable to resolve krb5 cache", __func__);
		goto err_cache;
	}

	if (krb5_cc_set_flags(context, ccache, 0)) {
		syslog(LOG_DEBUG, "%s: unable to set flags", __func__);
		goto err_cache;
	}

	if (krb5_cc_get_principal(context, ccache, &principal)) {
		syslog(LOG_DEBUG, "%s: unable to get principal", __func__);
		goto err_princ;
	}

	if (krb5_cc_start_seq_get(context, ccache, &cur)) {
		syslog(LOG_DEBUG, "%s: unable to seq start", __func__);
		goto err_ccstart;
	}

	while (!credtime && !krb5_cc_next_cred(context, ccache, &cur, &creds)) {
		if (k5_data_equal(creds.server->realm, principal->realm, 0) &&
		    k5_data_equal(creds.server->data[0], tgt, tgt.length) &&
		    k5_data_equal(creds.server->data[1], principal->realm, 0) &&
		    creds.times.endtime > time(NULL))
			credtime = creds.times.endtime;
                krb5_free_cred_contents(context, &creds);
        }
        krb5_cc_end_seq_get(context, ccache, &cur);

err_ccstart:
	krb5_free_principal(context, principal);
err_princ:
	krb5_cc_set_flags(context, ccache, KRB5_TC_OPENCLOSE);
	krb5_cc_close(context, ccache);
err_cache:
	krb5_free_context(context);
	return credtime;
}

static int
krb5cc_filter(const struct dirent *dirent)
{
	if (strstr(dirent->d_name, CIFS_DEFAULT_KRB5_PREFIX))
		return 1;
	else
		return 0;
}

/* search for a credcache that looks like a likely candidate */
static char *
find_krb5_cc(const char *dirname, uid_t uid)
{
	struct dirent **namelist;
	struct stat sbuf;
	char ccname[MAX_CCNAME_LEN], *credpath, *best_cache = NULL;
	int i, n;
	time_t cred_time, best_time = 0;

	n = scandir(dirname, &namelist, krb5cc_filter, NULL);
	if (n < 0) {
		syslog(LOG_DEBUG, "%s: scandir error on directory '%s': %s",
				  __func__, dirname, strerror(errno));
		return NULL;
	}

	for (i = 0; i < n; i++) {
		snprintf(ccname, sizeof(ccname), "FILE:%s/%s", dirname,
			 namelist[i]->d_name);
		credpath = ccname + 5;
		syslog(LOG_DEBUG, "%s: considering %s", __func__, credpath);

		if (lstat(credpath, &sbuf)) {
			syslog(LOG_DEBUG, "%s: stat error on '%s': %s",
					  __func__, credpath, strerror(errno));
			free(namelist[i]);
			continue;
		}
		if (sbuf.st_uid != uid) {
			syslog(LOG_DEBUG, "%s: %s is owned by %u, not %u",
					__func__, credpath, sbuf.st_uid, uid);
			free(namelist[i]);
			continue;
		}
		if (!S_ISREG(sbuf.st_mode)) {
			syslog(LOG_DEBUG, "%s: %s is not a regular file",
					__func__, credpath);
			free(namelist[i]);
			continue;
		}
		if (!(cred_time = get_tgt_time(ccname))) {
			syslog(LOG_DEBUG, "%s: %s is not a valid credcache.",
					__func__, ccname);
			free(namelist[i]);
			continue;
		}

		if (cred_time <= best_time) {
			syslog(LOG_DEBUG, "%s: %s expires sooner than current "
					  "best.", __func__, ccname);
			free(namelist[i]);
			continue;
		}

		syslog(LOG_DEBUG, "%s: %s is valid ccache", __func__, ccname);
		free(best_cache);
		best_cache = SMB_STRNDUP(ccname, MAX_CCNAME_LEN);
		best_time = cred_time;
		free(namelist[i]);
	}
	free(namelist);

	return best_cache;
}

/*
 * Prepares AP-REQ data for mechToken and gets session key
 * Uses credentials from cache. It will not ask for password
 * you should receive credentials for yuor name manually using
 * kinit or whatever you wish.
 *
 * in:
 * 	oid -		string with OID/ Could be OID_KERBEROS5
 * 			or OID_KERBEROS5_OLD
 * 	principal -	Service name.
 * 			Could be "cifs/FQDN" for KRB5 OID
 * 			or for MS_KRB5 OID style server principal
 * 			like "pdc$@YOUR.REALM.NAME"
 *
 * out:
 * 	secblob -	pointer for spnego wrapped AP-REQ data to be stored
 * 	sess_key-	pointer for SessionKey data to be stored
 *
 * ret: 0 - success, others - failure
 */
static int
handle_krb5_mech(const char *oid, const char *principal, DATA_BLOB *secblob,
		 DATA_BLOB *sess_key, const char *ccname)
{
	int retval;
	DATA_BLOB tkt, tkt_wrapped;

	syslog(LOG_DEBUG, "%s: getting service ticket for %s", __func__,
			  principal);

	/* get a kerberos ticket for the service and extract the session key */
	retval = cli_krb5_get_ticket(principal, 0, &tkt, sess_key, 0, ccname,
				     NULL);

	if (retval) {
		syslog(LOG_DEBUG, "%s: failed to obtain service ticket (%d)",
				  __func__, retval);
		return retval;
	}

	syslog(LOG_DEBUG, "%s: obtained service ticket", __func__);

	/* wrap that up in a nice GSS-API wrapping */
	tkt_wrapped = spnego_gen_krb5_wrap(tkt, TOK_ID_KRB_AP_REQ);

	/* and wrap that in a shiny SPNEGO wrapper */
	*secblob = gen_negTokenInit(oid, tkt_wrapped);

	data_blob_free(&tkt_wrapped);
	data_blob_free(&tkt);
	return retval;
}

#define DKD_HAVE_HOSTNAME	0x1
#define DKD_HAVE_VERSION	0x2
#define DKD_HAVE_SEC		0x4
#define DKD_HAVE_IP		0x8
#define DKD_HAVE_UID		0x10
#define DKD_HAVE_PID		0x20
#define DKD_MUSTHAVE_SET (DKD_HAVE_HOSTNAME|DKD_HAVE_VERSION|DKD_HAVE_SEC)

struct decoded_args {
	int		ver;
	char		*hostname;
	char		*ip;
	uid_t		uid;
	pid_t		pid;
	sectype_t	sec;
};

static unsigned int
decode_key_description(const char *desc, struct decoded_args *arg)
{
	int len;
	int retval = 0;
	char *pos;
	const char *tkn = desc;

	do {
		pos = index(tkn, ';');
		if (strncmp(tkn, "host=", 5) == 0) {

			if (pos == NULL)
				len = strlen(tkn);
			else
				len = pos - tkn;

			len -= 4;
			SAFE_FREE(arg->hostname);
			arg->hostname = SMB_XMALLOC_ARRAY(char, len);
			strlcpy(arg->hostname, tkn + 5, len);
			retval |= DKD_HAVE_HOSTNAME;
		} else if (!strncmp(tkn, "ip4=", 4) ||
			   !strncmp(tkn, "ip6=", 4)) {
			if (pos == NULL)
				len = strlen(tkn);
			else
				len = pos - tkn;

			len -= 3;
			SAFE_FREE(arg->ip);
			arg->ip = SMB_XMALLOC_ARRAY(char, len);
			strlcpy(arg->ip, tkn + 4, len);
			retval |= DKD_HAVE_IP;
		} else if (strncmp(tkn, "pid=", 4) == 0) {
			errno = 0;
			arg->pid = strtol(tkn + 4, NULL, 0);
			if (errno != 0) {
				syslog(LOG_ERR, "Invalid pid format: %s",
				       strerror(errno));
				return 1;
			} else {
				retval |= DKD_HAVE_PID;
			}
		} else if (strncmp(tkn, "sec=", 4) == 0) {
			if (strncmp(tkn + 4, "krb5", 4) == 0) {
				retval |= DKD_HAVE_SEC;
				arg->sec = KRB5;
			} else if (strncmp(tkn + 4, "mskrb5", 6) == 0) {
				retval |= DKD_HAVE_SEC;
				arg->sec = MS_KRB5;
			}
		} else if (strncmp(tkn, "uid=", 4) == 0) {
			errno = 0;
			arg->uid = strtol(tkn + 4, NULL, 16);
			if (errno != 0) {
				syslog(LOG_ERR, "Invalid uid format: %s",
				       strerror(errno));
				return 1;
			} else {
				retval |= DKD_HAVE_UID;
			}
		} else if (strncmp(tkn, "ver=", 4) == 0) {	/* if version */
			errno = 0;
			arg->ver = strtol(tkn + 4, NULL, 16);
			if (errno != 0) {
				syslog(LOG_ERR, "Invalid version format: %s",
				       strerror(errno));
				return 1;
			} else {
				retval |= DKD_HAVE_VERSION;
			}
		}
		if (pos == NULL)
			break;
		tkn = pos + 1;
	} while (tkn);
	return retval;
}

static int
cifs_resolver(const key_serial_t key, const char *key_descr)
{
	int c;
	struct addrinfo *addr;
	char ip[INET6_ADDRSTRLEN];
	void *p;
	const char *keyend = key_descr;
	/* skip next 4 ';' delimiters to get to description */
	for (c = 1; c <= 4; c++) {
		keyend = index(keyend+1, ';');
		if (!keyend) {
			syslog(LOG_ERR, "invalid key description: %s",
					key_descr);
			return 1;
		}
	}
	keyend++;

	/* resolve name to ip */
	c = getaddrinfo(keyend, NULL, NULL, &addr);
	if (c) {
		syslog(LOG_ERR, "unable to resolve hostname: %s [%s]",
				keyend, gai_strerror(c));
		return 1;
	}

	/* conver ip to string form */
	if (addr->ai_family == AF_INET)
		p = &(((struct sockaddr_in *)addr->ai_addr)->sin_addr);
	else
		p = &(((struct sockaddr_in6 *)addr->ai_addr)->sin6_addr);

	if (!inet_ntop(addr->ai_family, p, ip, sizeof(ip))) {
		syslog(LOG_ERR, "%s: inet_ntop: %s", __func__, strerror(errno));
		freeaddrinfo(addr);
		return 1;
	}

	/* setup key */
	c = keyctl_instantiate(key, ip, strlen(ip)+1, 0);
	if (c == -1) {
		syslog(LOG_ERR, "%s: keyctl_instantiate: %s", __func__,
				strerror(errno));
		freeaddrinfo(addr);
		return 1;
	}

	freeaddrinfo(addr);
	return 0;
}

/*
 * Older kernels sent IPv6 addresses without colons. Well, at least
 * they're fixed-length strings. Convert these addresses to have colon
 * delimiters to make getaddrinfo happy.
 */
static void
convert_inet6_addr(const char *from, char *to)
{
	int i = 1;

	while (*from) {
		*to++ = *from++;
		if (!(i++ % 4) && *from)
			*to++ = ':';
	}
	*to = 0;
}

static int
ip_to_fqdn(const char *addrstr, char *host, size_t hostlen)
{
	int rc;
	struct addrinfo hints = { .ai_flags = AI_NUMERICHOST };
	struct addrinfo *res;
	const char *ipaddr = addrstr;
	char converted[INET6_ADDRSTRLEN + 1];

	if ((strlen(ipaddr) > INET_ADDRSTRLEN) && !strchr(ipaddr, ':')) {
		convert_inet6_addr(ipaddr, converted);
		ipaddr = converted;
	}

	rc = getaddrinfo(ipaddr, NULL, &hints, &res);
	if (rc) {
		syslog(LOG_DEBUG, "%s: failed to resolve %s to "
			"ipaddr: %s", __func__, ipaddr,
		rc == EAI_SYSTEM ? strerror(errno) : gai_strerror(rc));
		return rc;
	}

	rc = getnameinfo(res->ai_addr, res->ai_addrlen, host, hostlen,
			 NULL, 0, NI_NAMEREQD);
	freeaddrinfo(res);
	if (rc) {
		syslog(LOG_DEBUG, "%s: failed to resolve %s to fqdn: %s",
			__func__, ipaddr,
			rc == EAI_SYSTEM ? strerror(errno) : gai_strerror(rc));
		return rc;
	}

	syslog(LOG_DEBUG, "%s: resolved %s to %s", __func__, ipaddr, host);
	return 0;
}

static void
usage(void)
{
	syslog(LOG_INFO, "Usage: %s [-t] [-v] key_serial", prog);
	fprintf(stderr, "Usage: %s [-t] [-v] key_serial\n", prog);
}

const struct option long_options[] = {
	{ "trust-dns",	0, NULL, 't' },
	{ "version",	0, NULL, 'v' },
	{ NULL,		0, NULL, 0 }
};

int main(const int argc, char *const argv[])
{
	struct cifs_spnego_msg *keydata = NULL;
	DATA_BLOB secblob = data_blob_null;
	DATA_BLOB sess_key = data_blob_null;
	key_serial_t key = 0;
	size_t datalen;
	unsigned int have;
	long rc = 1;
	int c, try_dns = 0;
	char *buf, *princ = NULL, *ccname = NULL;
	char hostbuf[NI_MAXHOST], *host;
	struct decoded_args arg = { };
	const char *oid;

	hostbuf[0] = '\0';

	openlog(prog, 0, LOG_DAEMON);

	while ((c = getopt_long(argc, argv, "ctv", long_options, NULL)) != -1) {
		switch (c) {
		case 'c':
			/* legacy option -- skip it */
			break;
		case 't':
			try_dns++;
			break;
		case 'v':
			printf("version: %s\n", CIFSSPNEGO_VERSION);
			goto out;
		default:
			syslog(LOG_ERR, "unknown option: %c", c);
			goto out;
		}
	}

	/* is there a key? */
	if (argc <= optind) {
		usage();
		goto out;
	}

	/* get key and keyring values */
	errno = 0;
	key = strtol(argv[optind], NULL, 10);
	if (errno != 0) {
		key = 0;
		syslog(LOG_ERR, "Invalid key format: %s", strerror(errno));
		goto out;
	}

	rc = keyctl_describe_alloc(key, &buf);
	if (rc == -1) {
		syslog(LOG_ERR, "keyctl_describe_alloc failed: %s",
		       strerror(errno));
		rc = 1;
		goto out;
	}

	syslog(LOG_DEBUG, "key description: %s", buf);

	if ((strncmp(buf, "cifs.resolver", sizeof("cifs.resolver")-1) == 0) ||
	    (strncmp(buf, "dns_resolver", sizeof("dns_resolver")-1) == 0)) {
		rc = cifs_resolver(key, buf);
		goto out;
	}

	have = decode_key_description(buf, &arg);
	SAFE_FREE(buf);
	if ((have & DKD_MUSTHAVE_SET) != DKD_MUSTHAVE_SET) {
		syslog(LOG_ERR, "unable to get necessary params from key "
				"description (0x%x)", have);
		rc = 1;
		goto out;
	}

	if (arg.ver > CIFS_SPNEGO_UPCALL_VERSION) {
		syslog(LOG_ERR, "incompatible kernel upcall version: 0x%x",
				arg.ver);
		rc = 1;
		goto out;
	}

	if (have & DKD_HAVE_UID) {
		rc = setuid(arg.uid);
		if (rc == -1) {
			syslog(LOG_ERR, "setuid: %s", strerror(errno));
			goto out;
		}

		ccname = find_krb5_cc(CIFS_DEFAULT_KRB5_DIR, arg.uid);
	}

	host = arg.hostname;

	// do mech specific authorization
	switch (arg.sec) {
	case MS_KRB5:
	case KRB5:
retry_new_hostname:
		/* for "cifs/" service name + terminating 0 */
		datalen = strlen(host) + 5 + 1;
		princ = SMB_XMALLOC_ARRAY(char, datalen);
		if (!princ) {
			rc = -ENOMEM;
			break;
		}

		if (arg.sec == MS_KRB5)
			oid = OID_KERBEROS5_OLD;
		else
			oid = OID_KERBEROS5;

		/*
		 * try getting a cifs/ principal first and then fall back to
		 * getting a host/ principal if that doesn't work.
		 */
		strlcpy(princ, "cifs/", datalen);
		strlcpy(princ + 5, host, datalen - 5);
		rc = handle_krb5_mech(oid, princ, &secblob, &sess_key, ccname);
		if (!rc)
			break;

		memcpy(princ, "host/", 5);
		rc = handle_krb5_mech(oid, princ, &secblob, &sess_key, ccname);
		if (!rc)
			break;

		if (!try_dns || !(have & DKD_HAVE_IP))
			break;

		rc = ip_to_fqdn(arg.ip, hostbuf, sizeof(hostbuf));
		if (rc)
			break;

		SAFE_FREE(princ);
		try_dns = 0;
		host = hostbuf;
		goto retry_new_hostname;
	default:
		syslog(LOG_ERR, "sectype: %d is not implemented", arg.sec);
		rc = 1;
		break;
	}

	SAFE_FREE(princ);

	if (rc)
		goto out;

	/* pack SecurityBLob and SessionKey into downcall packet */
	datalen =
	    sizeof(struct cifs_spnego_msg) + secblob.length + sess_key.length;
	keydata = (struct cifs_spnego_msg*)SMB_XMALLOC_ARRAY(char, datalen);
	if (!keydata) {
		rc = 1;
		goto out;
	}
	keydata->version = arg.ver;
	keydata->flags = 0;
	keydata->sesskey_len = sess_key.length;
	keydata->secblob_len = secblob.length;
	memcpy(&(keydata->data), sess_key.data, sess_key.length);
	memcpy(&(keydata->data) + keydata->sesskey_len,
	       secblob.data, secblob.length);

	/* setup key */
	rc = keyctl_instantiate(key, keydata, datalen, 0);
	if (rc == -1) {
		syslog(LOG_ERR, "keyctl_instantiate: %s", strerror(errno));
		goto out;
	}

	/* BB: maybe we need use timeout for key: for example no more then
	 * ticket lifietime? */
	/* keyctl_set_timeout( key, 60); */
out:
	/*
	 * on error, negatively instantiate the key ourselves so that we can
	 * make sure the kernel doesn't hang it off of a searchable keyring
	 * and interfere with the next attempt to instantiate the key.
	 */
	if (rc != 0  && key == 0)
		keyctl_negate(key, 1, KEY_REQKEY_DEFL_DEFAULT);
	data_blob_free(&secblob);
	data_blob_free(&sess_key);
	SAFE_FREE(ccname);
	SAFE_FREE(arg.hostname);
	SAFE_FREE(arg.ip);
	SAFE_FREE(keydata);
	return rc;
}
