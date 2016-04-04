#include "base.h"
#include "log.h"
#include "buffer.h"
#include "base64.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(USE_OPENSSL)
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif

#include "md5.h"

#define HASHLEN 16
typedef unsigned char HASH[HASHLEN];
#define HASHHEXLEN 32
typedef char HASHHEX[HASHHEXLEN+1];

/*
 * mod_secdownload verifies a checksum associated with a timestamp
 * and a path.
 *
 * It takes an URL of the form:
 *   securl := <uri-prefix> <mac> <protected-path>
 *   uri-prefix := '/' any*         # whatever was configured: must start with a '/')
 *   mac := [a-zA-Z0-9_-]{mac_len}  # mac length depends on selected algorithm
 *   protected-path := '/' <timestamp> <rel-path>
 *   timestamp := [a-f0-9]{8}       # timestamp when the checksum was calculated
 *                                  # to prevent access after timeout (active requests
 *                                  # will finish successfully even after the timeout)
 *   rel-path := '/' any*           # the protected path; changing the path breaks the
 *                                  # checksum
 *
 * The timestamp is the `epoch` timestamp in hex, i.e. time in seconds
 * since 00:00:00 UTC on 1 January 1970.
 *
 * mod_secdownload supports various MAC algorithms:
 *
 * # md5
 * mac_len := 32 (and hex only)
 * mac := md5-hex(<secrect><rel-path><timestamp>)   # lowercase hex
 * perl example:
    use Digest::MD5 qw(md5_hex);
    my $secret = "verysecret";
    my $rel_path = "/index.html"
    my $xtime = sprintf("%08x", time);
    my $url = '/'. md5_hex($secret . $rel_path . $xtime) . '/' . $xtime . $rel_path;
 *
 * # hmac-sha1
 * mac_len := 27  (no base64 padding)
 * mac := base64-url(hmac-sha1(<secret>, <protected-path>))
 * perl example:
    use Digest::SHA qw(hmac_sha1);
    use MIME::Base64 qw(encode_base64url);
    my $secret = "verysecret";
    my $rel_path = "/index.html"
    my $protected_path = '/' . sprintf("%08x", time) . $rel_path;
    my $url = '/'. encode_base64url(hmac_sha1($protected_path, $secret)) . $protected_path;
 *
 * # hmac-256
 * mac_len := 43  (no base64 padding)
 * mac := base64-url(hmac-256(<secret>, <protected-path>))
    use Digest::SHA qw(hmac_sha256);
    use MIME::Base64 qw(encode_base64url);
    my $secret = "verysecret";
    my $rel_path = "/index.html"
    my $protected_path = '/' . sprintf("%08x", time) . $rel_path;
    my $url = '/'. encode_base64url(hmac_sha256($protected_path, $secret)) . $protected_path;
 *
 */

/* plugin config for all request/connections */

typedef enum {
	SECDL_INVALID = 0,
	SECDL_MD5 = 1,
	SECDL_HMAC_SHA1 = 2,
	SECDL_HMAC_SHA256 = 3,
} secdl_algorithm;

typedef struct {
	buffer *doc_root;
	buffer *secret;
	buffer *uri_prefix;
	secdl_algorithm algorithm;

	unsigned int timeout;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

static int const_time_memeq(const char *a, const char *b, size_t len) {
	/* constant time memory compare, unless the compiler figures it out */
	char diff = 0;
	size_t i;
	for (i = 0; i < len; ++i) {
		diff |= (a[i] ^ b[i]);
	}
	return 0 == diff;
}

static const char* secdl_algorithm_names[] = {
	"invalid",
	"md5",
	"hmac-sha1",
	"hmac-sha256",
};

static secdl_algorithm algorithm_from_string(buffer *name) {
	size_t ndx;

	if (buffer_string_is_empty(name)) return SECDL_INVALID;

	for (ndx = 1; ndx < sizeof(secdl_algorithm_names)/sizeof(secdl_algorithm_names[0]); ++ndx) {
		if (0 == strcmp(secdl_algorithm_names[ndx], name->ptr)) return (secdl_algorithm)ndx;
	}

	return SECDL_INVALID;
}

static size_t secdl_algorithm_mac_length(secdl_algorithm alg) {
	switch (alg) {
	case SECDL_INVALID:
		break;
	case SECDL_MD5:
		return 32;
	case SECDL_HMAC_SHA1:
		return 27;
	case SECDL_HMAC_SHA256:
		return 43;
	}
	return 0;
}

static int secdl_verify_mac(server *srv, plugin_config *config, const char* protected_path, const char* mac, size_t maclen) {
	if (0 == maclen || secdl_algorithm_mac_length(config->algorithm) != maclen) return 0;

	switch (config->algorithm) {
	case SECDL_INVALID:
		break;
	case SECDL_MD5:
		{
			li_MD5_CTX Md5Ctx;
			HASH HA1;
			char hexmd5[32];
			const char *ts_str;
			const char *rel_uri;

			/* legacy message:
			 *   protected_path := '/' <timestamp-hex> <rel-path>
			 *   timestamp-hex := [0-9a-f]{8}
			 *   rel-path := '/' any*
			 *   (the protected path was already verified)
			 * message = <secret><rel-path><timestamp-hex>
			 */
			ts_str = protected_path + 1;
			rel_uri = ts_str + 8;

			li_MD5_Init(&Md5Ctx);
			li_MD5_Update(&Md5Ctx, CONST_BUF_LEN(config->secret));
			li_MD5_Update(&Md5Ctx, rel_uri, strlen(rel_uri));
			li_MD5_Update(&Md5Ctx, ts_str, 8);
			li_MD5_Final(HA1, &Md5Ctx);

			li_tohex(hexmd5, (const char *)HA1, 16);

			return (32 == maclen) && const_time_memeq(mac, hexmd5, 32);
		}
	case SECDL_HMAC_SHA1:
#if defined(USE_OPENSSL)
		{
			unsigned char digest[20];
			char base64_digest[27];

			if (NULL == HMAC(
					EVP_sha1(),
					(unsigned char const*) CONST_BUF_LEN(config->secret),
					(unsigned char const*) protected_path, strlen(protected_path),
					digest, NULL)) {
				log_error_write(srv, __FILE__, __LINE__, "s",
					"hmac-sha1: HMAC() failed");
				return 0;
			}

			li_to_base64_no_padding(base64_digest, 27, digest, 20, BASE64_URL);

			return (27 == maclen) && const_time_memeq(mac, base64_digest, 27);
		}
#endif
		break;
	case SECDL_HMAC_SHA256:
#if defined(USE_OPENSSL)
		{
			unsigned char digest[32];
			char base64_digest[43];

			if (NULL == HMAC(
					EVP_sha256(),
					(unsigned char const*) CONST_BUF_LEN(config->secret),
					(unsigned char const*) protected_path, strlen(protected_path),
					digest, NULL)) {
				log_error_write(srv, __FILE__, __LINE__, "s",
					"hmac-sha256: HMAC() failed");
				return 0;
			}

			li_to_base64_no_padding(base64_digest, 43, digest, 32, BASE64_URL);

			return (43 == maclen) && const_time_memeq(mac, base64_digest, 43);
		}
#endif
		break;
	}

	return 0;
}

/* init the plugin data */
INIT_FUNC(mod_secdownload_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_secdownload_free) {
	plugin_data *p = p_d;
	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			buffer_free(s->secret);
			buffer_free(s->doc_root);
			buffer_free(s->uri_prefix);

			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_secdownload_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "secdownload.secret",        NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 0 */
		{ "secdownload.document-root", NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 1 */
		{ "secdownload.uri-prefix",    NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 2 */
		{ "secdownload.timeout",       NULL, T_CONFIG_INT,    T_CONFIG_SCOPE_CONNECTION }, /* 3 */
		{ "secdownload.algorithm",     NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 4 */
		{ NULL,                        NULL, T_CONFIG_UNSET,  T_CONFIG_SCOPE_UNSET      }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;
		buffer *algorithm = buffer_init();

		s = calloc(1, sizeof(plugin_config));
		s->secret        = buffer_init();
		s->doc_root      = buffer_init();
		s->uri_prefix    = buffer_init();
		s->timeout       = 60;

		cv[0].destination = s->secret;
		cv[1].destination = s->doc_root;
		cv[2].destination = s->uri_prefix;
		cv[3].destination = &(s->timeout);
		cv[4].destination = algorithm;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			buffer_free(algorithm);
			return HANDLER_ERROR;
		}

		if (!buffer_is_empty(algorithm)) {
			s->algorithm = algorithm_from_string(algorithm);
			switch (s->algorithm) {
			case SECDL_INVALID:
				log_error_write(srv, __FILE__, __LINE__, "sb",
					"invalid secdownload.algorithm:",
					algorithm);
				buffer_free(algorithm);
				return HANDLER_ERROR;
#if !defined(USE_OPENSSL)
			case SECDL_HMAC_SHA1:
			case SECDL_HMAC_SHA256:
				log_error_write(srv, __FILE__, __LINE__, "sb",
					"unsupported secdownload.algorithm:",
					algorithm);
				buffer_free(algorithm);
				return HANDLER_ERROR;
#endif
			default:
				break;
			}
		}

		buffer_free(algorithm);
	}

	return HANDLER_GO_ON;
}

/**
 * checks if the supplied string is a hex string
 *
 * @param str a possible hex string
 * @return if the supplied string is a valid hex string 1 is returned otherwise 0
 */

static int is_hex_len(const char *str, size_t len) {
	size_t i;

	if (NULL == str) return 0;

	for (i = 0; i < len && *str; i++, str++) {
		/* illegal characters */
		if (!((*str >= '0' && *str <= '9') ||
		      (*str >= 'a' && *str <= 'f') ||
		      (*str >= 'A' && *str <= 'F'))
		    ) {
			return 0;
		}
	}

	return i == len;
}

/**
 * checks if the supplied string is a base64 (modified URL) string
 *
 * @param str a possible base64 (modified URL) string
 * @return if the supplied string is a valid base64 (modified URL) string 1 is returned otherwise 0
 */

static int is_base64_len(const char *str, size_t len) {
	size_t i;

	if (NULL == str) return 0;

	for (i = 0; i < len && *str; i++, str++) {
		/* illegal characters */
		if (!((*str >= '0' && *str <= '9') ||
		      (*str >= 'a' && *str <= 'z') ||
		      (*str >= 'A' && *str <= 'Z') ||
		      (*str == '-') || (*str == '_'))
		    ) {
			return 0;
		}
	}

	return i == len;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_secdownload_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(secret);
	PATCH(doc_root);
	PATCH(uri_prefix);
	PATCH(timeout);
	PATCH(algorithm);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("secdownload.secret"))) {
				PATCH(secret);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("secdownload.document-root"))) {
				PATCH(doc_root);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("secdownload.uri-prefix"))) {
				PATCH(uri_prefix);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("secdownload.timeout"))) {
				PATCH(timeout);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("secdownload.algorithm"))) {
				PATCH(algorithm);
			}
		}
	}

	return 0;
}
#undef PATCH


URIHANDLER_FUNC(mod_secdownload_uri_handler) {
	plugin_data *p = p_d;
	const char *rel_uri, *ts_str, *mac_str, *protected_path;
	time_t ts = 0;
	size_t i, mac_len;

	if (con->mode != DIRECT) return HANDLER_GO_ON;

	if (buffer_is_empty(con->uri.path)) return HANDLER_GO_ON;

	mod_secdownload_patch_connection(srv, con, p);

	if (buffer_string_is_empty(p->conf.uri_prefix)) return HANDLER_GO_ON;

	if (buffer_string_is_empty(p->conf.secret)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"secdownload.secret has to be set");
		con->http_status = 500;
		return HANDLER_FINISHED;
	}

	if (buffer_string_is_empty(p->conf.doc_root)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"secdownload.document-root has to be set");
		con->http_status = 500;
		return HANDLER_FINISHED;
	}

	if (SECDL_INVALID == p->conf.algorithm) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"secdownload.algorithm has to be set");
		con->http_status = 500;
		return HANDLER_FINISHED;
	}

	mac_len = secdl_algorithm_mac_length(p->conf.algorithm);

	if (0 != strncmp(con->uri.path->ptr, p->conf.uri_prefix->ptr, buffer_string_length(p->conf.uri_prefix))) return HANDLER_GO_ON;

	mac_str = con->uri.path->ptr + buffer_string_length(p->conf.uri_prefix);

	if (!is_base64_len(mac_str, mac_len)) return HANDLER_GO_ON;

	protected_path = mac_str + mac_len;
	if (*protected_path != '/') return HANDLER_GO_ON;

	ts_str = protected_path + 1;
	if (!is_hex_len(ts_str, 8)) return HANDLER_GO_ON;
	if (*(ts_str + 8) != '/') return HANDLER_GO_ON;

	for (i = 0; i < 8; i++) {
		ts = (ts << 4) + hex2int(ts_str[i]);
	}

	/* timed-out */
	if ( (srv->cur_ts > ts && (unsigned int) (srv->cur_ts - ts) > p->conf.timeout) ||
	     (srv->cur_ts < ts && (unsigned int) (ts - srv->cur_ts) > p->conf.timeout) ) {
		/* "Gone" as the url will never be valid again instead of "408 - Timeout" where the request may be repeated */
		con->http_status = 410;

		return HANDLER_FINISHED;
	}

	rel_uri = ts_str + 8;

	if (!secdl_verify_mac(srv, &p->conf, protected_path, mac_str, mac_len)) {
		con->http_status = 403;

		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__, "sb",
				"mac invalid:",
				con->uri.path);
		}

		return HANDLER_FINISHED;
	}

	/* starting with the last / we should have relative-path to the docroot
	 */

	buffer_copy_buffer(con->physical.doc_root, p->conf.doc_root);
	buffer_copy_buffer(con->physical.basedir, p->conf.doc_root);
	buffer_copy_string(con->physical.rel_path, rel_uri);
	buffer_copy_buffer(con->physical.path, con->physical.doc_root);
	buffer_append_string_buffer(con->physical.path, con->physical.rel_path);

	return HANDLER_GO_ON;
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_secdownload_plugin_init(plugin *p);
int mod_secdownload_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("secdownload");

	p->init        = mod_secdownload_init;
	p->handle_physical  = mod_secdownload_uri_handler;
	p->set_defaults  = mod_secdownload_set_defaults;
	p->cleanup     = mod_secdownload_free;

	p->data        = NULL;

	return 0;
}
