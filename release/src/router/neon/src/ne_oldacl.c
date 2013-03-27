/*
   Access control
   Copyright (C) 2001-2006, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

/* Contributed by Arun Garg <arung@pspl.co.in> */

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "ne_request.h"
#include "ne_locks.h"
#include "ne_alloc.h"
#include "ne_string.h"
#include "ne_acl.h"
#include "ne_uri.h"
#include "ne_xml.h" /* for NE_XML_MEDIA_TYPE */

#define EOL "\r\n"

static ne_buffer *acl_body(const ne_acl_entry *right, int count)
{
    ne_buffer *body = ne_buffer_create();
    int m;

    ne_buffer_zappend(body,
		      "<?xml version=\"1.0\" encoding=\"utf-8\"?>" EOL
		      "<acl xmlns='DAV:'>" EOL);

    for (m = 0; m < count; m++) {
	const char *type;

	type = (right[m].type == ne_acl_grant ? "grant" : "deny");

	ne_buffer_concat(body, "<ace>" EOL "<principal>", NULL);

	switch (right[m].apply) {
	case ne_acl_all:
	    ne_buffer_zappend(body, "<all/>" EOL);
	    break;
	case ne_acl_property:
	    ne_buffer_concat(body, "<property><", right[m].principal,
			     "/></property>" EOL, NULL);
	    break;
	case ne_acl_href:
	    ne_buffer_concat(body, "<href>", right[m].principal,
			     "</href>" EOL, NULL);
	    break;
	}

	ne_buffer_concat(body, "</principal>" EOL "<", type, ">" EOL, NULL);
	
	if (right[m].read == 0)
	    ne_buffer_concat(body,
			     "<privilege>" "<read/>" "</privilege>" EOL,
			     NULL);
	if (right[m].read_acl == 0)
	    ne_buffer_concat(body,
			     "<privilege>" "<read-acl/>" "</privilege>" EOL,
			     NULL);
	if (right[m].write == 0)
	    ne_buffer_concat(body,
			     "<privilege>" "<write/>" "</privilege>" EOL,
			     NULL);
	if (right[m].write_acl == 0)
	    ne_buffer_concat(body,
			     "<privilege>" "<write-acl/>" "</privilege>" EOL,
			     NULL);
	if (right[m].read_cuprivset == 0)
	    ne_buffer_concat(body,
			     "<privilege>"
			     "<read-current-user-privilege-set/>"
			     "</privilege>" EOL, NULL);
	ne_buffer_concat(body, "</", type, ">" EOL, NULL);
	ne_buffer_zappend(body, "</ace>" EOL);
    }
    ne_buffer_zappend(body, "</acl>" EOL);

    return body;
}

int ne_acl_set(ne_session *sess, const char *uri,
	       const ne_acl_entry *entries, int numentries)
{
    int ret;
    ne_request *req = ne_request_create(sess, "ACL", uri);
    ne_buffer *body = acl_body(entries, numentries);

#ifdef NE_HAVE_DAV
    ne_lock_using_resource(req, uri, 0);
#endif

    ne_set_request_body_buffer(req, body->data, ne_buffer_size(body));
    ne_add_request_header(req, "Content-Type", NE_XML_MEDIA_TYPE);
    ret = ne_request_dispatch(req);

    ne_buffer_destroy(body);

    if (ret == NE_OK && ne_get_status(req)->code == 207) {
	ret = NE_ERROR;
    }

    ne_request_destroy(req);
    return ret;
}
