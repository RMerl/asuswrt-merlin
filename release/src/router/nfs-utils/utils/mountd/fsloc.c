/*
 * COPYRIGHT (c) 2006
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "fsloc.h"
#include "exportfs.h"

/* Debugging tool: prints out @servers info to syslog */
static void replicas_print(struct servers *sp)
{
	int i;
	if (!sp) {
		xlog(L_NOTICE, "NULL replicas pointer\n");
		return;
	}
	xlog(L_NOTICE, "replicas listsize=%i\n", sp->h_num);
	for (i=0; i<sp->h_num; i++) {
		xlog(L_NOTICE, "    %s:%s\n",
		       sp->h_mp[i]->h_host, sp->h_mp[i]->h_path);
	}
}

#ifdef DEBUG
/* Called by setting 'Method = stub' in config file.  Just returns
 * some syntactically correct gibberish for testing purposes.
 */
static struct servers *method_stub(char *key)
{
	struct servers *sp;
	struct mount_point *mp;

	xlog(L_NOTICE, "called method_stub\n");
	sp = malloc(sizeof(struct servers));
	if (!sp)
		return NULL;
	mp = calloc(1, sizeof(struct mount_point));
	if (!mp) {
		free(sp);
		return NULL;
	}
	sp->h_num = 1;
	sp->h_mp[0] = mp;
	mp->h_host = strdup("stub_server");
	mp->h_path = strdup("/my/test/path");
	sp->h_referral = 1;
	return sp;
}
#endif	/* DEBUG */

/* Scan @list, which is a NULL-terminated array of strings of the
 * form path@host[+host], and return corresponding servers structure.
 */
static struct servers *parse_list(char **list)
{
	int i;
	struct servers *res;
	struct mount_point *mp;
	char *cp;

	res = malloc(sizeof(struct servers));
	if (!res)
		return NULL;
	res->h_num = 0;

	/* parse each of the answers in sucession. */
	for (i=0; list[i] && i<FSLOC_MAX_LIST; i++) {
		mp = calloc(1, sizeof(struct mount_point));
		if (!mp) {
			release_replicas(res);
			return NULL;
		}
		cp = strchr(list[i], '@');
		if ((!cp) || list[i][0] != '/') {
			xlog(L_WARNING, "invalid entry '%s'", list[i]);
			continue; /* XXX Need better error handling */
		}
		res->h_mp[i] = mp;
		res->h_num++;
		mp->h_path = strndup(list[i], cp - list[i]);
		cp++;
		mp->h_host = strdup(cp);
		/* hosts are '+' separated, kernel expects ':' separated */
		while ( (cp = strchr(mp->h_host, '+')) )
		       *cp = ':';
	}
	return res;
}

/* @data is a string of form path@host[+host][:path@host[+host]]
 */
static struct servers *method_list(char *data)
{
	char *copy, *ptr=data;
	char **list;
	int i, listsize;
	struct servers *rv=NULL;

	xlog(L_NOTICE, "method_list(%s)\n", data);
	for (ptr--, listsize=1; ptr; ptr=index(ptr, ':'), listsize++)
		ptr++;
	list = malloc(listsize * sizeof(char *));
	copy = strdup(data);
	if (copy)
		xlog(L_NOTICE, "converted to %s\n", copy);
	if (list && copy) {
		ptr = copy;
		for (i=0; i<listsize; i++) {
			list[i] = strsep(&ptr, ":");
		}
		rv = parse_list(list);
	}
	free(copy);
	free(list);
	replicas_print(rv);
	return rv;
}

/* Returns appropriately filled struct servers, or NULL if had a problem */
struct servers *replicas_lookup(int method, char *data, char *key)
{
	struct servers *sp=NULL;
	switch(method) {
	case FSLOC_NONE:
		break;
	case FSLOC_REFER:
		sp = method_list(data);
		if (sp)
			sp->h_referral = 1;
		break;
	case FSLOC_REPLICA:
		sp = method_list(data);
		if (sp)
			sp->h_referral = 0;
		break;
#ifdef DEBUG
	case FSLOC_STUB:
		sp = method_stub(data);
		break;
#endif
	default:
		xlog(L_WARNING, "Unknown method = %i", method);
	}
	replicas_print(sp);
	return sp;
}

void release_replicas(struct servers *server)
{
	int i;

	if (!server) return;
	for (i = 0; i < server->h_num; i++) {
		free(server->h_mp[i]->h_host);
		free(server->h_mp[i]->h_path);
		free(server->h_mp[i]);
	}
	free(server);
}
