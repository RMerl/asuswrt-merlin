/*
* CIFS user-space helper.
* Copyright (C) Igor Mammedov (niallain@gmail.com) 2007
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

#include "cifs_spnego.h"

const char *CIFSSPNEGO_VERSION = "1.2";
static const char *prog = "cifs.upcall";
typedef enum _secType {
	NONE = 0,
	KRB5,
	MS_KRB5
} secType_t;
const DATA_BLOB data_blob_null = { NULL, 0, NULL };

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
handle_krb5_mech(const char *oid, const char *principal,
		     DATA_BLOB * secblob, DATA_BLOB * sess_key)
{
	int retval;
	DATA_BLOB tkt, tkt_wrapped;

	/* get a kerberos ticket for the service and extract the session key */
	retval = cli_krb5_get_ticket(principal, 0,
				     &tkt, sess_key, 0, NULL, NULL);

	if (retval)
		return retval;

	/* wrap that up in a nice GSS-API wrapping */
	tkt_wrapped = spnego_gen_krb5_wrap(tkt, TOK_ID_KRB_AP_REQ);

	/* and wrap that in a shiny SPNEGO wrapper */
	*secblob = gen_negTokenInit(oid, tkt_wrapped);

	data_blob_free(&tkt_wrapped);
	data_blob_free(&tkt);
	return retval;
}

#define DKD_HAVE_HOSTNAME	1
#define DKD_HAVE_VERSION	2
#define DKD_HAVE_SEC		4
#define DKD_HAVE_IPV4		8
#define DKD_HAVE_IPV6		16
#define DKD_HAVE_UID		32
#define DKD_MUSTHAVE_SET (DKD_HAVE_HOSTNAME|DKD_HAVE_VERSION|DKD_HAVE_SEC)

static int
decode_key_description(const char *desc, int *ver, secType_t * sec,
			   char **hostname, uid_t * uid)
{
	int retval = 0;
	char *pos;
	const char *tkn = desc;

	do {
		pos = index(tkn, ';');
		if (strncmp(tkn, "host=", 5) == 0) {
			int len;

			if (pos == NULL) {
				len = strlen(tkn);
			} else {
				len = pos - tkn;
			}
			len -= 4;
			SAFE_FREE(*hostname);
			*hostname = SMB_XMALLOC_ARRAY(char, len);
			strlcpy(*hostname, tkn + 5, len);
			retval |= DKD_HAVE_HOSTNAME;
		} else if (strncmp(tkn, "ipv4=", 5) == 0) {
			/* BB: do we need it if we have hostname already? */
		} else if (strncmp(tkn, "ipv6=", 5) == 0) {
			/* BB: do we need it if we have hostname already? */
		} else if (strncmp(tkn, "sec=", 4) == 0) {
			if (strncmp(tkn + 4, "krb5", 4) == 0) {
				retval |= DKD_HAVE_SEC;
				*sec = KRB5;
			} else if (strncmp(tkn + 4, "mskrb5", 6) == 0) {
				retval |= DKD_HAVE_SEC;
				*sec = MS_KRB5;
			}
		} else if (strncmp(tkn, "uid=", 4) == 0) {
			errno = 0;
			*uid = strtol(tkn + 4, NULL, 16);
			if (errno != 0) {
				syslog(LOG_WARNING, "Invalid uid format: %s",
				       strerror(errno));
				return 1;
			} else {
				retval |= DKD_HAVE_UID;
			}
		} else if (strncmp(tkn, "ver=", 4) == 0) {	/* if version */
			errno = 0;
			*ver = strtol(tkn + 4, NULL, 16);
			if (errno != 0) {
				syslog(LOG_WARNING,
				       "Invalid version format: %s",
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
			syslog(LOG_WARNING, "invalid key description: %s",
					key_descr);
			return 1;
		}
	}
	keyend++;

	/* resolve name to ip */
	c = getaddrinfo(keyend, NULL, NULL, &addr);
	if (c) {
		syslog(LOG_WARNING, "unable to resolve hostname: %s [%s]",
				keyend, gai_strerror(c));
		return 1;
	}

	/* conver ip to string form */
	if (addr->ai_family == AF_INET) {
		p = &(((struct sockaddr_in *)addr->ai_addr)->sin_addr);
	} else {
		p = &(((struct sockaddr_in6 *)addr->ai_addr)->sin6_addr);
	}
	if (!inet_ntop(addr->ai_family, p, ip, sizeof(ip))) {
		syslog(LOG_WARNING, "%s: inet_ntop: %s",
				__FUNCTION__, strerror(errno));
		freeaddrinfo(addr);
		return 1;
	}

	/* setup key */
	c = keyctl_instantiate(key, ip, strlen(ip)+1, 0);
	if (c == -1) {
		syslog(LOG_WARNING, "%s: keyctl_instantiate: %s",
				__FUNCTION__, strerror(errno));
		freeaddrinfo(addr);
		return 1;
	}

	freeaddrinfo(addr);
	return 0;
}

static void
usage(void)
{
	syslog(LOG_WARNING, "Usage: %s [-c] [-v] key_serial", prog);
	fprintf(stderr, "Usage: %s [-c] [-v] key_serial\n", prog);
}

int main(const int argc, char *const argv[])
{
	struct cifs_spnego_msg *keydata = NULL;
	DATA_BLOB secblob = data_blob_null;
	DATA_BLOB sess_key = data_blob_null;
	secType_t sectype = NONE;
	key_serial_t key = 0;
	size_t datalen;
	long rc = 1;
	uid_t uid = 0;
	int kernel_upcall_version = 0;
	int c, use_cifs_service_prefix = 0;
	char *buf, *hostname = NULL;
	const char *oid;

	openlog(prog, 0, LOG_DAEMON);

	while ((c = getopt(argc, argv, "cv")) != -1) {
		switch (c) {
		case 'c':{
			use_cifs_service_prefix = 1;
			break;
			}
		case 'v':{
			printf("version: %s\n", CIFSSPNEGO_VERSION);
			goto out;
			}
		default:{
			syslog(LOG_WARNING, "unknown option: %c", c);
			goto out;
			}
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
		syslog(LOG_WARNING, "Invalid key format: %s", strerror(errno));
		goto out;
	}

	rc = keyctl_describe_alloc(key, &buf);
	if (rc == -1) {
		syslog(LOG_WARNING, "keyctl_describe_alloc failed: %s",
		       strerror(errno));
		rc = 1;
		goto out;
	}

	if ((strncmp(buf, "cifs.resolver", sizeof("cifs.resolver")-1) == 0) ||
	    (strncmp(buf, "dns_resolver", sizeof("dns_resolver")-1) == 0)) {
		rc = cifs_resolver(key, buf);
		goto out;
	}

	rc = decode_key_description(buf, &kernel_upcall_version, &sectype,
				    &hostname, &uid);
	if ((rc & DKD_MUSTHAVE_SET) != DKD_MUSTHAVE_SET) {
		syslog(LOG_WARNING,
		       "unable to get from description necessary params");
		rc = 1;
		SAFE_FREE(buf);
		goto out;
	}
	SAFE_FREE(buf);

	if (kernel_upcall_version > CIFS_SPNEGO_UPCALL_VERSION) {
		syslog(LOG_WARNING,
		       "incompatible kernel upcall version: 0x%x",
		       kernel_upcall_version);
		rc = 1;
		goto out;
	}

	if (rc & DKD_HAVE_UID) {
		rc = setuid(uid);
		if (rc == -1) {
			syslog(LOG_WARNING, "setuid: %s", strerror(errno));
			goto out;
		}
	}

	/* BB: someday upcall SPNEGO blob could be checked here to decide
	 * what mech to use */

	// do mech specific authorization
	switch (sectype) {
	case MS_KRB5:
	case KRB5:{
			char *princ;
			size_t len;

			/* for "cifs/" service name + terminating 0 */
			len = strlen(hostname) + 5 + 1;
			princ = SMB_XMALLOC_ARRAY(char, len);
			if (!princ) {
				rc = 1;
				break;
			}
			if (use_cifs_service_prefix) {
				strlcpy(princ, "cifs/", len);
			} else {
				strlcpy(princ, "host/", len);
			}
			strlcpy(princ + 5, hostname, len - 5);

			if (sectype == MS_KRB5)
				oid = OID_KERBEROS5_OLD;
			else
				oid = OID_KERBEROS5;

			rc = handle_krb5_mech(oid, princ, &secblob, &sess_key);
			SAFE_FREE(princ);
			break;
		}
	default:{
			syslog(LOG_WARNING, "sectype: %d is not implemented",
			       sectype);
			rc = 1;
			break;
		}
	}

	if (rc) {
		goto out;
	}

	/* pack SecurityBLob and SessionKey into downcall packet */
	datalen =
	    sizeof(struct cifs_spnego_msg) + secblob.length + sess_key.length;
	keydata = (struct cifs_spnego_msg*)SMB_XMALLOC_ARRAY(char, datalen);
	if (!keydata) {
		rc = 1;
		goto out;
	}
	keydata->version = kernel_upcall_version;
	keydata->flags = 0;
	keydata->sesskey_len = sess_key.length;
	keydata->secblob_len = secblob.length;
	memcpy(&(keydata->data), sess_key.data, sess_key.length);
	memcpy(&(keydata->data) + keydata->sesskey_len,
	       secblob.data, secblob.length);

	/* setup key */
	rc = keyctl_instantiate(key, keydata, datalen, 0);
	if (rc == -1) {
		syslog(LOG_WARNING, "keyctl_instantiate: %s", strerror(errno));
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
	SAFE_FREE(hostname);
	SAFE_FREE(keydata);
	return rc;
}
