/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * CGI helper functions
 *
 * Copyright 2003, ASUSTeK Inc.
 * All Rights Reserved.		
 *				     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;   
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior      
 * written permission of ASUSTeK Inc..			    
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>    //Viz
#include <stdarg.h>   //Viz add 2010.08
#ifdef BCMDBG
#include <assert.h>
#else
#define assert(a)
#endif

#include <json.h>

#if defined(linux)
/* Use SVID search */
#define __USE_GNU
#include <search.h>
#elif defined(vxworks)
/* Use vxsearch */
#include <vxsearch.h>
extern char *strsep(char **stringp, char *delim);
#endif

/* CGI hash table */
static struct hsearch_data htab;

static void
unescape(char *s)
{
	unsigned int c;

	while ((s = strpbrk(s, "%+"))) {
		/* Parse %xx */
		if (*s == '%') {
			sscanf(s + 1, "%02x", &c);
			*s++ = (char) c;
			strncpy(s, s + 2, strlen(s) + 1);
		}
		/* Space is special */
		else if (*s == '+')
			*s++ = ' ';
	}
}

char *
get_cgi(char *name)
{
	ENTRY e, *ep;

	if (!htab.table)
		return NULL;

	e.key = name;
	hsearch_r(e, FIND, &ep, &htab);

	return ep ? ep->data : NULL;
}

char *
get_cgi_json(char *name, json_object *root)
{
	if(root == NULL){
		ENTRY e, *ep;

	if (!htab.table)
		return NULL;

	e.key = name;
	hsearch_r(e, FIND, &ep, &htab);

	return ep ? ep->data : NULL;
	}else{
		struct json_object *json_value;
		json_value = json_object_object_get(root, name);
		return (char *)json_object_get_string(json_value);
	}
}

void
set_cgi(char *name, char *value)
{
	ENTRY e, *ep;

	if (!htab.table)
		return;

	e.key = name;
	hsearch_r(e, FIND, &ep, &htab);
	if (ep)
		ep->data = value;
	else {
		e.data = value;
		hsearch_r(e, ENTER, &ep, &htab);
	}
	assert(ep);
}

void
init_cgi(char *query)
{
	int len, nel;
	char *q, *name, *value;

	/* Clear variables */
	if (!query) {
		hdestroy_r(&htab);
		return;
	}

	/* Parse into individual assignments */
	q = query;
	len = strlen(query);
	nel = 1;
	while (strsep(&q, "&;"))
		nel++;
	hcreate_r(nel, &htab);

	for (q = query; q < (query + len);) {
		/* Unescape each assignment */
		unescape(name = value = q);

		/* Skip to next assignment */
		for (q += strlen(q); q < (query + len) && !*q; q++);

		/* Assign variable */
		name = strsep(&value, "=");
		if (value) {
//			printf("set_cgi: name=%s, value=%s.\n", name , value);	// N12 test
			set_cgi(name, value);
		}
	}
}

///////////vvvvvvvvvvvvvvvvvvv//////////////////Viz add 2010.08
char *webcgi_get(const char *name)
{
       ENTRY e, *ep;
 
       if (!htab.table) return NULL;
 
       e.key = (char *)name;
       hsearch_r(e, FIND, &ep, &htab);
 
//    cprintf("%s=%s\n", name, ep ? ep->data : "(null)");
 
       return ep ? ep->data : NULL;
}
 
void webcgi_set(char *name, char *value)
{
       ENTRY e, *ep;
 
       if (!htab.table) {
               hcreate_r(16, &htab);
       }
 
       e.key = name;
       hsearch_r(e, FIND, &ep, &htab);
       if (ep) {
               ep->data = value;
       }
       else {
               e.data = value;
               hsearch_r(e, ENTER, &ep, &htab);
       }
}

void webcgi_init(char *query)
{
       int nel;
       char *q, *end, *name, *value;
 
       if (htab.table) hdestroy_r(&htab);
       if (query == NULL) return;
 
//    cprintf("query = %s\n", query);
       
       end = query + strlen(query);
       q = query;
       nel = 1;
       while (strsep(&q, "&;")) {
               nel++;
       }
       hcreate_r(nel, &htab);
 
       for (q = query; q < end; ) {
               value = q;
               q += strlen(q) + 1;
 
               unescape(value);
               name = strsep(&value, "=");
               if (value) webcgi_set(name, value);
       }
}

FILE *connfp = NULL;
int web_read(void *buffer, int len)
{
       int r;
       if (len <= 0) return 0;
       while ((r = fread(buffer, 1, len, connfp)) == 0) {
               if (errno != EINTR) return -1;
       }
       return r;
}
/*
int web_read_x(void *buffer, int len)
{
       int n;
       int t = 0;
       while (len > 0) {
               n = web_read(buffer, len);
               if (n <= 0) return len;
               (unsigned char *)buffer += n;
               len -= n;
               t += n;
       }
       return t;
}
*/
////////^^^^^^^^^^^^^^^^^^^^^^^////////////Viz add 2010.08
