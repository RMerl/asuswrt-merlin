/* 
   WebDAV Class 2 locking operations
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

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <ctype.h> /* for isdigit() */

#include "ne_alloc.h"

#include "ne_request.h"
#include "ne_xml.h"
#include "ne_locks.h"
#include "ne_uri.h"
#include "ne_basic.h"
#include "ne_props.h"
#include "ne_207.h"
#include "ne_internal.h"
#include "ne_xmlreq.h"

#define HOOK_ID "http://webdav.org/neon/hooks/webdav-locking"

/* A list of lock objects. */
struct lock_list {
    struct ne_lock *lock;
    struct lock_list *next, *prev;
};

struct ne_lock_store_s {
    struct lock_list *locks;
    struct lock_list *cursor; /* current position in 'locks' */
};

struct lh_req_cookie {
    const ne_lock_store *store;
    struct lock_list *submit;
};

/* Context for PROPFIND/lockdiscovery callbacks */
struct discover_ctx {
    ne_propfind_handler *phandler;
    ne_lock_result results;
    void *userdata;
    ne_buffer *cdata;
};

/* Context for handling LOCK response */
struct lock_ctx {
    struct ne_lock active; /* activelock */
    ne_request *req; /* the request in question */
    ne_xml_parser *parser;
    char *token; /* the token we're after. */
    int found;
    ne_buffer *cdata;
};

/* use the "application" state space. */
#define ELM_LOCK_FIRST (NE_PROPS_STATE_TOP + 66)

#define ELM_lockdiscovery (ELM_LOCK_FIRST)
#define ELM_activelock (ELM_LOCK_FIRST + 1)
#define ELM_lockscope (ELM_LOCK_FIRST + 2)
#define ELM_locktype (ELM_LOCK_FIRST + 3)
#define ELM_depth (ELM_LOCK_FIRST + 4)
#define ELM_owner (ELM_LOCK_FIRST + 5)
#define ELM_timeout (ELM_LOCK_FIRST + 6)
#define ELM_locktoken (ELM_LOCK_FIRST + 7)
#define ELM_lockinfo (ELM_LOCK_FIRST + 8)
#define ELM_write (ELM_LOCK_FIRST + 9)
#define ELM_exclusive (ELM_LOCK_FIRST + 10)
#define ELM_shared (ELM_LOCK_FIRST + 11)
#define ELM_href (ELM_LOCK_FIRST + 12)
#define ELM_prop (NE_207_STATE_PROP)

static const struct ne_xml_idmap element_map[] = {
#define ELM(x) { "DAV:", #x, ELM_ ## x }
    ELM(lockdiscovery), ELM(activelock), ELM(prop), ELM(lockscope),
    ELM(locktype), ELM(depth), ELM(owner), ELM(timeout), ELM(locktoken),
    ELM(lockinfo), ELM(lockscope), ELM(locktype), ELM(write), ELM(exclusive),
    ELM(shared), ELM(href)
    /* no "lockentry" */
#undef ELM
};

static const ne_propname lock_props[] = {
    { "DAV:", "lockdiscovery" },
    { NULL }
};

/* this simply registers the accessor for the function. */
static void lk_create(ne_request *req, void *session, 
		       const char *method, const char *uri)
{
    struct lh_req_cookie *lrc = ne_malloc(sizeof *lrc);
    lrc->store = session;
    lrc->submit = NULL;
    ne_set_request_private(req, HOOK_ID, lrc);
}

static void lk_pre_send(ne_request *r, void *userdata, ne_buffer *req)
{
    struct lh_req_cookie *lrc = ne_get_request_private(r, HOOK_ID);

    if (lrc->submit != NULL) {
	struct lock_list *item;

	/* Add in the If header */
	ne_buffer_czappend(req, "If:");
	for (item = lrc->submit; item != NULL; item = item->next) {
	    char *uri = ne_uri_unparse(&item->lock->uri);
	    ne_buffer_concat(req, " <", uri, "> (<",
			     item->lock->token, ">)", NULL);
	    ne_free(uri);
	}
	ne_buffer_czappend(req, "\r\n");
    }
}

/* Insert 'lock' into lock list *list. */
static void insert_lock(struct lock_list **list, struct ne_lock *lock)
{
    struct lock_list *item = ne_malloc(sizeof *item);
    if (*list != NULL) {
	(*list)->prev = item;
    }
    item->prev = NULL;
    item->next = *list;
    item->lock = lock;
    *list = item;
}

static void free_list(struct lock_list *list, int destroy)
{
    struct lock_list *next;

    while (list != NULL) {
	next = list->next;
	if (destroy)
	    ne_lock_destroy(list->lock);
	ne_free(list);
	list = next;
    }
}

static void lk_destroy(ne_request *req, void *userdata)
{
    struct lh_req_cookie *lrc = ne_get_request_private(req, HOOK_ID);
    free_list(lrc->submit, 0);
    ne_free(lrc);
}

void ne_lockstore_destroy(ne_lock_store *store)
{
    free_list(store->locks, 1);
    ne_free(store);
}

ne_lock_store *ne_lockstore_create(void)
{
    return ne_calloc(sizeof(ne_lock_store));
}

#define CURSOR_RET(s) ((s)->cursor?(s)->cursor->lock:NULL)

struct ne_lock *ne_lockstore_first(ne_lock_store *store)
{
    store->cursor = store->locks;
    return CURSOR_RET(store);
}

struct ne_lock *ne_lockstore_next(ne_lock_store *store)
{
    store->cursor = store->cursor->next;
    return CURSOR_RET(store);
}

void ne_lockstore_register(ne_lock_store *store, ne_session *sess)
{
    /* Register the hooks */
    ne_hook_create_request(sess, lk_create, store);
    ne_hook_pre_send(sess, lk_pre_send, store);
    ne_hook_destroy_request(sess, lk_destroy, store);
}

/* Submit the given lock for the given URI */
static void submit_lock(struct lh_req_cookie *lrc, struct ne_lock *lock)
{
    struct lock_list *item;

    /* Check for dups */
    for (item = lrc->submit; item != NULL; item = item->next) {
	if (ne_strcasecmp(item->lock->token, lock->token) == 0)
	    return;
    }

    insert_lock(&lrc->submit, lock);
}

struct ne_lock *ne_lockstore_findbyuri(ne_lock_store *store,
				       const ne_uri *uri)
{
    struct lock_list *cur;

    for (cur = store->locks; cur != NULL; cur = cur->next) {
	if (ne_uri_cmp(&cur->lock->uri, uri) == 0) {
	    return cur->lock;
	}
    }

    return NULL;
}

void ne_lock_using_parent(ne_request *req, const char *path)
{
    struct lh_req_cookie *lrc = ne_get_request_private(req, HOOK_ID);
    ne_uri u = {0};
    struct lock_list *item;
    char *parent;

    if (lrc == NULL)
	return;
    
    parent = ne_path_parent(path);
    if (parent == NULL)
	return;
    
    ne_fill_server_uri(ne_get_session(req), &u);

    for (item = lrc->store->locks; item != NULL; item = item->next) {

	/* Only care about locks which are on this server. */
	u.path = item->lock->uri.path;
	if (ne_uri_cmp(&u, &item->lock->uri))
	    continue;
	
	/* This lock is needed if it is an infinite depth lock which
	 * covers the parent, or a lock on the parent itself. */
	if ((item->lock->depth == NE_DEPTH_INFINITE && 
	     ne_path_childof(item->lock->uri.path, parent)) ||
	    ne_path_compare(item->lock->uri.path, parent) == 0) {
	    NE_DEBUG(NE_DBG_LOCKS, "Locked parent, %s on %s\n",
		     item->lock->token, item->lock->uri.path);
	    submit_lock(lrc, item->lock);
	}
    }

    u.path = parent; /* handy: makes u.path valid and ne_free(parent). */
    ne_uri_free(&u);
}

void ne_lock_using_resource(ne_request *req, const char *uri, int depth)
{
    struct lh_req_cookie *lrc = ne_get_request_private(req, HOOK_ID);
    struct lock_list *item;
    int match;

    if (lrc == NULL)
	return;	

    /* Iterate over the list of stored locks to see if any of them
     * apply to this resource */
    for (item = lrc->store->locks; item != NULL; item = item->next) {
	
	match = 0;
	
	if (depth == NE_DEPTH_INFINITE &&
	    ne_path_childof(uri, item->lock->uri.path)) {
	    /* Case 1: this is a depth-infinity request which will 
	     * modify a lock somewhere inside the collection. */
	    NE_DEBUG(NE_DBG_LOCKS, "Has child: %s\n", item->lock->token);
	    match = 1;
	} 
	else if (ne_path_compare(uri, item->lock->uri.path) == 0) {
	    /* Case 2: this request is directly on a locked resource */
	    NE_DEBUG(NE_DBG_LOCKS, "Has direct lock: %s\n", item->lock->token);
	    match = 1;
	}
	else if (item->lock->depth == NE_DEPTH_INFINITE && 
		 ne_path_childof(item->lock->uri.path, uri)) {
	    /* Case 3: there is a higher-up infinite-depth lock which
	     * covers the resource that this request will modify. */
	    NE_DEBUG(NE_DBG_LOCKS, "Is child of: %s\n", item->lock->token);
	    match = 1;
	}
	
	if (match) {
	    submit_lock(lrc, item->lock);
	}
    }

}

void ne_lockstore_add(ne_lock_store *store, struct ne_lock *lock)
{
    insert_lock(&store->locks, lock);
}

void ne_lockstore_remove(ne_lock_store *store, struct ne_lock *lock)
{
    struct lock_list *item;

    /* Find the lock */
    for (item = store->locks; item != NULL; item = item->next)
	if (item->lock == lock)
	    break;
    
    if (item->prev != NULL) {
	item->prev->next = item->next;
    } else {
	store->locks = item->next;
    }
    if (item->next != NULL) {
	item->next->prev = item->prev;
    }
    ne_free(item);
}

struct ne_lock *ne_lock_copy(const struct ne_lock *lock)
{
    struct ne_lock *ret = ne_calloc(sizeof *ret);

    ne_uri_copy(&ret->uri, &lock->uri);
    ret->token = ne_strdup(lock->token);
    ret->depth = lock->depth;
    ret->type = lock->type;
    ret->scope = lock->scope;
    if (lock->owner) ret->owner = ne_strdup(lock->owner);
    ret->timeout = lock->timeout;

    return ret;
}

struct ne_lock *ne_lock_create(void)
{
    struct ne_lock *lock = ne_calloc(sizeof *lock);
    lock->depth = NE_DEPTH_ZERO;
    lock->type = ne_locktype_write;
    lock->scope = ne_lockscope_exclusive;
    lock->timeout = NE_TIMEOUT_INVALID;
    return lock;
}

void ne_lock_free(struct ne_lock *lock)
{
    ne_uri_free(&lock->uri);
    if (lock->owner) {
        ne_free(lock->owner);
        lock->owner = NULL;
    }
    if (lock->token) {
        ne_free(lock->token);
        lock->token = NULL;
    }
}

void ne_lock_destroy(struct ne_lock *lock)
{
    ne_lock_free(lock);
    ne_free(lock);
}

int ne_unlock(ne_session *sess, const struct ne_lock *lock)
{
    ne_request *req = ne_request_create(sess, "UNLOCK", lock->uri.path);
    int ret;
    
    ne_print_request_header(req, "Lock-Token", "<%s>", lock->token);
    
    /* UNLOCK of a lock-null resource removes the resource from the
     * parent collection; so an UNLOCK may modify the parent
     * collection. (somewhat counter-intuitive, and not easily derived
     * from 2518.) */
    ne_lock_using_parent(req, lock->uri.path);

    ret = ne_request_dispatch(req);
    
    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    }

    ne_request_destroy(req);
    
    return ret;
}

static int parse_depth(const char *depth)
{
    if (ne_strcasecmp(depth, "infinity") == 0) {
	return NE_DEPTH_INFINITE;
    } else if (isdigit(depth[0])) {
	return atoi(depth);
    } else {
	return -1;
    }
}

static long parse_timeout(const char *timeout)
{
    if (ne_strcasecmp(timeout, "infinite") == 0) {
	return NE_TIMEOUT_INFINITE;
    } else if (strncasecmp(timeout, "Second-", 7) == 0) {
	long to = strtol(timeout+7, NULL, 10);
	if (to == LONG_MIN || to == LONG_MAX)
	    return NE_TIMEOUT_INVALID;
	return to;
    } else {
	return NE_TIMEOUT_INVALID;
    }
}

static void discover_results(void *userdata, const ne_uri *uri,
			     const ne_prop_result_set *set)
{
    struct discover_ctx *ctx = userdata;
    struct ne_lock *lock = ne_propset_private(set);
    const ne_status *status = ne_propset_status(set, &lock_props[0]);

    /* Require at least that the lock has a token. */
    if (lock->token) {
	if (status && status->klass != 2) {
	    ctx->results(ctx->userdata, NULL, uri, status);
	} else {
	    ctx->results(ctx->userdata, lock, uri, NULL);
	}
    }
    else if (status) {
	ctx->results(ctx->userdata, NULL, uri, status);
    }

    NE_DEBUG(NE_DBG_LOCKS, "End of response for %s\n", uri->path);
}

static int 
end_element_common(struct ne_lock *l, int state, const char *cdata)
{
    switch (state) { 
    case ELM_write:
	l->type = ne_locktype_write;
	break;
    case ELM_exclusive:
	l->scope = ne_lockscope_exclusive;
	break;
    case ELM_shared:
	l->scope = ne_lockscope_shared;
	break;
    case ELM_depth:
	NE_DEBUG(NE_DBG_LOCKS, "Got depth: %s\n", cdata);
	l->depth = parse_depth(cdata);
	if (l->depth == -1) {
	    return -1;
	}
	break;
    case ELM_timeout:
	NE_DEBUG(NE_DBG_LOCKS, "Got timeout: %s\n", cdata);
	l->timeout = parse_timeout(cdata);
	if (l->timeout == NE_TIMEOUT_INVALID) {
	    return -1;
	}
	break;
    case ELM_owner:
	l->owner = strdup(cdata);
	break;
    case ELM_href:
	l->token = strdup(cdata);
	break;
    }
    return 0;
}

/* End-element handler for lock discovery PROPFIND response */
static int end_element_ldisc(void *userdata, int state, 
                             const char *nspace, const char *name)
{
    struct discover_ctx *ctx = userdata;
    struct ne_lock *lock = ne_propfind_current_private(ctx->phandler);

    return end_element_common(lock, state, ctx->cdata->data);
}

static inline int can_accept(int parent, int id)
{
    return (parent == NE_XML_STATEROOT && id == ELM_prop) ||
        (parent == ELM_prop && id == ELM_lockdiscovery) ||
        (parent == ELM_lockdiscovery && id == ELM_activelock) ||
        (parent == ELM_activelock && 
         (id == ELM_lockscope || id == ELM_locktype ||
          id == ELM_depth || id == ELM_owner ||
          id == ELM_timeout || id == ELM_locktoken)) ||
        (parent == ELM_lockscope &&
         (id == ELM_exclusive || id == ELM_shared)) ||
        (parent == ELM_locktype && id == ELM_write) ||
        (parent == ELM_locktoken && id == ELM_href);
}

static int ld_startelm(void *userdata, int parent,
                       const char *nspace, const char *name,
		       const char **atts)
{
    struct discover_ctx *ctx = userdata;
    int id = ne_xml_mapid(element_map, NE_XML_MAPLEN(element_map),
                          nspace, name);
    
    ne_buffer_clear(ctx->cdata);
    
    if (can_accept(parent, id))
        return id;
    else
        return NE_XML_DECLINE;
}    

#define MAX_CDATA (256)

static int lk_cdata(void *userdata, int state,
                    const char *cdata, size_t len)
{
    struct lock_ctx *ctx = userdata;

    if (ctx->cdata->used + len < MAX_CDATA)
        ne_buffer_append(ctx->cdata, cdata, len);
    
    return 0;
}

static int ld_cdata(void *userdata, int state,
                    const char *cdata, size_t len)
{
    struct discover_ctx *ctx = userdata;

    if (ctx->cdata->used + len < MAX_CDATA)
        ne_buffer_append(ctx->cdata, cdata, len);
    
    return 0;
}

static int lk_startelm(void *userdata, int parent,
                       const char *nspace, const char *name,
		       const char **atts)
{
    struct lock_ctx *ctx = userdata;
    int id;

    id = ne_xml_mapid(element_map, NE_XML_MAPLEN(element_map), nspace, name);

    NE_DEBUG(NE_DBG_LOCKS, "lk_startelm: %s => %d\n", name, id);
    
    if (id == 0)
        return NE_XML_DECLINE;    

    if (parent == 0 && ctx->token == NULL) {
        const char *token = ne_get_response_header(ctx->req, "Lock-Token");
        /* at the root element; retrieve the Lock-Token header,
         * and bail if it wasn't given. */
        if (token == NULL) {
            ne_xml_set_error(ctx->parser, 
                             _("LOCK response missing Lock-Token header"));
            return NE_XML_ABORT;
        }

        if (token[0] == '<') token++;
        ctx->token = ne_strdup(token);
        ne_shave(ctx->token, ">");
        NE_DEBUG(NE_DBG_LOCKS, "lk_startelm: Finding token %s\n",
                 ctx->token);
    }

    /* TODO: only accept 'prop' as root for LOCK response */
    if (!can_accept(parent, id))
        return NE_XML_DECLINE;

    if (id == ELM_activelock && !ctx->found) {
	/* a new activelock */
	ne_lock_free(&ctx->active);
	memset(&ctx->active, 0, sizeof ctx->active);
        ctx->active.timeout = NE_TIMEOUT_INVALID;
    }

    ne_buffer_clear(ctx->cdata);

    return id;
}

/* End-element handler for LOCK response */
static int lk_endelm(void *userdata, int state,
                     const char *nspace, const char *name)
{
    struct lock_ctx *ctx = userdata;

    if (ctx->found)
	return 0;

    if (end_element_common(&ctx->active, state, ctx->cdata->data))
	return -1;

    if (state == ELM_activelock) {
	if (ctx->active.token && strcmp(ctx->active.token, ctx->token) == 0) {
	    ctx->found = 1;
	}
    }

    return 0;
}

/* Creator callback for private structure. */
static void *ld_create(void *userdata, const ne_uri *uri)
{
    struct ne_lock *lk = ne_lock_create();

    ne_uri_copy(&lk->uri, uri);

    return lk;
}

/* Destructor callback for private structure. */
static void ld_destroy(void *userdata, void *private)
{
    struct ne_lock *lk = private;

    ne_lock_destroy(lk);
}

/* Discover all locks on URI */
int ne_lock_discover(ne_session *sess, const char *uri, 
		     ne_lock_result callback, void *userdata)
{
    ne_propfind_handler *handler;
    struct discover_ctx ctx = {0};
    int ret;
    
    ctx.results = callback;
    ctx.userdata = userdata;
    ctx.cdata = ne_buffer_create();
    ctx.phandler = handler = ne_propfind_create(sess, uri, NE_DEPTH_ZERO);

    ne_propfind_set_private(handler, ld_create, ld_destroy, &ctx);
    
    ne_xml_push_handler(ne_propfind_get_parser(handler), 
                        ld_startelm, ld_cdata, end_element_ldisc, &ctx);
    
    ret = ne_propfind_named(handler, lock_props, discover_results, &ctx);
    
    ne_buffer_destroy(ctx.cdata);
    ne_propfind_destroy(handler);

    return ret;
}

static void add_timeout_header(ne_request *req, long timeout)
{
    if (timeout == NE_TIMEOUT_INFINITE) {
	ne_add_request_header(req, "Timeout", "Infinite");
    } 
    else if (timeout != NE_TIMEOUT_INVALID && timeout > 0) {
	ne_print_request_header(req, "Timeout", "Second-%ld", timeout);
    }
    /* just ignore it if timeout == 0 or invalid. */
}

int ne_lock(ne_session *sess, struct ne_lock *lock) 
{
    ne_request *req = ne_request_create(sess, "LOCK", lock->uri.path);
    ne_buffer *body = ne_buffer_create();
    ne_xml_parser *parser = ne_xml_create();
    int ret;
    struct lock_ctx ctx;

    memset(&ctx, 0, sizeof ctx);
    ctx.cdata = ne_buffer_create();    
    ctx.req = req;
    ctx.parser = parser;

    /* LOCK is not idempotent. */
    ne_set_request_flag(req, NE_REQFLAG_IDEMPOTENT, 0);

    ne_xml_push_handler(parser, lk_startelm, lk_cdata, lk_endelm, &ctx);
    
    /* Create the body */
    ne_buffer_concat(body, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		    "<lockinfo xmlns='DAV:'>\n" " <lockscope>",
		    lock->scope==ne_lockscope_exclusive?
		    "<exclusive/>":"<shared/>",
		    "</lockscope>\n"
		    "<locktype><write/></locktype>", NULL);

    if (lock->owner) {
	ne_buffer_concat(body, "<owner>", lock->owner, "</owner>\n", NULL);
    }
    ne_buffer_czappend(body, "</lockinfo>\n");

    ne_set_request_body_buffer(req, body->data, ne_buffer_size(body));
    ne_add_request_header(req, "Content-Type", NE_XML_MEDIA_TYPE);
    ne_add_depth_header(req, lock->depth);
    add_timeout_header(req, lock->timeout);
    
    /* TODO: 
     * By 2518, we need this only if we are creating a lock-null resource.
     * Since we don't KNOW whether the lock we're given is a lock-null
     * or not, we cover our bases.
     */
    ne_lock_using_parent(req, lock->uri.path);
    /* This one is clearer from 2518 sec 8.10.4. */
    ne_lock_using_resource(req, lock->uri.path, lock->depth);

    ret = ne_xml_dispatch_request(req, parser);

    ne_buffer_destroy(body);
    ne_buffer_destroy(ctx.cdata);
    
    if (ret == NE_OK && ne_get_status(req)->klass == 2) {
        if (ne_get_status(req)->code == 207) {
            ret = NE_ERROR;
            /* TODO: set the error string appropriately */
        } else if (ctx.found) {
	    /* it worked: copy over real lock details if given. */
            if (lock->token) ne_free(lock->token);
	    lock->token = ctx.token;
            ctx.token = NULL;
	    if (ctx.active.timeout != NE_TIMEOUT_INVALID)
		lock->timeout = ctx.active.timeout;
	    lock->scope = ctx.active.scope;
	    lock->type = ctx.active.type;
	    if (ctx.active.depth >= 0)
		lock->depth = ctx.active.depth;
	    if (ctx.active.owner) {
		if (lock->owner) ne_free(lock->owner);
		lock->owner = ctx.active.owner;
		ctx.active.owner = NULL;
	    }
	} else {
	    ret = NE_ERROR;
	    ne_set_error(sess, _("Response missing activelock for %s"), 
			 ctx.token);
	}
    } else if (ret == NE_OK /* && status != 2xx */) {
	ret = NE_ERROR;
    }

    ne_lock_free(&ctx.active);
    if (ctx.token) ne_free(ctx.token);
    ne_request_destroy(req);
    ne_xml_destroy(parser);

    return ret;
}

int ne_lock_refresh(ne_session *sess, struct ne_lock *lock)
{
    ne_request *req = ne_request_create(sess, "LOCK", lock->uri.path);
    ne_xml_parser *parser = ne_xml_create();
    int ret;
    struct lock_ctx ctx;

    memset(&ctx, 0, sizeof ctx);
    ctx.cdata = ne_buffer_create();
    ctx.req = req;
    ctx.token = lock->token;
    ctx.parser = parser;

    /* Handle the response and update *lock appropriately. */
    ne_xml_push_handler(parser, lk_startelm, lk_cdata, lk_endelm, &ctx);
    
    /* For a lock refresh, submitting only this lock token must be
     * sufficient. */
    ne_print_request_header(req, "If", "(<%s>)", lock->token);
    add_timeout_header(req, lock->timeout);

    ret = ne_xml_dispatch_request(req, parser);

    if (ret == NE_OK) {
        if (ne_get_status(req)->klass != 2) {
            ret = NE_ERROR; /* and use default session error */
	} else if (!ctx.found) {
            ne_set_error(sess, _("No activelock for <%s> returned in "
                                 "LOCK refresh response"), lock->token);
            ret = NE_ERROR;
        } else /* success! */ {
            /* update timeout for passed-in lock structure. */
            lock->timeout = ctx.active.timeout;
        }
    }

    ne_lock_free(&ctx.active);
    ne_buffer_destroy(ctx.cdata);
    ne_request_destroy(req);
    ne_xml_destroy(parser);

    return ret;
}
