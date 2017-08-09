/*
   Unix SMB/CIFS implementation.
   a WINS nsswitch module
   Copyright (C) Andrew Tridgell 1999

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "includes.h"
#include "nsswitch/winbind_nss.h"

#ifdef HAVE_NS_API_H

#include <ns_daemon.h>
#endif

#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

#if HAVE_PTHREAD
static pthread_mutex_t wins_nss_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifndef INADDRSZ
#define INADDRSZ 4
#endif

static int initialised;

NSS_STATUS _nss_wins_gethostbyname_r(const char *hostname, struct hostent *he,
			  char *buffer, size_t buflen, int *h_errnop);
NSS_STATUS _nss_wins_gethostbyname2_r(const char *name, int af, struct hostent *he,
			   char *buffer, size_t buflen, int *h_errnop);

static void nss_wins_init(void)
{
	initialised = 1;
	load_case_tables_library();
	lp_set_cmdline("log level", "0");

	TimeInit();
	setup_logging("nss_wins",False);
	lp_load(get_dyn_CONFIGFILE(),True,False,False,True);
	load_interfaces();
}

static struct in_addr *lookup_byname_backend(const char *name, int *count)
{
	struct ip_service *address = NULL;
	struct in_addr *ret = NULL;
	int j;

	if (!initialised) {
		nss_wins_init();
	}

	*count = 0;

	/* always try with wins first */
	if (NT_STATUS_IS_OK(resolve_wins(name,0x00,&address,count))) {
		if ( (ret = SMB_MALLOC_P(struct in_addr)) == NULL ) {
			free( address );
			return NULL;
		}
		if (address[0].ss.ss_family != AF_INET) {
			free(address);
			free(ret);
			return NULL;
		}
		*ret = ((struct sockaddr_in *)(void *)&address[0].ss)
			->sin_addr;
		free( address );
		return ret;
	}

	/* uggh, we have to broadcast to each interface in turn */
	for (j=iface_count() - 1;j >= 0;j--) {
		const struct in_addr *bcast = iface_n_bcast_v4(j);
		struct sockaddr_storage ss;
		struct sockaddr_storage *pss;
		NTSTATUS status;

		if (!bcast) {
			continue;
		}
		in_addr_to_sockaddr_storage(&ss, *bcast);
		status = name_query(name, 0x00, True, True, &ss,
				    NULL, &pss, count, NULL);
		if (NT_STATUS_IS_OK(status) && (*count > 0)) {
			if ((ret = SMB_MALLOC_P(struct in_addr)) == NULL) {
				return NULL;
			}
			*ret = ((struct sockaddr_in *)pss)->sin_addr;
			TALLOC_FREE(pss);
			break;
		}
	}

	return ret;
}

#ifdef HAVE_NS_API_H

static struct node_status *lookup_byaddr_backend(char *addr, int *count)
{
	struct sockaddr_storage ss;
	struct nmb_name nname;
	struct node_status *result;
	NTSTATUS status;

	if (!initialised) {
		nss_wins_init();
	}

	make_nmb_name(&nname, "*", 0);
	if (!interpret_string_addr(&ss, addr, AI_NUMERICHOST)) {
		return NULL;
	}
	status = node_status_query(NULL, &nname, &ss, &result, count, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	return result;
}

/* IRIX version */

int init(void)
{
	nsd_logprintf(NSD_LOG_MIN, "entering init (wins)\n");
	nss_wins_init();
	return NSD_OK;
}

int lookup(nsd_file_t *rq)
{
	char *map;
	char *key;
	char *addr;
	struct in_addr *ip_list;
	struct node_status *status;
	int i, count, len, size;
	char response[1024];
	bool found = False;

	nsd_logprintf(NSD_LOG_MIN, "entering lookup (wins)\n");
	if (! rq)
		return NSD_ERROR;

	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char*)0);
	if (! map) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char*)0);
	if (! key || ! *key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	response[0] = '\0';
	len = sizeof(response) - 2;

	/*
	 * response needs to be a string of the following format
	 * ip_address[ ip_address]*\tname[ alias]*
	 */
	if (StrCaseCmp(map,"hosts.byaddr") == 0) {
		if ( status = lookup_byaddr_backend(key, &count)) {
		    size = strlen(key) + 1;
		    if (size > len) {
			talloc_free(status);
			return NSD_ERROR;
		    }
		    len -= size;
		    strncat(response,key,size);
		    strncat(response,"\t",1);
		    for (i = 0; i < count; i++) {
			/* ignore group names */
			if (status[i].flags & 0x80) continue;
			if (status[i].type == 0x20) {
				size = sizeof(status[i].name) + 1;
				if (size > len) {
				    talloc_free(status);
				    return NSD_ERROR;
				}
				len -= size;
				strncat(response, status[i].name, size);
				strncat(response, " ", 1);
				found = True;
			}
		    }
		    response[strlen(response)-1] = '\n';
		    talloc_free(status);
		}
	} else if (StrCaseCmp(map,"hosts.byname") == 0) {
	    if (ip_list = lookup_byname_backend(key, &count)) {
		for (i = count; i ; i--) {
		    addr = inet_ntoa(ip_list[i-1]);
		    size = strlen(addr) + 1;
		    if (size > len) {
			free(ip_list);
			return NSD_ERROR;
		    }
		    len -= size;
		    if (i != 0)
			response[strlen(response)-1] = ' ';
		    strncat(response,addr,size);
		    strncat(response,"\t",1);
		}
		size = strlen(key) + 1;
		if (size > len) {
		    free(ip_list);
		    return NSD_ERROR;
		}
		strncat(response,key,size);
		strncat(response,"\n",1);
		found = True;
		free(ip_list);
	    }
	}

	if (found) {
	    nsd_logprintf(NSD_LOG_LOW, "lookup (wins %s) %s\n",map,response);
	    nsd_set_result(rq,NS_SUCCESS,response,strlen(response),VOLATILE);
	    return NSD_OK;
	}
	nsd_logprintf(NSD_LOG_LOW, "lookup (wins) not found\n");
	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

#else

/* Allocate some space from the nss static buffer.  The buffer and buflen
   are the pointers passed in by the C library to the _nss_*_*
   functions. */

static char *get_static(char **buffer, size_t *buflen, int len)
{
	char *result;

	/* Error check.  We return false if things aren't set up right, or
	   there isn't enough buffer space left. */

	if ((buffer == NULL) || (buflen == NULL) || (*buflen < len)) {
		return NULL;
	}

	/* Return an index into the static buffer */

	result = *buffer;
	*buffer += len;
	*buflen -= len;

	return result;
}

/****************************************************************************
gethostbyname() - we ignore any domain portion of the name and only
handle names that are at most 15 characters long
  **************************************************************************/
NSS_STATUS
_nss_wins_gethostbyname_r(const char *hostname, struct hostent *he,
			  char *buffer, size_t buflen, int *h_errnop)
{
	NSS_STATUS nss_status = NSS_STATUS_SUCCESS;
	struct in_addr *ip_list;
	int i, count;
	fstring name;
	size_t namelen;
	TALLOC_CTX *frame;

#if HAVE_PTHREAD
	pthread_mutex_lock(&wins_nss_mutex);
#endif

	frame = talloc_stackframe();

	memset(he, '\0', sizeof(*he));
	fstrcpy(name, hostname);

	/* Do lookup */

	ip_list = lookup_byname_backend(name, &count);

	if (!ip_list) {
		nss_status = NSS_STATUS_NOTFOUND;
		goto out;
	}

	/* Copy h_name */

	namelen = strlen(name) + 1;

	if ((he->h_name = get_static(&buffer, &buflen, namelen)) == NULL) {
		free(ip_list);
		nss_status = NSS_STATUS_TRYAGAIN;
		goto out;
	}

	memcpy(he->h_name, name, namelen);

	/* Copy h_addr_list, align to pointer boundary first */

	if ((i = (unsigned long)(buffer) % sizeof(char*)) != 0)
		i = sizeof(char*) - i;

	if (get_static(&buffer, &buflen, i) == NULL) {
		free(ip_list);
		nss_status = NSS_STATUS_TRYAGAIN;
		goto out;
	}

	if ((he->h_addr_list = (char **)get_static(
		     &buffer, &buflen, (count + 1) * sizeof(char *))) == NULL) {
		free(ip_list);
		nss_status = NSS_STATUS_TRYAGAIN;
		goto out;
	}

	for (i = 0; i < count; i++) {
		if ((he->h_addr_list[i] = get_static(&buffer, &buflen,
						     INADDRSZ)) == NULL) {
			free(ip_list);
			nss_status = NSS_STATUS_TRYAGAIN;
			goto out;
		}
		memcpy(he->h_addr_list[i], &ip_list[i], INADDRSZ);
	}

	he->h_addr_list[count] = NULL;

	free(ip_list);

	/* Set h_addr_type and h_length */

	he->h_addrtype = AF_INET;
	he->h_length = INADDRSZ;

	/* Set h_aliases */

	if ((i = (unsigned long)(buffer) % sizeof(char*)) != 0)
		i = sizeof(char*) - i;

	if (get_static(&buffer, &buflen, i) == NULL) {
		nss_status = NSS_STATUS_TRYAGAIN;
		goto out;
	}

	if ((he->h_aliases = (char **)get_static(
		     &buffer, &buflen, sizeof(char *))) == NULL) {
		nss_status = NSS_STATUS_TRYAGAIN;
		goto out;
	}

	he->h_aliases[0] = NULL;

	nss_status = NSS_STATUS_SUCCESS;

  out:

	TALLOC_FREE(frame);

#if HAVE_PTHREAD
	pthread_mutex_unlock(&wins_nss_mutex);
#endif
	return nss_status;
}


NSS_STATUS
_nss_wins_gethostbyname2_r(const char *name, int af, struct hostent *he,
			   char *buffer, size_t buflen, int *h_errnop)
{
	NSS_STATUS nss_status;

	if(af!=AF_INET) {
		*h_errnop = NO_DATA;
		nss_status = NSS_STATUS_UNAVAIL;
	} else {
		nss_status = _nss_wins_gethostbyname_r(
				name, he, buffer, buflen, h_errnop);
	}
	return nss_status;
}
#endif
