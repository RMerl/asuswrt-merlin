/* 
   WebDAV 207 multi-status response handling
   Copyright (C) 1999-2006, Joe Orton <joe@manyfish.co.uk>

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

/* Generic handling for WebDAV 207 Multi-Status responses. */

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "ne_alloc.h"
#include "ne_utils.h"
#include "ne_xml.h"
#include "ne_207.h"
#include "ne_uri.h"
#include "ne_basic.h"

#include "ne_internal.h"

struct ne_207_parser_s {
    ne_207_start_response *start_response;
    ne_207_end_response *end_response;
    ne_207_start_propstat *start_propstat;
    ne_207_end_propstat *end_propstat;
    ne_xml_parser *parser;
    void *userdata;

    ne_uri base;

    ne_buffer *cdata;

    /* remember whether we are in a response: the validation
     * doesn't encapsulate this since we only count as being 
     * "in a response" when we've seen the href element. */
    int in_response;

    /* current position */
    void *response, *propstat;
    /* caching */
    ne_status status;
    char *description, *href;
};

#define ELM_multistatus 1
#define ELM_response 2
#define ELM_responsedescription 3
#define ELM_href 4
#define ELM_prop (NE_207_STATE_PROP)
#define ELM_status 6
#define ELM_propstat 7

static const struct ne_xml_idmap map207[] = {
    { "DAV:", "multistatus", ELM_multistatus },
    { "DAV:", "response", ELM_response },
    { "DAV:", "responsedescription", ELM_responsedescription },
    { "DAV:", "href", ELM_href },
    { "DAV:", "propstat", ELM_propstat },
    { "DAV:", "prop",  ELM_prop },
    { "DAV:", "status", ELM_status }
};

/* Set the callbacks for the parser */
void ne_207_set_response_handlers(ne_207_parser *p,
                                  ne_207_start_response *start,
                                  ne_207_end_response *end)
{
    p->start_response = start;
    p->end_response = end;
}

void ne_207_set_propstat_handlers(ne_207_parser *p,
				   ne_207_start_propstat *start,
				   ne_207_end_propstat *end)
{
    p->start_propstat = start;
    p->end_propstat = end;
}

void *ne_207_get_current_response(ne_207_parser *p)
{
    return p->response;
}

void *ne_207_get_current_propstat(ne_207_parser *p)
{
    return p->propstat;
}

/* return non-zero if (child, parent) is an interesting element */
static int can_handle(int parent, int child) 
{
    return (parent == 0 && child == ELM_multistatus) ||
        (parent == ELM_multistatus && child == ELM_response) ||
        (parent == ELM_response && 
         (child == ELM_href || child == ELM_status || 
          child == ELM_propstat || child == ELM_responsedescription)) ||
        (parent == ELM_propstat &&
         (child == ELM_prop || child == ELM_status ||
          child == ELM_responsedescription));
}

static int cdata_207(void *userdata, int state, const char *buf, size_t len)
{
    ne_207_parser *p = userdata;

    if ((state == ELM_href || state == ELM_responsedescription ||
         state == ELM_status) && p->cdata->used + len < 2048)
        ne_buffer_append(p->cdata, buf, len);

    return 0;
}

static int start_element(void *userdata, int parent, 
                         const char *nspace, const char *name, 
                         const char **atts) 
{
    ne_207_parser *p = userdata;
    int state = ne_xml_mapid(map207, NE_XML_MAPLEN(map207), nspace, name);

    if (!can_handle(parent, state))
        return NE_XML_DECLINE;

    /* if not in a response, ignore everything. */
    if (!p->in_response && state != ELM_response && state != ELM_multistatus &&
        state != ELM_href)
        return NE_XML_DECLINE;

    if (state == ELM_propstat && p->start_propstat) {
        p->propstat = p->start_propstat(p->userdata, p->response);
        if (p->propstat == NULL) {
            return NE_XML_ABORT;
        }
    }

    ne_buffer_clear(p->cdata);

    return state;
}

#define GIVE_STATUS(p) ((p)->status.reason_phrase?&(p)->status:NULL)

#define HAVE_CDATA(p) ((p)->cdata->used > 1)

static int 
end_element(void *userdata, int state, const char *nspace, const char *name)
{
    ne_207_parser *p = userdata;
    const char *cdata = ne_shave(p->cdata->data, "\r\n\t ");

    switch (state) {
    case ELM_responsedescription:
	if (HAVE_CDATA(p)) {
            if (p->description) ne_free(p->description);
	    p->description = ne_strdup(cdata);
	}
	break;
    case ELM_href:
	/* Now we have the href, begin the response */
	if (p->start_response && HAVE_CDATA(p)) {
            ne_uri ref, resolved;

            if (ne_uri_parse(cdata, &ref) == 0) {
                ne_uri_resolve(&p->base, &ref, &resolved);

                p->response = p->start_response(p->userdata, &resolved);
                p->in_response = 1;
                ne_uri_free(&resolved);
            }
            ne_uri_free(&ref);
	}
	break;
    case ELM_status:
	if (HAVE_CDATA(p)) {
            if (p->status.reason_phrase) ne_free(p->status.reason_phrase);
	    if (ne_parse_statusline(cdata, &p->status)) {
		char buf[500];
		NE_DEBUG(NE_DBG_HTTP, "Status line: %s\n", cdata);
		ne_snprintf(buf, 500, 
			    _("Invalid HTTP status line in status element "
                              "at line %d of response:\nStatus line was: %s"),
			    ne_xml_currentline(p->parser), cdata);
		ne_xml_set_error(p->parser, buf);
		return -1;
	    } else {
		NE_DEBUG(NE_DBG_XML, "Decoded status line: %s\n", cdata);
	    }
	}
	break;
    case ELM_propstat:
	if (p->end_propstat)
	    p->end_propstat(p->userdata, p->propstat, GIVE_STATUS(p),
			    p->description);
	p->propstat = NULL;
        if (p->description) ne_free(p->description);
        if (p->status.reason_phrase) ne_free(p->status.reason_phrase);
        p->description = p->status.reason_phrase = NULL;
	break;
    case ELM_response:
        if (!p->in_response) break;
	if (p->end_response)
	    p->end_response(p->userdata, p->response, GIVE_STATUS(p),
			    p->description);
	p->response = NULL;
	p->in_response = 0;
        if (p->description) ne_free(p->description);
        if (p->status.reason_phrase) ne_free(p->status.reason_phrase);
        p->description = p->status.reason_phrase = NULL;
	break;
    }
    return 0;
}

ne_207_parser *ne_207_create(ne_xml_parser *parser, const ne_uri *base, 
                             void *userdata)
{
    ne_207_parser *p = ne_calloc(sizeof *p);

    p->parser = parser;
    p->userdata = userdata;
    p->cdata = ne_buffer_create();

    ne_uri_copy(&p->base, base);

    /* Add handler for the standard 207 elements */
    ne_xml_push_handler(parser, start_element, cdata_207, end_element, p);
    
    return p;
}

void ne_207_destroy(ne_207_parser *p) 
{
    if (p->status.reason_phrase) ne_free(p->status.reason_phrase);
    ne_buffer_destroy(p->cdata);
    ne_uri_free(&p->base);
    ne_free(p);
}

int ne_accept_207(void *userdata, ne_request *req, const ne_status *status)
{
    return (status->code == 207);
}

/* Handling of 207 errors: we keep a string buffer, and append
 * messages to it as they come down.
 *
 * Note, 424 means it would have worked but something else went wrong.
 * We will have had the error for "something else", so we display
 * that, and skip 424 errors. */

/* This is passed as userdata to the 207 code. */
struct context {
    char *href;
    ne_buffer *buf;
    unsigned int is_error;
};

static void *start_response(void *userdata, const ne_uri *uri)
{
    struct context *ctx = userdata;
    if (ctx->href) ne_free(ctx->href);
    ctx->href = ne_uri_unparse(uri);
    return NULL;
}

static void handle_error(struct context *ctx, const ne_status *status,
			 const char *description)
{
    if (status && status->klass != 2 && status->code != 424) {
	char buf[50];
	ctx->is_error = 1;
	sprintf(buf, "%d", status->code);
	ne_buffer_concat(ctx->buf, ctx->href, ": ", 
			 buf, " ", status->reason_phrase, "\n", NULL);
	if (description != NULL) {
	    /* TODO: these can be multi-line. Would be good to
	     * word-wrap this at col 80. */
	    ne_buffer_concat(ctx->buf, " -> ", description, "\n", NULL);
	}
    }

}

static void end_response(void *userdata, void *response,
			 const ne_status *status, const char *description)
{
    struct context *ctx = userdata;
    handle_error(ctx, status, description);
}

static void 
end_propstat(void *userdata, void *propstat,
	     const ne_status *status, const char *description)
{
    struct context *ctx = userdata;
    handle_error(ctx, status, description);
}

/* Dispatch a DAV request and handle a 207 error response appropriately */
/* TODO: hook up Content-Type parsing; passing charset to XML parser */
int ne_simple_request(ne_session *sess, ne_request *req)
{
    int ret;
    struct context ctx = {0};
    ne_207_parser *p207;
    ne_xml_parser *p = ne_xml_create();
    ne_uri base = {0};

    /* Mock up a base URI; it should really be retrieved from the
     * request object. */
    ne_fill_server_uri(sess, &base);
    base.path = ne_strdup("/");
    p207 = ne_207_create(p, &base, &ctx);
    ne_uri_free(&base);    

    /* The error string is progressively written into the
     * ne_buffer by the element callbacks */
    ctx.buf = ne_buffer_create();

    ne_207_set_response_handlers(p207, start_response, end_response);
    ne_207_set_propstat_handlers(p207, NULL, end_propstat);
    
    ne_add_response_body_reader(req, ne_accept_207, ne_xml_parse_v, p);

    ret = ne_request_dispatch(req);

    if (ret == NE_OK) {
	if (ne_get_status(req)->code == 207) {
	    if (ne_xml_failed(p)) { 
		/* The parse was invalid */
		ne_set_error(sess, "%s", ne_xml_get_error(p));
		ret = NE_ERROR;
	    } else if (ctx.is_error) {
		/* If we've actually got any error information
		 * from the 207, then set that as the error */
		ne_set_error(sess, "%s", ctx.buf->data);
		ret = NE_ERROR;
	    }
	} else if (ne_get_status(req)->klass != 2) {
	    ret = NE_ERROR;
	}
    }

    ne_207_destroy(p207);
    ne_xml_destroy(p);
    ne_buffer_destroy(ctx.buf);
    if (ctx.href) ne_free(ctx.href);

    ne_request_destroy(req);

    return ret;
}
    
