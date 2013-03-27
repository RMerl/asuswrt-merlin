/* 
   WebDAV property manipulation
   Copyright (C) 2000-2008, Joe Orton <joe@manyfish.co.uk>

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

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "ne_alloc.h"
#include "ne_xml.h"
#include "ne_props.h"
#include "ne_basic.h"
#include "ne_locks.h"
#include "ne_internal.h"

/* don't store flat props with a value > 10K */
#define MAX_FLATPROP_LEN (102400)

struct ne_propfind_handler_s {
    ne_session *sess;
    ne_request *request;

    int has_props; /* whether we've already written some
		    * props to the body. */
    ne_buffer *body;
    
    ne_207_parser *parser207;
    ne_xml_parser *parser;

    /* Creator/destructor callbacks. */
    ne_props_create_complex creator;
    ne_props_destroy_complex destructor;
    void *cd_userdata;
    
    /* Current propset, or NULL if none being processed. */
    ne_prop_result_set *current;

    ne_buffer *value; /* current flat property value */
    int depth; /* nesting depth within a flat property */

    ne_props_result callback;
    void *userdata;
};

#define ELM_flatprop (NE_207_STATE_TOP - 1)

/* We build up the results of one 'response' element in memory. */
struct prop {
    char *name, *nspace, *value, *lang;
    /* Store a ne_propname here too, for convienience.  pname.name =
     * name, pname.nspace = nspace, but they are const'ed in pname. */
    ne_propname pname;
};

#define NSPACE(x) ((x) ? (x) : "")

struct propstat {
    struct prop *props;
    int numprops;
    ne_status status;
};

/* Results set. */
struct ne_prop_result_set_s {
    struct propstat *pstats;
    int numpstats, counter;
    void *private;
    ne_uri uri;
};

#define MAX_PROP_COUNTER (1024)

static int 
startelm(void *userdata, int state, const char *name, const char *nspace,
	 const char **atts);
static int 
endelm(void *userdata, int state, const char *name, const char *nspace);

/* Handle character data; flat property value. */
static int chardata(void *userdata, int state, const char *data, size_t len)
{
    ne_propfind_handler *hdl = userdata;

    if (state == ELM_flatprop && hdl->value->length < MAX_FLATPROP_LEN)
        ne_buffer_append(hdl->value, data, len);

    return 0;
}

ne_xml_parser *ne_propfind_get_parser(ne_propfind_handler *handler)
{
    return handler->parser;
}

ne_request *ne_propfind_get_request(ne_propfind_handler *handler)
{
    return handler->request;
}

static int propfind(ne_propfind_handler *handler, 
		    ne_props_result results, void *userdata)
{
    int ret;
    ne_request *req = handler->request;

    /* Register the flat property handler to catch any properties 
     * which the user isn't handling as 'complex'. */
    ne_xml_push_handler(handler->parser, startelm, chardata, endelm, handler);

    handler->callback = results;
    handler->userdata = userdata;

    ne_set_request_body_buffer(req, handler->body->data,
			       ne_buffer_size(handler->body));

    ne_add_request_header(req, "Content-Type", NE_XML_MEDIA_TYPE);
    
    ne_add_response_body_reader(req, ne_accept_207, ne_xml_parse_v, 
				  handler->parser);

    ret = ne_request_dispatch(req);

    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    } else if (ne_xml_failed(handler->parser)) {
	ne_set_error(handler->sess, "%s", ne_xml_get_error(handler->parser));
	ret = NE_ERROR;
    }

    return ret;
}

static void set_body(ne_propfind_handler *hdl, const ne_propname *names)
{
    ne_buffer *body = hdl->body;
    int n;
    
    if (!hdl->has_props) {
	ne_buffer_czappend(body, "<prop>\n");
	hdl->has_props = 1;
    }

    for (n = 0; names[n].name != NULL; n++) {
	ne_buffer_concat(body, "<", names[n].name, " xmlns=\"", 
			 NSPACE(names[n].nspace), "\"/>\n", NULL);
    }

}

int ne_propfind_allprop(ne_propfind_handler *handler, 
			 ne_props_result results, void *userdata)
{
    ne_buffer_czappend(handler->body, "<allprop/></propfind>\n");
    return propfind(handler, results, userdata);
}

int ne_propfind_named(ne_propfind_handler *handler, const ne_propname *props,
		       ne_props_result results, void *userdata)
{
    set_body(handler, props);
    ne_buffer_czappend(handler->body, "</prop></propfind>\n");
    return propfind(handler, results, userdata);
}


/* The easy one... PROPPATCH */
int ne_proppatch(ne_session *sess, const char *uri, 
		 const ne_proppatch_operation *items)
{
    ne_request *req = ne_request_create(sess, "PROPPATCH", uri);
    ne_buffer *body = ne_buffer_create();
    int n, ret;
    
    /* Create the request body */
    ne_buffer_czappend(body, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                       "<D:propertyupdate xmlns:D=\"DAV:\">");

    for (n = 0; items[n].name != NULL; n++) {
	const char *elm = (items[n].type == ne_propset) ? "set" : "remove";

	/* <set><prop><prop-name>value</prop-name></prop></set> */
	ne_buffer_concat(body, "<D:", elm, "><D:prop>"
			 "<", items[n].name->name, NULL);
	
	if (items[n].name->nspace) {
	    ne_buffer_concat(body, " xmlns=\"", items[n].name->nspace, "\"", NULL);
	}

	if (items[n].type == ne_propset) {
	    ne_buffer_concat(body, ">", items[n].value, NULL);
	} else {
	    ne_buffer_append(body, ">", 1);
	}

	ne_buffer_concat(body, "</", items[n].name->name, "></D:prop></D:", elm, 
                         ">\n", NULL);
    }	

    ne_buffer_czappend(body, "</D:propertyupdate>\n");

    ne_set_request_body_buffer(req, body->data, ne_buffer_size(body));
    ne_add_request_header(req, "Content-Type", NE_XML_MEDIA_TYPE);
    
#ifdef NE_HAVE_DAV
    ne_lock_using_resource(req, uri, NE_DEPTH_ZERO);
#endif

    ret = ne_simple_request(sess, req);
    
    ne_buffer_destroy(body);

    return ret;
}

/* Compare two property names. */
static int pnamecmp(const ne_propname *pn1, const ne_propname *pn2)
{
    if (pn1->nspace == NULL && pn2->nspace != NULL) {
	return 1;
    } else if (pn1->nspace != NULL && pn2->nspace == NULL) {
	return -1;
    } else if (pn1->nspace == NULL) {
	return strcmp(pn1->name, pn2->name);
    } else {
	return (strcmp(pn1->nspace, pn2->nspace) ||
		strcmp(pn1->name, pn2->name));
    }
}

/* Find property in 'set' with name 'pname'.  If found, set pstat_ret
 * to the containing propstat, likewise prop_ret, and returns zero.
 * If not found, returns non-zero.  */
static int findprop(const ne_prop_result_set *set, const ne_propname *pname,
		    struct propstat **pstat_ret, struct prop **prop_ret)
{
    
    int ps, p;

    for (ps = 0; ps < set->numpstats; ps++) {
	for (p = 0; p < set->pstats[ps].numprops; p++) {
	    struct prop *prop = &set->pstats[ps].props[p];

	    if (pnamecmp(&prop->pname, pname) == 0) {
		if (pstat_ret != NULL)
		    *pstat_ret = &set->pstats[ps];
		if (prop_ret != NULL)
		    *prop_ret = prop;
		return 0;
	    }
	}
    }

    return -1;
}

const char *ne_propset_value(const ne_prop_result_set *set,
			      const ne_propname *pname)
{
    struct prop *prop;
    
    if (findprop(set, pname, NULL, &prop)) {
	return NULL;
    } else {
	return prop->value;
    }
}

const char *ne_propset_lang(const ne_prop_result_set *set,
			     const ne_propname *pname)
{
    struct prop *prop;

    if (findprop(set, pname, NULL, &prop)) {
	return NULL;
    } else {
	return prop->lang;
    }
}

void *ne_propfind_current_private(ne_propfind_handler *handler)
{
    return handler->current ? handler->current->private : NULL;
}

void *ne_propset_private(const ne_prop_result_set *set)
{
    return set->private;
}

int ne_propset_iterate(const ne_prop_result_set *set,
			ne_propset_iterator iterator, void *userdata)
{
    int ps, p;

    for (ps = 0; ps < set->numpstats; ps++) {
	for (p = 0; p < set->pstats[ps].numprops; p++) {
	    struct prop *prop = &set->pstats[ps].props[p];
	    int ret = iterator(userdata, &prop->pname, prop->value, 
			       &set->pstats[ps].status);
	    if (ret)
		return ret;

	}
    }

    return 0;
}

const ne_status *ne_propset_status(const ne_prop_result_set *set,
				      const ne_propname *pname)
{
    struct propstat *pstat;
    
    if (findprop(set, pname, &pstat, NULL)) {
	/* TODO: it is tempting to return a dummy status object here
	 * rather than NULL, which says "Property result was not given
	 * by server."  but I'm not sure if this is best left to the
	 * client.  */
	return NULL;
    } else {
	return &pstat->status;
    }
}

static void *start_response(void *userdata, const ne_uri *uri)
{
    ne_prop_result_set *set = ne_calloc(sizeof(*set));
    ne_propfind_handler *hdl = userdata;

    ne_uri_copy(&set->uri, uri);

    if (hdl->creator) {
	set->private = hdl->creator(hdl->cd_userdata, &set->uri);
    }

    hdl->current = set;

    return set;
}

static void *start_propstat(void *userdata, void *response)
{
    ne_prop_result_set *set = response;
    ne_propfind_handler *hdl = userdata;
    struct propstat *pstat;
    int n;

    if (++hdl->current->counter == MAX_PROP_COUNTER) {
        ne_xml_set_error(hdl->parser, _("Response exceeds maximum property count"));
        return NULL;
    }
    
    n = set->numpstats;
    set->pstats = ne_realloc(set->pstats, sizeof(struct propstat) * (n+1));
    set->numpstats = n+1;

    pstat = &set->pstats[n];
    memset(pstat, 0, sizeof(*pstat));
    
    /* And return this as the new pstat. */
    return &set->pstats[n];
}

static int startelm(void *userdata, int parent,
                    const char *nspace, const char *name, const char **atts)
{
    ne_propfind_handler *hdl = userdata;
    struct propstat *pstat = ne_207_get_current_propstat(hdl->parser207);
    struct prop *prop;
    int n;
    const char *lang;

    /* Just handle all children of propstat and their descendants. */
    if ((parent != NE_207_STATE_PROP && parent != ELM_flatprop) 
        || pstat == NULL)
        return NE_XML_DECLINE;

    if (parent == ELM_flatprop) {
        /* collecting the flatprop value. */
        hdl->depth++;
        if (hdl->value->used < MAX_FLATPROP_LEN) {
            const char **a = atts;

            ne_buffer_concat(hdl->value, "<", nspace, name, NULL);
            
            while (a[0] && hdl->value->used < MAX_FLATPROP_LEN) {
                const char *nsep = strchr(a[0], ':'), *pfx;

                /* Resolve the attribute namespace prefix, if any.
                 * Ignore a failure to resolve the namespace prefix. */
                pfx = nsep ? ne_xml_resolve_nspace(hdl->parser,
                                                   a[0], nsep - a[0]) : NULL;
                
                if (pfx) {
                    ne_buffer_concat(hdl->value, " ", pfx, nsep + 1, "='", 
                                     a[1], "'", NULL);
                }
                else {
                    ne_buffer_concat(hdl->value, " ", a[0], "='", a[1], "'", NULL);
                }
                a += 2;
            }

            ne_buffer_czappend(hdl->value, ">");
        }

        return ELM_flatprop;
    }        

    /* Enforce maximum number of properties per resource to prevent a
     * memory exhaustion attack by a hostile server. */
    if (++hdl->current->counter == MAX_PROP_COUNTER) {
        ne_xml_set_error(hdl->parser, _("Response exceeds maximum property count"));
        return NE_XML_ABORT;
    }

    /* Add a property to this propstat */
    n = pstat->numprops;

    pstat->props = ne_realloc(pstat->props, sizeof(struct prop) * (n + 1));
    pstat->numprops = n+1;

    /* Fill in the new property. */
    prop = &pstat->props[n];

    prop->pname.name = prop->name = ne_strdup(name);
    if (nspace[0] == '\0') {
	prop->pname.nspace = prop->nspace = NULL;
    } else {
	prop->pname.nspace = prop->nspace = ne_strdup(nspace);
    }
    prop->value = NULL;

    NE_DEBUG(NE_DBG_XML, "Got property #%d: {%s}%s.\n", n, 
	     NSPACE(prop->nspace), prop->name);

    /* This is under discussion at time of writing (April '01), and it
     * looks like we need to retrieve the xml:lang property from any
     * element here or above.
     *
     * Also, I think we might need attribute namespace handling here.  */
    lang = ne_xml_get_attr(hdl->parser, atts, NULL, "xml:lang");
    if (lang != NULL) {
	prop->lang = ne_strdup(lang);
	NE_DEBUG(NE_DBG_XML, "Property language is %s\n", prop->lang);
    } else {
	prop->lang = NULL;
    }

    hdl->depth = 0;

    return ELM_flatprop;
}

static int endelm(void *userdata, int state,
                  const char *nspace, const char *name)
{
    ne_propfind_handler *hdl = userdata;
    struct propstat *pstat = ne_207_get_current_propstat(hdl->parser207);
    int n;

    if (hdl->depth > 0) {
        /* nested. */
        if (hdl->value->used < MAX_FLATPROP_LEN)
            ne_buffer_concat(hdl->value, "</", nspace, name, ">", NULL);
        hdl->depth--;
    } else {
        /* end of the current property value */
        n = pstat->numprops - 1;
        pstat->props[n].value = ne_buffer_finish(hdl->value);
        hdl->value = ne_buffer_create();
    }
    return 0;
}

static void end_propstat(void *userdata, void *pstat_v, 
			 const ne_status *status,
			 const char *description)
{
    struct propstat *pstat = pstat_v;

    /* Nothing to do if no status was given. */
    if (!status) return;

    /* If we get a non-2xx response back here, we wipe the value for
     * each of the properties in this propstat, so the caller knows to
     * look at the status instead. It's annoying, since for each prop
     * we will have done an unnecessary strdup("") above, but there is
     * no easy way round that given the fact that we don't know
     * whether we've got an error or not till after we get the
     * property element.
     *
     * Interestingly IIS breaks the 2518 DTD and puts the status
     * element first in the propstat. This is useful since then we
     * *do* know whether each subsequent empty prop element means, but
     * we can't rely on that here. */
    if (status->klass != 2) {
	int n;
	
	for (n = 0; n < pstat->numprops; n++) {
	    ne_free(pstat->props[n].value);
	    pstat->props[n].value = NULL;
	}
    }

    /* copy the status structure, and dup the reason phrase. */
    pstat->status = *status;
    pstat->status.reason_phrase = ne_strdup(status->reason_phrase);
}

/* Frees up a results set */
static void free_propset(ne_propfind_handler *handler,
                         ne_prop_result_set *set)
{
    int n;
    
    if (handler->destructor && set->private) {
        handler->destructor(handler->cd_userdata, set->private);
    }

    for (n = 0; n < set->numpstats; n++) {
	int m;
	struct propstat *p = &set->pstats[n];

	for (m = 0; m < p->numprops; m++) {
            if (p->props[m].nspace) ne_free(p->props[m].nspace);
            ne_free(p->props[m].name);
            if (p->props[m].lang) ne_free(p->props[m].lang);
            if (p->props[m].value) ne_free(p->props[m].value);
            p->props[m].nspace = p->props[m].lang = 
                p->props[m].value = NULL;
	}

	if (p->status.reason_phrase)
	    ne_free(p->status.reason_phrase);
	if (p->props)
	    ne_free(p->props);
    }

    if (set->pstats)
	ne_free(set->pstats);
    ne_uri_free(&set->uri);
    ne_free(set);
}

static void end_response(void *userdata, void *resource,
			 const ne_status *status,
			 const char *description)
{
    ne_propfind_handler *handler = userdata;
    ne_prop_result_set *set = resource;

    /* Pass back the results for this resource. */
    if (handler->callback && set->numpstats > 0)
	handler->callback(handler->userdata, &set->uri, set);

    /* Clean up the propset tree we've just built. */
    free_propset(handler, set);
    handler->current = NULL;
}

ne_propfind_handler *
ne_propfind_create(ne_session *sess, const char *uri, int depth)
{
    ne_propfind_handler *ret = ne_calloc(sizeof(ne_propfind_handler));
    ne_uri base = {0};

    ne_fill_server_uri(sess, &base);
    base.path = ne_strdup(uri);

    ret->parser = ne_xml_create();
    ret->parser207 = ne_207_create(ret->parser, &base, ret);
    ret->sess = sess;
    ret->body = ne_buffer_create();
    ret->request = ne_request_create(sess, "PROPFIND", uri);
    ret->value = ne_buffer_create();

    ne_add_depth_header(ret->request, depth);

    ne_207_set_response_handlers(ret->parser207, 
				  start_response, end_response);

    ne_207_set_propstat_handlers(ret->parser207, start_propstat,
				  end_propstat);

    /* The start of the request body is fixed: */
    ne_buffer_czappend(ret->body, 
                       "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" 
                       "<propfind xmlns=\"DAV:\">");

    ne_uri_free(&base);

    return ret;
}

/* Destroy a propfind handler */
void ne_propfind_destroy(ne_propfind_handler *handler)
{
    ne_buffer_destroy(handler->value);
    if (handler->current)
        free_propset(handler, handler->current);
    ne_207_destroy(handler->parser207);
    ne_xml_destroy(handler->parser);
    ne_buffer_destroy(handler->body);
    ne_request_destroy(handler->request);
    ne_free(handler);    
}

int ne_simple_propfind(ne_session *sess, const char *href, int depth,
			const ne_propname *props,
			ne_props_result results, void *userdata)
{
    ne_propfind_handler *hdl;
    int ret;

    hdl = ne_propfind_create(sess, href, depth);
    if (props != NULL) {
	ret = ne_propfind_named(hdl, props, results, userdata);
    } else {
	ret = ne_propfind_allprop(hdl, results, userdata);
    }
	
    ne_propfind_destroy(hdl);
    
    return ret;
}

int ne_propnames(ne_session *sess, const char *href, int depth,
		  ne_props_result results, void *userdata)
{
    ne_propfind_handler *hdl;
    int ret;

    hdl = ne_propfind_create(sess, href, depth);

    ne_buffer_czappend(hdl->body, "<propname/></propfind>");

    ret = propfind(hdl, results, userdata);

    ne_propfind_destroy(hdl);

    return ret;
}

void ne_propfind_set_private(ne_propfind_handler *hdl,
                             ne_props_create_complex creator,
                             ne_props_destroy_complex destructor,
                             void *userdata)
{
    hdl->creator = creator;
    hdl->destructor = destructor;
    hdl->cd_userdata = userdata;
}
