/* 
   Basic HTTP and WebDAV methods
   Copyright (C) 1999-2008, Joe Orton <joe@manyfish.co.uk>

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

#include <sys/types.h>
#include <sys/stat.h> /* for struct stat */

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <errno.h>

#include "ne_request.h"
#include "ne_alloc.h"
#include "ne_utils.h"
#include "ne_basic.h"
#include "ne_207.h"

#ifdef NE_HAVE_DAV
#include "ne_uri.h"
#include "ne_locks.h"
#endif

#include "ne_dates.h"
#include "ne_internal.h"

int ne_getmodtime(ne_session *sess, const char *uri, time_t *modtime) 
{
    ne_request *req = ne_request_create(sess, "HEAD", uri);
    const char *value;
    int ret;

    ret = ne_request_dispatch(req);

    value = ne_get_response_header(req, "Last-Modified"); 

    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	*modtime = -1;
	ret = NE_ERROR;
    } 
    else if (value) {
    	//printf("value = %s\n",value);
        *modtime = ne_httpdate_parse(value);
    }
    else {
        *modtime = -1;
    }

    ne_request_destroy(req);

    return ret;
}

#ifdef NE_LFS
#define ne_fstat fstat64
typedef struct stat64 struct_stat;
#else
#define ne_fstat fstat
typedef struct stat struct_stat;
#endif

/* PUT's from fd to URI */
int ne_put(ne_session *sess, const char *uri, int fd) 
{
    ne_request *req;
    struct_stat st;
    int ret;

    if (ne_fstat(fd, &st)) {
        int errnum = errno;
        char buf[200];

        ne_set_error(sess, _("Could not determine file size: %s"),
                     ne_strerror(errnum, buf, sizeof buf));
        return NE_ERROR;
    }
    
    req = ne_request_create(sess, "PUT", uri);

#ifdef NE_HAVE_DAV
    ne_lock_using_resource(req, uri, 0);
    ne_lock_using_parent(req, uri);
#endif

    ne_set_request_body_fd(req, fd, 0, st.st_size);
	
    ret = ne_request_dispatch(req);
    
    if (ret == NE_OK && ne_get_status(req)->klass != 2)
	ret = NE_ERROR;

    ne_request_destroy(req);

    return ret;
}

/* Dispatch a GET request REQ, writing the response body to FD fd.  If
 * RANGE is non-NULL, then it is the value of the Range request
 * header, e.g. "bytes=1-5".  Returns an NE_* error code. */
static int dispatch_to_fd(ne_request *req, int fd, const char *range)
{
    ne_session *const sess = ne_get_session(req);
    const ne_status *const st = ne_get_status(req);
    int ret;
    size_t rlen;

    /* length of bytespec after "bytes=" */
    rlen = range ? strlen(range + 6) : 0;

    do {
        const char *value;
        
        ret = ne_begin_request(req);
        if (ret != NE_OK) break;

        value = ne_get_response_header(req, "Content-Range");

        /* For a 206 response, check that a Content-Range header is
         * given which matches the Range request header. */
        if (range && st->code == 206 
            && (value == NULL || strncmp(value, "bytes ", 6) != 0
                || strncmp(range + 6, value + 6, rlen)
                || (range[5 + rlen] != '-' && value[6 + rlen] != '/'))) {
            ne_set_error(sess, _("Response did not include requested range"));
            return NE_ERROR;
        }

        if ((range && st->code == 206) || (!range && st->klass == 2)) {
            ret = ne_read_response_to_fd(req, fd);
        } else {
            ret = ne_discard_response(req);
        }

        if (ret == NE_OK) ret = ne_end_request(req);
    } while (ret == NE_RETRY);

    return ret;
}

static int get_range_common(ne_session *sess, const char *uri, 
                            const char *brange, int fd)

{
    ne_request *req = ne_request_create(sess, "GET", uri);
    const ne_status *status;
    int ret;

    ne_add_request_header(req, "Range", brange);
    ne_add_request_header(req, "Accept-Ranges", "bytes");

    ret = dispatch_to_fd(req, fd, brange);

    status = ne_get_status(req);

    if (ret == NE_OK && status->code == 416) {
	/* connection is terminated too early with Apache/1.3, so we check
	 * this even if ret == NE_ERROR... */
	ne_set_error(sess, _("Range is not satisfiable"));
	ret = NE_ERROR;
    }
    else if (ret == NE_OK) {
	if (status->klass == 2 && status->code != 206) {
	    ne_set_error(sess, _("Resource does not support ranged GET requests"));
	    ret = NE_ERROR;
	}
	else if (status->klass != 2) {
	    ret = NE_ERROR;
	}
    } 
    
    ne_request_destroy(req);

    return ret;
}

int ne_get_range(ne_session *sess, const char *uri, 
		 ne_content_range *range, int fd)
{
    char brange[64];

    if (range->end == -1) {
        ne_snprintf(brange, sizeof brange, "bytes=%" FMT_NE_OFF_T "-", 
                    range->start);
    }
    else {
	ne_snprintf(brange, sizeof brange,
                    "bytes=%" FMT_NE_OFF_T "-%" FMT_NE_OFF_T,
                    range->start, range->end);
    }

    return get_range_common(sess, uri, brange, fd);
}

/* Get to given fd */
int ne_get(ne_session *sess, const char *uri, int fd)
{
    ne_request *req = ne_request_create(sess, "GET", uri);
    int ret;

    ret = dispatch_to_fd(req, fd, NULL);
    
    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    }

    ne_request_destroy(req);

    return ret;
}


/* Get to given fd */
int ne_post(ne_session *sess, const char *uri, int fd, const char *buffer)
{
    ne_request *req = ne_request_create(sess, "POST", uri);
    int ret;

    ne_set_request_flag(req, NE_REQFLAG_IDEMPOTENT, 0);

    ne_set_request_body_buffer(req, buffer, strlen(buffer));

    ret = dispatch_to_fd(req, fd, NULL);
    
    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    }

    ne_request_destroy(req);

    return ret;
}

int ne_get_content_type(ne_request *req, ne_content_type *ct)
{
    const char *value;
    char *sep, *stype;

    value = ne_get_response_header(req, "Content-Type");
    if (value == NULL || strchr(value, '/') == NULL) {
        return -1;
    }

    ct->value = ne_strdup(value);
    
    stype = strchr(ct->value, '/');

    *stype++ = '\0';
    ct->type = ct->value;
    ct->charset = NULL;
    
    sep = strchr(stype, ';');

    if (sep) {
	char *tok;
	/* look for the charset parameter. TODO; probably better to
	 * hand-carve a parser than use ne_token/strstr/shave here. */
	*sep++ = '\0';
	do {
	    tok = ne_qtoken(&sep, ';', "\"\'");
	    if (tok) {
		tok = strstr(tok, "charset=");
		if (tok)
		    ct->charset = ne_shave(tok+8, "\"\'");
	    } else {
		break;
	    }
	} while (sep != NULL);
    }

    /* set subtype, losing any trailing whitespace */
    ct->subtype = ne_shave(stype, " \t");
    
    if (ct->charset == NULL && ne_strcasecmp(ct->type, "text") == 0) {
        /* 3280§3.1: text/xml without charset implies us-ascii. */
        if (ne_strcasecmp(ct->subtype, "xml") == 0)
            ct->charset = "us-ascii";
        /* 2616§3.7.1: subtypes of text/ default to charset ISO-8859-1. */
        else
            ct->charset = "ISO-8859-1";
    }
    
    return 0;
}

static const struct options_map {
    const char *name;
    unsigned int cap;
} options_map[] = {
    { "1", NE_CAP_DAV_CLASS1 },
    { "2", NE_CAP_DAV_CLASS2 },
    { "3", NE_CAP_DAV_CLASS3 },
    { "<http://apache.org/dav/propset/fs/1>", NE_CAP_MODDAV_EXEC },
    { "access-control", NE_CAP_DAV_ACL },
    { "version-control", NE_CAP_VER_CONTROL },
    { "checkout-in-place", NE_CAP_CO_IN_PLACE },
    { "version-history", NE_CAP_VER_HISTORY },
    { "workspace", NE_CAP_WORKSPACE },
    { "update", NE_CAP_UPDATE },
    { "label", NE_CAP_LABEL },
    { "working-resource", NE_CAP_WORK_RESOURCE },
    { "merge", NE_CAP_MERGE },
    { "baseline", NE_CAP_BASELINE },
    { "version-controlled-collection", NE_CAP_VC_COLLECTION }
};

static void parse_dav_header(const char *value, unsigned int *caps)
{
    char *tokens = ne_strdup(value), *pnt = tokens;
    
    *caps = 0;

    do {
        char *tok = ne_qtoken(&pnt, ',',  "\"'");
        unsigned n;

        if (!tok) break;
        
        tok = ne_shave(tok, " \r\t\n");

        for (n = 0; n < sizeof(options_map)/sizeof(options_map[0]); n++) {
            if (strcmp(tok, options_map[n].name) == 0) {
                *caps |= options_map[n].cap;
            }
        }
    } while (pnt != NULL);
    
    ne_free(tokens);
}

int ne_options2(ne_session *sess, const char *uri, unsigned int *caps)
{
    ne_request *req = ne_request_create(sess, "OPTIONS", uri);
    int ret = ne_request_dispatch(req);
    const char *header = ne_get_response_header(req, "DAV");
    
    if (header) parse_dav_header(header, caps);
 
    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    }
    
    ne_request_destroy(req);

    return ret;
}

int ne_options(ne_session *sess, const char *path,
               ne_server_capabilities *caps)
{
    int ret;
    unsigned int capmask = 0;
    
    memset(caps, 0, sizeof *caps);

    ret = ne_options2(sess, path, &capmask);

    caps->dav_class1 = capmask & NE_CAP_DAV_CLASS1 ? 1 : 0;
    caps->dav_class2 = capmask & NE_CAP_DAV_CLASS2 ? 1 : 0;
    caps->dav_executable = capmask & NE_CAP_MODDAV_EXEC ? 1 : 0;
    
    return ret;
}

#ifdef NE_HAVE_DAV

void ne_add_depth_header(ne_request *req, int depth)
{
    const char *value;
    switch(depth) {
    case NE_DEPTH_ZERO:
	value = "0";
	break;
    case NE_DEPTH_ONE:
	value = "1";
	break;
    default:
	value = "infinity";
	break;
    }
    ne_add_request_header(req, "Depth", value);
}

static int copy_or_move(ne_session *sess, int is_move, int overwrite,
			int depth, const char *src, const char *dest) 
{
    ne_request *req = ne_request_create( sess, is_move?"MOVE":"COPY", src );

    /* 2518 S8.9.2 says only use Depth: infinity with MOVE. */
    if (!is_move) {
	ne_add_depth_header(req, depth);
    }

#ifdef NE_HAVE_DAV
    if (is_move) {
	ne_lock_using_resource(req, src, NE_DEPTH_INFINITE);
    }
    ne_lock_using_resource(req, dest, NE_DEPTH_INFINITE);
    /* And we need to be able to add members to the destination's parent */
    ne_lock_using_parent(req, dest);
#endif

    if (ne_get_session_flag(sess, NE_SESSFLAG_RFC4918)) {
        ne_add_request_header(req, "Destination", dest);
    }
    else {
        ne_print_request_header(req, "Destination", "%s://%s%s", 
                                ne_get_scheme(sess), 
                                ne_get_server_hostport(sess), dest);
    }
    
    ne_add_request_header(req, "Overwrite", overwrite?"T":"F");

    return ne_simple_request(sess, req);
}

int ne_copy(ne_session *sess, int overwrite, int depth,
	     const char *src, const char *dest) 
{
    return copy_or_move(sess, 0, overwrite, depth, src, dest);
}

int ne_move(ne_session *sess, int overwrite,
	     const char *src, const char *dest) 
{
    return copy_or_move(sess, 1, overwrite, 0, src, dest);
}

/* Deletes the specified resource. (and in only two lines of code!) */
int ne_delete(ne_session *sess, const char *uri) 
{
    ne_request *req = ne_request_create(sess, "DELETE", uri);

#ifdef NE_HAVE_DAV
    ne_lock_using_resource(req, uri, NE_DEPTH_INFINITE);
    ne_lock_using_parent(req, uri);
#endif
    
    /* joe: I asked on the DAV WG list about whether we might get a
     * 207 error back from a DELETE... conclusion, you shouldn't if
     * you don't send the Depth header, since we might be an HTTP/1.1
     * client and a 2xx response indicates success to them.  But
     * it's all a bit unclear. In any case, DAV servers today do
     * return 207 to DELETE even if we don't send the Depth header.
     * So we handle 207 errors appropriately. */

    return ne_simple_request(sess, req);
}

int ne_mkcol(ne_session *sess, const char *uri) 
{
    ne_request *req;
    char *real_uri;
    int ret;

    if (ne_path_has_trailing_slash(uri)) {
	real_uri = ne_strdup(uri);
    } else {
	real_uri = ne_concat(uri, "/", NULL);
    }

    req = ne_request_create(sess, "MKCOL", real_uri);

#ifdef NE_HAVE_DAV
    ne_lock_using_resource(req, real_uri, 0);
    ne_lock_using_parent(req, real_uri);
#endif
    
    ret = ne_simple_request(sess, req);

    ne_free(real_uri);

    return ret;
}

#endif /* NE_HAVE_DAV */
