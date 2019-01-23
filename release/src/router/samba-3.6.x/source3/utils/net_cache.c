/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) Rafal Szczesniak    2002

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
#include "net.h"

/**
 * @file net_cache.c
 * @brief This is part of the net tool which is basically command
 *        line wrapper for gencache.c functions (mainly for testing)
 *
 **/


/*
 * These routines are used via gencache_iterate() to display the cache's contents
 * (print_cache_entry) and to flush it (delete_cache_entry).
 * Both of them are defined by first arg of gencache_iterate() routine.
 */
static void print_cache_entry(const char* keystr, DATA_BLOB value,
                              const time_t timeout, void* dptr)
{
	char *timeout_str;
	char *alloc_str = NULL;
	const char *datastr;
	char *datastr_free = NULL;
	time_t now_t = time(NULL);
	struct tm timeout_tm, now_tm;
	struct tm *ptimeout_tm, *pnow_tm;

	ptimeout_tm = localtime_r(&timeout, &timeout_tm);
	if (ptimeout_tm == NULL) {
		return;
	}
	pnow_tm = localtime_r(&now_t, &now_tm);
	if (pnow_tm == NULL) {
		return;
	}

	/* form up timeout string depending whether it's today's date or not */
	if (timeout_tm.tm_year != now_tm.tm_year ||
			timeout_tm.tm_mon != now_tm.tm_mon ||
			timeout_tm.tm_mday != now_tm.tm_mday) {

		timeout_str = asctime(&timeout_tm);
		if (!timeout_str) {
			return;
		}
		timeout_str[strlen(timeout_str) - 1] = '\0';	/* remove tailing CR */
	} else {
		if (asprintf(&alloc_str, "%.2d:%.2d:%.2d", timeout_tm.tm_hour,
		         timeout_tm.tm_min, timeout_tm.tm_sec) == -1) {
			return;
		}
		timeout_str = alloc_str;
	}

	datastr = (char *)value.data;

	if ((value.length > 0) && (value.data[value.length-1] != '\0')) {
		datastr_free = talloc_asprintf(
			talloc_tos(), "<binary length %d>",
			(int)value.length);
		datastr = datastr_free;
		if (datastr == NULL) {
			datastr = "<binary>";
		}
	}

	d_printf(_("Key: %s\t Timeout: %s\t Value: %s  %s\n"), keystr,
	         timeout_str, datastr, timeout > now_t ? "": _("(expired)"));

	SAFE_FREE(alloc_str);
}

static void delete_cache_entry(const char* keystr, const char* datastr,
                               const time_t timeout, void* dptr)
{
	if (!gencache_del(keystr))
		d_fprintf(stderr, _("Couldn't delete entry! key = %s\n"),
			  keystr);
}


/**
 * Parse text representation of timeout value
 *
 * @param timeout_str string containing text representation of the timeout
 * @return numeric timeout of time_t type
 **/
static time_t parse_timeout(const char* timeout_str)
{
	char sign = '\0', *number = NULL, unit = '\0';
	int len, number_begin, number_end;
	time_t timeout;

	/* sign detection */
	if (timeout_str[0] == '!' || timeout_str[0] == '+') {
		sign = timeout_str[0];
		number_begin = 1;
	} else {
		number_begin = 0;
	}

	/* unit detection */
	len = strlen(timeout_str);
	switch (timeout_str[len - 1]) {
	case 's':
	case 'm':
	case 'h':
	case 'd':
	case 'w': unit = timeout_str[len - 1];
	}

	/* number detection */
	len = (sign) ? strlen(&timeout_str[number_begin]) : len;
	number_end = (unit) ? len - 1 : len;
	number = SMB_STRNDUP(&timeout_str[number_begin], number_end);

	/* calculate actual timeout value */
	timeout = (time_t)atoi(number);

	switch (unit) {
	case 'm': timeout *= 60; break;
	case 'h': timeout *= 60*60; break;
	case 'd': timeout *= 60*60*24; break;
	case 'w': timeout *= 60*60*24*7; break;  /* that's fair enough, I think :) */
	}

	switch (sign) {
	case '!': timeout = time(NULL) - timeout; break;
	case '+':
	default:  timeout += time(NULL); break;
	}

	if (number) SAFE_FREE(number);
	return timeout;
}


/**
 * Add an entry to the cache. If it does exist, then set it.
 *
 * @param c	A net_context structure
 * @param argv key, value and timeout are passed in command line
 * @return 0 on success, otherwise failure
 **/
static int net_cache_add(struct net_context *c, int argc, const char **argv)
{
	const char *keystr, *datastr, *timeout_str;
	time_t timeout;

	if (argc < 3 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net cache add <key string> <data string> "
			   "<timeout>\n"));
		return -1;
	}

	keystr = argv[0];
	datastr = argv[1];
	timeout_str = argv[2];

	/* parse timeout given in command line */
	timeout = parse_timeout(timeout_str);
	if (!timeout) {
		d_fprintf(stderr, _("Invalid timeout argument.\n"));
		return -1;
	}

	if (gencache_set(keystr, datastr, timeout)) {
		d_printf(_("New cache entry stored successfully.\n"));
		return 0;
	}

	d_fprintf(stderr, _("Entry couldn't be added. Perhaps there's already such a key.\n"));
	return -1;
}

/**
 * Delete an entry in the cache
 *
 * @param c	A net_context structure
 * @param argv key to delete an entry of
 * @return 0 on success, otherwise failure
 **/
static int net_cache_del(struct net_context *c, int argc, const char **argv)
{
	const char *keystr = argv[0];

	if (argc < 1 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net cache del <key string>\n"));
		return -1;
	}

	if(gencache_del(keystr)) {
		d_printf(_("Entry deleted.\n"));
		return 0;
	}

	d_fprintf(stderr, _("Couldn't delete specified entry\n"));
	return -1;
}


/**
 * Get and display an entry from the cache
 *
 * @param c	A net_context structure
 * @param argv key to search an entry of
 * @return 0 on success, otherwise failure
 **/
static int net_cache_get(struct net_context *c, int argc, const char **argv)
{
	const char* keystr = argv[0];
	DATA_BLOB value;
	time_t timeout;

	if (argc < 1 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net cache get <key>\n"));
		return -1;
	}

	if (gencache_get_data_blob(keystr, &value, &timeout, NULL)) {
		print_cache_entry(keystr, value, timeout, NULL);
		data_blob_free(&value);
		return 0;
	}

	d_fprintf(stderr, _("Failed to find entry\n"));
	return -1;
}


/**
 * Search an entry/entries in the cache
 *
 * @param c	A net_context structure
 * @param argv key pattern to match the entries to
 * @return 0 on success, otherwise failure
 **/
static int net_cache_search(struct net_context *c, int argc, const char **argv)
{
	const char* pattern;

	if (argc < 1 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net cache search <pattern>\n"));
		return -1;
	}

	pattern = argv[0];
	gencache_iterate_blobs(print_cache_entry, NULL, pattern);
	return 0;
}


/**
 * List the contents of the cache
 *
 * @param c	A net_context structure
 * @param argv ignored in this functionailty
 * @return always returns 0
 **/
static int net_cache_list(struct net_context *c, int argc, const char **argv)
{
	const char* pattern = "*";

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net cache list\n"
			   "    %s\n",
			 _("Usage:"),
			 _("List all cache entries."));
		return 0;
	}
	gencache_iterate_blobs(print_cache_entry, NULL, pattern);
	return 0;
}


/**
 * Flush the whole cache
 *
 * @param c	A net_context structure
 * @param argv ignored in this functionality
 * @return always returns 0
 **/
static int net_cache_flush(struct net_context *c, int argc, const char **argv)
{
	const char* pattern = "*";
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net cache flush\n"
			   "    %s",
			 _("Usage:"),
			 _("Delete all cache entries."));
		return 0;
	}
	gencache_iterate(delete_cache_entry, NULL, pattern);
	return 0;
}

static int net_cache_stabilize(struct net_context *c, int argc,
			       const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net cache stabilize\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Move transient cache content to stable storage"));
		return 0;
	}

	if (!gencache_stabilize()) {
		return -1;
	}
	return 0;
}
/**
 * Entry point to 'net cache' subfunctionality
 *
 * @param c	A net_context structure
 * @param argv arguments passed to further called functions
 * @return whatever further functions return
 **/
int net_cache(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"add",
			net_cache_add,
			NET_TRANSPORT_LOCAL,
			N_("Add new cache entry"),
			N_("net cache add <key string> <data string> <timeout>\n"
			   "  Add new cache entry.\n"
			   "    key string\tKey string to add cache data under.\n"
			   "    data string\tData to store under given key.\n"
			   "    timeout\tTimeout for cache data.")
		},
		{
			"del",
			net_cache_del,
			NET_TRANSPORT_LOCAL,
			N_("Delete existing cache entry by key"),
			N_("net cache del <key string>\n"
			   "  Delete existing cache entry by key.\n"
			   "    key string\tKey string to delete.")
		},
		{
			"get",
			net_cache_get,
			NET_TRANSPORT_LOCAL,
			N_("Get cache entry by key"),
			N_("net cache get <key string>\n"
			   "  Get cache entry by key.\n"
			   "    key string\tKey string to look up cache entry for.")

		},
		{
			"search",
			net_cache_search,
			NET_TRANSPORT_LOCAL,
			N_("Search entry by pattern"),
			N_("net cache search <pattern>\n"
			   "  Search entry by pattern.\n"
			   "    pattern\tPattern to search for in cache.")
		},
		{
			"list",
			net_cache_list,
			NET_TRANSPORT_LOCAL,
			N_("List all cache entries"),
			N_("net cache list\n"
			   "  List all cache entries")
		},
		{
			"flush",
			net_cache_flush,
			NET_TRANSPORT_LOCAL,
			N_("Delete all cache entries"),
			N_("net cache flush\n"
			   "  Delete all cache entries")
		},
		{
			"stabilize",
			net_cache_stabilize,
			NET_TRANSPORT_LOCAL,
			N_("Move transient cache content to stable storage"),
			N_("net cache stabilize\n"
			   "  Move transient cache content to stable storage")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net cache", func);
}
