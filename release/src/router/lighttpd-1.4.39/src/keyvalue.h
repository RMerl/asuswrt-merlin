#ifndef _KEY_VALUE_H_
#define _KEY_VALUE_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_PCRE_H
# include <pcre.h>
#endif

struct server;

/* sources:
 * - [RFC2616], Section 9
 *   (or http://tools.ietf.org/html/draft-ietf-httpbis-p2-semantics-22)
 * - http://tools.ietf.org/html/draft-ietf-httpbis-method-registrations-11, Appendix A
 *
 * http://tools.ietf.org/html/draft-ietf-httpbis-p2-semantics-22, Section 8.1 defines
 * a new registry (not available yet):
 *   http://www.iana.org/assignments/http-methods
 */

typedef enum {
	HTTP_METHOD_UNSET = -1,
	HTTP_METHOD_GET,               /* [RFC2616], Section 9.3 */
	HTTP_METHOD_HEAD,              /* [RFC2616], Section 9.4 */
	HTTP_METHOD_POST,              /* [RFC2616], Section 9.5 */
	HTTP_METHOD_PUT,               /* [RFC2616], Section 9.6 */
	HTTP_METHOD_DELETE,            /* [RFC2616], Section 9.7 */
	HTTP_METHOD_CONNECT,           /* [RFC2616], Section 9.9 */
	HTTP_METHOD_OPTIONS,           /* [RFC2616], Section 9.2 */
	HTTP_METHOD_TRACE,             /* [RFC2616], Section 9.8 */
	HTTP_METHOD_ACL,               /* [RFC3744], Section 8.1 */
	HTTP_METHOD_BASELINE_CONTROL,  /* [RFC3253], Section 12.6 */
	HTTP_METHOD_BIND,              /* [RFC5842], Section 4 */
	HTTP_METHOD_CHECKIN,           /* [RFC3253], Section 4.4 and [RFC3253], Section 9.4 */
	HTTP_METHOD_CHECKOUT,          /* [RFC3253], Section 4.3 and [RFC3253], Section 8.8 */
	HTTP_METHOD_COPY,              /* [RFC4918], Section 9.8 */
	HTTP_METHOD_LABEL,             /* [RFC3253], Section 8.2 */
	HTTP_METHOD_LINK,              /* [RFC2068], Section 19.6.1.2 */
	HTTP_METHOD_LOCK,              /* [RFC4918], Section 9.10 */
	HTTP_METHOD_MERGE,             /* [RFC3253], Section 11.2 */
	HTTP_METHOD_MKACTIVITY,        /* [RFC3253], Section 13.5 */
	HTTP_METHOD_MKCALENDAR,        /* [RFC4791], Section 5.3.1 */
	HTTP_METHOD_MKCOL,             /* [RFC4918], Section 9.3 */
	HTTP_METHOD_MKREDIRECTREF,     /* [RFC4437], Section 6 */
	HTTP_METHOD_MKWORKSPACE,       /* [RFC3253], Section 6.3 */
	HTTP_METHOD_MOVE,              /* [RFC4918], Section 9.9 */
	HTTP_METHOD_ORDERPATCH,        /* [RFC3648], Section 7 */
	HTTP_METHOD_PATCH,             /* [RFC5789], Section 2 */
	HTTP_METHOD_PROPFIND,          /* [RFC4918], Section 9.1 */
	HTTP_METHOD_PROPPATCH,         /* [RFC4918], Section 9.2 */
	HTTP_METHOD_REBIND,            /* [RFC5842], Section 6 */
	HTTP_METHOD_REPORT,            /* [RFC3253], Section 3.6 */
	HTTP_METHOD_SEARCH,            /* [RFC5323], Section 2 */
	HTTP_METHOD_UNBIND,            /* [RFC5842], Section 5 */
	HTTP_METHOD_UNCHECKOUT,        /* [RFC3253], Section 4.5 */
	HTTP_METHOD_UNLINK,            /* [RFC2068], Section 19.6.1.3 */
	HTTP_METHOD_UNLOCK,            /* [RFC4918], Section 9.11 */
	HTTP_METHOD_UPDATE,            /* [RFC3253], Section 7.1 */
	HTTP_METHOD_UPDATEREDIRECTREF, /* [RFC4437], Section 7 */
	HTTP_METHOD_VERSION_CONTROL,   /* [RFC3253], Section 3.5 */

    // Sungmin add
    HTTP_METHOD_OAUTH,
	HTTP_METHOD_WOL,
	HTTP_METHOD_GSL,
	HTTP_METHOD_LOGOUT,
	HTTP_METHOD_GETSRVTIME,
	HTTP_METHOD_RESCANSMBPC,
	HTTP_METHOD_GETROUTERMAC,
	HTTP_METHOD_GETFIRMVER,
	HTTP_METHOD_GETROUTERINFO,
	HTTP_METHOD_GETNOTICE,
	HTTP_METHOD_GSLL,
	HTTP_METHOD_REMOVESL,
	HTTP_METHOD_GETLATESTVER,
	HTTP_METHOD_GETDISKSPACE,
	HTTP_METHOD_PROPFINDMEDIALIST,
	HTTP_METHOD_GETMUSICCLASSIFICATION,
	HTTP_METHOD_GETMUSICPLAYLIST,
	HTTP_METHOD_GETTHUMBIMAGE,
	HTTP_METHOD_GETPRODUCTICON,
	HTTP_METHOD_GETVIDEOSUBTITLE,
	HTTP_METHOD_UPLOADTOFACEBOOK,
	HTTP_METHOD_UPLOADTOFLICKR,
	HTTP_METHOD_UPLOADTOPICASA,	
	HTTP_METHOD_UPLOADTOTWITTER,
	HTTP_METHOD_GENROOTCERTIFICATE,
	HTTP_METHOD_SETROOTCERTIFICATE,
	HTTP_METHOD_GETX509CERTINFO,
	HTTP_METHOD_APPLYAPP,
	HTTP_METHOD_NVRAMGET,
	HTTP_METHOD_GETCPUUSAGE,
	HTTP_METHOD_GETMEMORYUSAGE,
	HTTP_METHOD_UPDATEACCOUNT,
	HTTP_METHOD_GETACCOUNTINFO,
	HTTP_METHOD_GETACCOUNTLIST,
	HTTP_METHOD_DELETEACCOUNT,
	HTTP_METHOD_UPDATEACCOUNTINVITE,
	HTTP_METHOD_GETACCOUNTINVITEINFO,
	HTTP_METHOD_GETACCOUNTINVITELIST,
	HTTP_METHOD_DELETEACCOUNTINVITE,
	HTTP_METHOD_OPENSTREAMINGPORT
} http_method_t;

typedef enum { HTTP_VERSION_UNSET = -1, HTTP_VERSION_1_0, HTTP_VERSION_1_1 } http_version_t;

typedef struct {
	int key;

	char *value;
} keyvalue;

typedef struct {
	char *key;

	char *value;
} s_keyvalue;

typedef struct {
#ifdef HAVE_PCRE_H
	pcre *key;
	pcre_extra *key_extra;
#endif

	buffer *value;
} pcre_keyvalue;

typedef enum { HTTP_AUTH_BASIC, HTTP_AUTH_DIGEST } httpauth_type;

typedef struct {
	char *key;

	char *realm;
	httpauth_type type;
} httpauth_keyvalue;

#define KVB(x) \
typedef struct {\
	x **kv; \
	size_t used;\
	size_t size;\
} x ## _buffer

KVB(keyvalue);
KVB(s_keyvalue);
KVB(httpauth_keyvalue);
KVB(pcre_keyvalue);

const char *get_http_status_name(int i);
const char *get_http_version_name(int i);
const char *get_http_method_name(http_method_t i);
const char *get_http_status_body_name(int i);
int get_http_version_key(const char *s);
http_method_t get_http_method_key(const char *s);

const char *keyvalue_get_value(keyvalue *kv, int k);
int keyvalue_get_key(keyvalue *kv, const char *s);

keyvalue_buffer *keyvalue_buffer_init(void);
int keyvalue_buffer_append(keyvalue_buffer *kvb, int k, const char *value);
void keyvalue_buffer_free(keyvalue_buffer *kvb);

s_keyvalue_buffer *s_keyvalue_buffer_init(void);
int s_keyvalue_buffer_append(s_keyvalue_buffer *kvb, const char *key, const char *value);
void s_keyvalue_buffer_free(s_keyvalue_buffer *kvb);

httpauth_keyvalue_buffer *httpauth_keyvalue_buffer_init(void);
int httpauth_keyvalue_buffer_append(httpauth_keyvalue_buffer *kvb, const char *key, const char *realm, httpauth_type type);
void httpauth_keyvalue_buffer_free(httpauth_keyvalue_buffer *kvb);

pcre_keyvalue_buffer *pcre_keyvalue_buffer_init(void);
int pcre_keyvalue_buffer_append(struct server *srv, pcre_keyvalue_buffer *kvb, const char *key, const char *value);
void pcre_keyvalue_buffer_free(pcre_keyvalue_buffer *kvb);

#endif
