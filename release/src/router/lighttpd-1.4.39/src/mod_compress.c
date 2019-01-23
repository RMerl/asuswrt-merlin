#include "base.h"
#include "log.h"
#include "buffer.h"
#include "response.h"
#include "stat_cache.h"

#include "plugin.h"

#include "crc32.h"
#include "etag.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#if defined HAVE_ZLIB_H && defined HAVE_LIBZ
# define USE_ZLIB
# include <zlib.h>
#endif

#if defined HAVE_BZLIB_H && defined HAVE_LIBBZ2
# define USE_BZ2LIB
/* we don't need stdio interface */
# define BZ_NO_STDIO
# include <bzlib.h>
#endif

#include "sys-mmap.h"

/* request: accept-encoding */
#define HTTP_ACCEPT_ENCODING_IDENTITY BV(0)
#define HTTP_ACCEPT_ENCODING_GZIP     BV(1)
#define HTTP_ACCEPT_ENCODING_DEFLATE  BV(2)
#define HTTP_ACCEPT_ENCODING_COMPRESS BV(3)
#define HTTP_ACCEPT_ENCODING_BZIP2    BV(4)
#define HTTP_ACCEPT_ENCODING_X_GZIP   BV(5)
#define HTTP_ACCEPT_ENCODING_X_BZIP2  BV(6)

#ifdef __WIN32
# define mkdir(x,y) mkdir(x)
#endif

typedef struct {
	buffer *compress_cache_dir;
	array  *compress;
	off_t   compress_max_filesize; /** max filesize in kb */
	int     allowed_encodings;
} plugin_config;

typedef struct {
	PLUGIN_DATA;
	buffer *ofn;
	buffer *b;

	plugin_config **config_storage;
	plugin_config conf;
} plugin_data;

INIT_FUNC(mod_compress_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->ofn = buffer_init();
	p->b = buffer_init();

	return p;
}

FREE_FUNC(mod_compress_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	buffer_free(p->ofn);
	buffer_free(p->b);

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			array_free(s->compress);
			buffer_free(s->compress_cache_dir);

			free(s);
		}
		free(p->config_storage);
	}


	free(p);

	return HANDLER_GO_ON;
}

/* 0 on success, -1 for error */
static int mkdir_recursive(char *dir) {
	char *p = dir;

	if (!dir || !dir[0])
		return 0;

	while ((p = strchr(p + 1, '/')) != NULL) {

		*p = '\0';
		if ((mkdir(dir, 0700) != 0) && (errno != EEXIST)) {
			*p = '/';
			return -1;
		}

		*p++ = '/';
		if (!*p) return 0; /* Ignore trailing slash */
	}

	return (mkdir(dir, 0700) != 0) && (errno != EEXIST) ? -1 : 0;
}

/* 0 on success, -1 for error */
static int mkdir_for_file(char *filename) {
	char *p = filename;

	if (!filename || !filename[0])
		return -1;

	while ((p = strchr(p + 1, '/')) != NULL) {

		*p = '\0';
		if ((mkdir(filename, 0700) != 0) && (errno != EEXIST)) {
			*p = '/';
			return -1;
		}

		*p++ = '/';
		if (!*p) return -1; /* Unexpected trailing slash in filename */
	}

	return 0;
}

SETDEFAULTS_FUNC(mod_compress_setdefaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "compress.cache-dir",             NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
		{ "compress.filetype",              NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },
		{ "compress.max-filesize",          NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },
		{ "compress.allowed-encodings",     NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                             NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;
		array  *encodings_arr = array_init();

		s = calloc(1, sizeof(plugin_config));
		s->compress_cache_dir = buffer_init();
		s->compress = array_init();
		s->compress_max_filesize = 0;
		s->allowed_encodings = 0;

		cv[0].destination = s->compress_cache_dir;
		cv[1].destination = s->compress;
		cv[2].destination = &(s->compress_max_filesize);
		cv[3].destination = encodings_arr; /* temp array for allowed encodings list */

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}

		if (encodings_arr->used) {
			size_t j = 0;
			for (j = 0; j < encodings_arr->used; j++) {
				data_string *ds = (data_string *)encodings_arr->data[j];
#ifdef USE_ZLIB
				if (NULL != strstr(ds->value->ptr, "gzip"))
					s->allowed_encodings |= HTTP_ACCEPT_ENCODING_GZIP | HTTP_ACCEPT_ENCODING_X_GZIP;
				if (NULL != strstr(ds->value->ptr, "x-gzip"))
					s->allowed_encodings |= HTTP_ACCEPT_ENCODING_X_GZIP;
				if (NULL != strstr(ds->value->ptr, "deflate"))
					s->allowed_encodings |= HTTP_ACCEPT_ENCODING_DEFLATE;
				/*
				if (NULL != strstr(ds->value->ptr, "compress"))
					s->allowed_encodings |= HTTP_ACCEPT_ENCODING_COMPRESS;
				*/
#endif
#ifdef USE_BZ2LIB
				if (NULL != strstr(ds->value->ptr, "bzip2"))
					s->allowed_encodings |= HTTP_ACCEPT_ENCODING_BZIP2 | HTTP_ACCEPT_ENCODING_X_BZIP2;
				if (NULL != strstr(ds->value->ptr, "x-bzip2"))
					s->allowed_encodings |= HTTP_ACCEPT_ENCODING_X_BZIP2;
#endif
			}
		} else {
			/* default encodings */
			s->allowed_encodings = 0
#ifdef USE_ZLIB
				| HTTP_ACCEPT_ENCODING_GZIP | HTTP_ACCEPT_ENCODING_X_GZIP | HTTP_ACCEPT_ENCODING_DEFLATE
#endif
#ifdef USE_BZ2LIB
				| HTTP_ACCEPT_ENCODING_BZIP2 | HTTP_ACCEPT_ENCODING_X_BZIP2
#endif
				;
		}

		array_free(encodings_arr);

		if (!buffer_string_is_empty(s->compress_cache_dir)) {
			struct stat st;
			mkdir_recursive(s->compress_cache_dir->ptr);

			if (0 != stat(s->compress_cache_dir->ptr, &st)) {
				log_error_write(srv, __FILE__, __LINE__, "sbs", "can't stat compress.cache-dir",
						s->compress_cache_dir, strerror(errno));

				return HANDLER_ERROR;
			}
		}
	}

	return HANDLER_GO_ON;

}

#ifdef USE_ZLIB
static int deflate_file_to_buffer_gzip(server *srv, connection *con, plugin_data *p, char *start, off_t st_size, time_t mtime) {
	unsigned char *c;
	unsigned long crc;
	z_stream z;
	size_t outlen;

	UNUSED(srv);
	UNUSED(con);

	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if (Z_OK != deflateInit2(&z,
				 Z_DEFAULT_COMPRESSION,
				 Z_DEFLATED,
				 -MAX_WBITS,  /* supress zlib-header */
				 8,
				 Z_DEFAULT_STRATEGY)) {
		return -1;
	}

	z.next_in = (unsigned char *)start;
	z.avail_in = st_size;
	z.total_in = 0;


	buffer_string_prepare_copy(p->b, (z.avail_in * 1.1) + 12 + 18);

	/* write gzip header */

	c = (unsigned char *)p->b->ptr;
	c[0] = 0x1f;
	c[1] = 0x8b;
	c[2] = Z_DEFLATED;
	c[3] = 0; /* options */
	c[4] = (mtime >>  0) & 0xff;
	c[5] = (mtime >>  8) & 0xff;
	c[6] = (mtime >> 16) & 0xff;
	c[7] = (mtime >> 24) & 0xff;
	c[8] = 0x00; /* extra flags */
	c[9] = 0x03; /* UNIX */

	outlen = 10;
	z.next_out = (unsigned char *)p->b->ptr + outlen;
	z.avail_out = p->b->size - outlen - 9;
	z.total_out = 0;

	if (Z_STREAM_END != deflate(&z, Z_FINISH)) {
		deflateEnd(&z);
		return -1;
	}

	/* trailer */
	outlen += z.total_out;

	crc = generate_crc32c(start, st_size);

	c = (unsigned char *)p->b->ptr + outlen;

	c[0] = (crc >>  0) & 0xff;
	c[1] = (crc >>  8) & 0xff;
	c[2] = (crc >> 16) & 0xff;
	c[3] = (crc >> 24) & 0xff;
	c[4] = (z.total_in >>  0) & 0xff;
	c[5] = (z.total_in >>  8) & 0xff;
	c[6] = (z.total_in >> 16) & 0xff;
	c[7] = (z.total_in >> 24) & 0xff;
	outlen += 8;
	buffer_commit(p->b, outlen);

	if (Z_OK != deflateEnd(&z)) {
		return -1;
	}

	return 0;
}

static int deflate_file_to_buffer_deflate(server *srv, connection *con, plugin_data *p, unsigned char *start, off_t st_size) {
	z_stream z;

	UNUSED(srv);
	UNUSED(con);

	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if (Z_OK != deflateInit2(&z,
				 Z_DEFAULT_COMPRESSION,
				 Z_DEFLATED,
				 -MAX_WBITS,  /* supress zlib-header */
				 8,
				 Z_DEFAULT_STRATEGY)) {
		return -1;
	}

	z.next_in = start;
	z.avail_in = st_size;
	z.total_in = 0;

	buffer_string_prepare_copy(p->b, (z.avail_in * 1.1) + 12);

	z.next_out = (unsigned char *)p->b->ptr;
	z.avail_out = p->b->size - 1;
	z.total_out = 0;

	if (Z_STREAM_END != deflate(&z, Z_FINISH)) {
		deflateEnd(&z);
		return -1;
	}

	if (Z_OK != deflateEnd(&z)) {
		return -1;
	}

	/* trailer */
	buffer_commit(p->b, z.total_out);

	return 0;
}

#endif

#ifdef USE_BZ2LIB
static int deflate_file_to_buffer_bzip2(server *srv, connection *con, plugin_data *p, unsigned char *start, off_t st_size) {
	bz_stream bz;

	UNUSED(srv);
	UNUSED(con);

	bz.bzalloc = NULL;
	bz.bzfree = NULL;
	bz.opaque = NULL;

	if (BZ_OK != BZ2_bzCompressInit(&bz,
					9, /* blocksize = 900k */
					0, /* no output */
					0)) { /* workFactor: default */
		return -1;
	}

	bz.next_in = (char *)start;
	bz.avail_in = st_size;
	bz.total_in_lo32 = 0;
	bz.total_in_hi32 = 0;

	buffer_string_prepare_copy(p->b, (bz.avail_in * 1.1) + 12);

	bz.next_out = p->b->ptr;
	bz.avail_out = p->b->size - 1;
	bz.total_out_lo32 = 0;
	bz.total_out_hi32 = 0;

	if (BZ_STREAM_END != BZ2_bzCompress(&bz, BZ_FINISH)) {
		BZ2_bzCompressEnd(&bz);
		return -1;
	}

	if (BZ_OK != BZ2_bzCompressEnd(&bz)) {
		return -1;
	}

	/* file is too large for now */
	if (bz.total_out_hi32) return -1;

	/* trailer */
	buffer_commit(p->b, bz.total_out_lo32);

	return 0;
}
#endif

static int deflate_file_to_file(server *srv, connection *con, plugin_data *p, buffer *fn, stat_cache_entry *sce, int type) {
	int ifd, ofd;
	int ret;
	void *start;
	const char *filename = fn->ptr;
	ssize_t r;

	/* overflow */
	if ((off_t)(sce->st.st_size * 1.1) < sce->st.st_size) return -1;

	/* don't mmap files > 128Mb
	 *
	 * we could use a sliding window, but currently there is no need for it
	 */

	if (sce->st.st_size > 128 * 1024 * 1024) return -1;

	buffer_reset(p->ofn);
	buffer_copy_buffer(p->ofn, p->conf.compress_cache_dir);
	buffer_append_slash(p->ofn);

	if (0 == strncmp(con->physical.path->ptr, con->physical.doc_root->ptr, buffer_string_length(con->physical.doc_root))) {
		buffer_append_string(p->ofn, con->physical.path->ptr + buffer_string_length(con->physical.doc_root));
	} else {
		buffer_append_string_buffer(p->ofn, con->uri.path);
	}

	switch(type) {
	case HTTP_ACCEPT_ENCODING_GZIP:
	case HTTP_ACCEPT_ENCODING_X_GZIP:
		buffer_append_string_len(p->ofn, CONST_STR_LEN("-gzip-"));
		break;
	case HTTP_ACCEPT_ENCODING_DEFLATE:
		buffer_append_string_len(p->ofn, CONST_STR_LEN("-deflate-"));
		break;
	case HTTP_ACCEPT_ENCODING_BZIP2:
	case HTTP_ACCEPT_ENCODING_X_BZIP2:
		buffer_append_string_len(p->ofn, CONST_STR_LEN("-bzip2-"));
		break;
	default:
		log_error_write(srv, __FILE__, __LINE__, "sd", "unknown compression type", type);
		return -1;
	}

	buffer_append_string_buffer(p->ofn, sce->etag);

	if (-1 == mkdir_for_file(p->ofn->ptr)) {
		log_error_write(srv, __FILE__, __LINE__, "sb", "couldn't create directory for file", p->ofn);
		return -1;
	}

	if (-1 == (ofd = open(p->ofn->ptr, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0600))) {
		if (errno == EEXIST) {
			/* cache-entry exists */
#if 0
			log_error_write(srv, __FILE__, __LINE__, "bs", p->ofn, "compress-cache hit");
#endif
			buffer_copy_buffer(con->physical.path, p->ofn);

			return 0;
		}

		log_error_write(srv, __FILE__, __LINE__, "sbss", "creating cachefile", p->ofn, "failed", strerror(errno));

		return -1;
	}
#if 0
	log_error_write(srv, __FILE__, __LINE__, "bs", p->ofn, "compress-cache miss");
#endif
	if (-1 == (ifd = open(filename, O_RDONLY | O_BINARY))) {
		log_error_write(srv, __FILE__, __LINE__, "sbss", "opening plain-file", fn, "failed", strerror(errno));

		close(ofd);

		/* Remove the incomplete cache file, so that later hits aren't served from it */
		if (-1 == unlink(p->ofn->ptr)) {
			log_error_write(srv, __FILE__, __LINE__, "sbss", "unlinking incomplete cachefile", p->ofn, "failed:", strerror(errno));
		}

		return -1;
	}

#ifdef USE_MMAP
	if (MAP_FAILED == (start = mmap(NULL, sce->st.st_size, PROT_READ, MAP_SHARED, ifd, 0))) {
		log_error_write(srv, __FILE__, __LINE__, "sbss", "mmaping", fn, "failed", strerror(errno));

		close(ofd);
		close(ifd);

		/* Remove the incomplete cache file, so that later hits aren't served from it */
		if (-1 == unlink(p->ofn->ptr)) {
			log_error_write(srv, __FILE__, __LINE__, "sbss", "unlinking incomplete cachefile", p->ofn, "failed:", strerror(errno));
		}

		return -1;
	}
#else
	start = malloc(sce->st.st_size);
	if (NULL == start || sce->st.st_size != read(ifd, start, sce->st.st_size)) {
		log_error_write(srv, __FILE__, __LINE__, "sbss", "reading", fn, "failed", strerror(errno));

		close(ofd);
		close(ifd);
		free(start);

		/* Remove the incomplete cache file, so that later hits aren't served from it */
		if (-1 == unlink(p->ofn->ptr)) {
			log_error_write(srv, __FILE__, __LINE__, "sbss", "unlinking incomplete cachefile", p->ofn, "failed:", strerror(errno));
		}

		return -1;
	}
#endif

	ret = -1;
	switch(type) {
#ifdef USE_ZLIB
	case HTTP_ACCEPT_ENCODING_GZIP:
	case HTTP_ACCEPT_ENCODING_X_GZIP:
		ret = deflate_file_to_buffer_gzip(srv, con, p, start, sce->st.st_size, sce->st.st_mtime);
		break;
	case HTTP_ACCEPT_ENCODING_DEFLATE:
		ret = deflate_file_to_buffer_deflate(srv, con, p, start, sce->st.st_size);
		break;
#endif
#ifdef USE_BZ2LIB
	case HTTP_ACCEPT_ENCODING_BZIP2:
	case HTTP_ACCEPT_ENCODING_X_BZIP2:
		ret = deflate_file_to_buffer_bzip2(srv, con, p, start, sce->st.st_size);
		break;
#endif
	}

	if (ret == 0) {
		r = write(ofd, CONST_BUF_LEN(p->b));
		if (-1 == r) {
			log_error_write(srv, __FILE__, __LINE__, "sbss", "writing cachefile", p->ofn, "failed:", strerror(errno));
			ret = -1;
		} else if ((size_t)r != buffer_string_length(p->b)) {
			log_error_write(srv, __FILE__, __LINE__, "sbs", "writing cachefile", p->ofn, "failed: not enough bytes written");
			ret = -1;
		}
	}

#ifdef USE_MMAP
	munmap(start, sce->st.st_size);
#else
	free(start);
#endif

	close(ofd);
	close(ifd);

	if (ret != 0) {
		/* Remove the incomplete cache file, so that later hits aren't served from it */
		if (-1 == unlink(p->ofn->ptr)) {
			log_error_write(srv, __FILE__, __LINE__, "sbss", "unlinking incomplete cachefile", p->ofn, "failed:", strerror(errno));
		}

		return -1;
	}

	buffer_copy_buffer(con->physical.path, p->ofn);

	return 0;
}

static int deflate_file_to_buffer(server *srv, connection *con, plugin_data *p, buffer *fn, stat_cache_entry *sce, int type) {
	int ifd;
	int ret = -1;
	void *start;

	/* overflow */
	if ((off_t)(sce->st.st_size * 1.1) < sce->st.st_size) return -1;

	/* don't mmap files > 128M
	 *
	 * we could use a sliding window, but currently there is no need for it
	 */

	if (sce->st.st_size > 128 * 1024 * 1024) return -1;


	if (-1 == (ifd = open(fn->ptr, O_RDONLY | O_BINARY))) {
		log_error_write(srv, __FILE__, __LINE__, "sbss", "opening plain-file", fn, "failed", strerror(errno));

		return -1;
	}

#ifdef USE_MMAP
	if (MAP_FAILED == (start = mmap(NULL, sce->st.st_size, PROT_READ, MAP_SHARED, ifd, 0))) {
		log_error_write(srv, __FILE__, __LINE__, "sbss", "mmaping", fn, "failed", strerror(errno));

		close(ifd);
		return -1;
	}
#else
	start = malloc(sce->st.st_size);
	if (NULL == start || sce->st.st_size != read(ifd, start, sce->st.st_size)) {
		log_error_write(srv, __FILE__, __LINE__, "sbss", "reading", fn, "failed", strerror(errno));

		close(ifd);
		free(start);
		return -1;
	}
#endif

	switch(type) {
#ifdef USE_ZLIB
	case HTTP_ACCEPT_ENCODING_GZIP:
	case HTTP_ACCEPT_ENCODING_X_GZIP:
		ret = deflate_file_to_buffer_gzip(srv, con, p, start, sce->st.st_size, sce->st.st_mtime);
		break;
	case HTTP_ACCEPT_ENCODING_DEFLATE:
		ret = deflate_file_to_buffer_deflate(srv, con, p, start, sce->st.st_size);
		break;
#endif
#ifdef USE_BZ2LIB
	case HTTP_ACCEPT_ENCODING_BZIP2:
	case HTTP_ACCEPT_ENCODING_X_BZIP2:
		ret = deflate_file_to_buffer_bzip2(srv, con, p, start, sce->st.st_size);
		break;
#endif
	default:
		ret = -1;
		break;
	}

#ifdef USE_MMAP
	munmap(start, sce->st.st_size);
#else
	free(start);
#endif
	close(ifd);

	if (ret != 0) return -1;

	chunkqueue_reset(con->write_queue);
	chunkqueue_append_buffer(con->write_queue, p->b);

	buffer_reset(con->physical.path);

	con->file_finished = 1;
	con->file_started  = 1;

	return 0;
}


#define PATCH(x) \
	p->conf.x = s->x;
static int mod_compress_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(compress_cache_dir);
	PATCH(compress);
	PATCH(compress_max_filesize);
	PATCH(allowed_encodings);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("compress.cache-dir"))) {
				PATCH(compress_cache_dir);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("compress.filetype"))) {
				PATCH(compress);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("compress.max-filesize"))) {
				PATCH(compress_max_filesize);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("compress.allowed-encodings"))) {
				PATCH(allowed_encodings);
			}
		}
	}

	return 0;
}
#undef PATCH

static int mod_compress_contains_encoding(const char *headervalue, const char *encoding) {
	const char *m;
	for ( ;; ) {
		m = strstr(headervalue, encoding);
		if (NULL == m) return 0;
		if (m == headervalue || m[-1] == ' ' || m[-1] == ',') return 1;

		/* only partial match, search for next value */
		m = strchr(m, ',');
		if (NULL == m) return 0;
		headervalue = m + 1;
	}
}

PHYSICALPATH_FUNC(mod_compress_physical) {
	plugin_data *p = p_d;
	size_t m;
	off_t max_fsize;
	stat_cache_entry *sce = NULL;
	buffer *mtime = NULL;
	buffer *content_type;

	if (con->mode != DIRECT || con->http_status) return HANDLER_GO_ON;

	/* only GET and POST can get compressed */
	if (con->request.http_method != HTTP_METHOD_GET &&
	    con->request.http_method != HTTP_METHOD_POST) {
		return HANDLER_GO_ON;
	}

	if (buffer_string_is_empty(con->physical.path)) {
		return HANDLER_GO_ON;
	}

	mod_compress_patch_connection(srv, con, p);

	max_fsize = p->conf.compress_max_filesize;

	if (con->conf.log_request_handling) {
		log_error_write(srv, __FILE__, __LINE__,  "s",  "-- handling file as static file");
	}

	if (HANDLER_ERROR == stat_cache_get_entry(srv, con, con->physical.path, &sce)) {
		con->http_status = 403;

		log_error_write(srv, __FILE__, __LINE__, "sbsb",
				"not a regular file:", con->uri.path,
				"->", con->physical.path);

		return HANDLER_FINISHED;
	}

	/* we only handle regular files */
#ifdef HAVE_LSTAT
	if ((sce->is_symlink == 1) && !con->conf.follow_symlink) {
		return HANDLER_GO_ON;
	}
#endif
	if (!S_ISREG(sce->st.st_mode)) {
		return HANDLER_GO_ON;
	}

	/* don't compress files that are too large as we need to much time to handle them */
	if (max_fsize && (sce->st.st_size >> 10) > max_fsize) return HANDLER_GO_ON;

	/* don't try to compress files less than 128 bytes
	 *
	 * - extra overhead for compression
	 * - mmap() fails for st_size = 0 :)
	 */
	if (sce->st.st_size < 128) return HANDLER_GO_ON;

	/* check if mimetype is in compress-config */
	content_type = NULL;
	if (sce->content_type->ptr) {
		char *c;
		if ( (c = strchr(sce->content_type->ptr, ';')) != NULL) {
			content_type = srv->tmp_buf;
			buffer_copy_string_len(content_type, sce->content_type->ptr, c - sce->content_type->ptr);
		}
	}

	for (m = 0; m < p->conf.compress->used; m++) {
		data_string *compress_ds = (data_string *)p->conf.compress->data[m];

		if (!compress_ds) {
			log_error_write(srv, __FILE__, __LINE__, "sbb", "evil", con->physical.path, con->uri.path);

			return HANDLER_GO_ON;
		}

		if (buffer_is_equal(compress_ds->value, sce->content_type)
		    || (content_type && buffer_is_equal(compress_ds->value, content_type))) {
			/* mimetype found */
			data_string *ds;

			/* the response might change according to Accept-Encoding */
			response_header_insert(srv, con, CONST_STR_LEN("Vary"), CONST_STR_LEN("Accept-Encoding"));

			if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Accept-Encoding"))) {
				int accept_encoding = 0;
				char *value = ds->value->ptr;
				int matched_encodings = 0;
				int use_etag = sce->etag != NULL && sce->etag->ptr != NULL;

				/* get client side support encodings */
#ifdef USE_ZLIB
				if (mod_compress_contains_encoding(value, "gzip")) accept_encoding |= HTTP_ACCEPT_ENCODING_GZIP;
				if (mod_compress_contains_encoding(value, "x-gzip")) accept_encoding |= HTTP_ACCEPT_ENCODING_X_GZIP;
				if (mod_compress_contains_encoding(value, "deflate")) accept_encoding |= HTTP_ACCEPT_ENCODING_DEFLATE;
				if (mod_compress_contains_encoding(value, "compress")) accept_encoding |= HTTP_ACCEPT_ENCODING_COMPRESS;
#endif
#ifdef USE_BZ2LIB
				if (mod_compress_contains_encoding(value, "bzip2")) accept_encoding |= HTTP_ACCEPT_ENCODING_BZIP2;
				if (mod_compress_contains_encoding(value, "x-bzip2")) accept_encoding |= HTTP_ACCEPT_ENCODING_X_BZIP2;
#endif
				if (mod_compress_contains_encoding(value, "identity")) accept_encoding |= HTTP_ACCEPT_ENCODING_IDENTITY;

				/* find matching entries */
				matched_encodings = accept_encoding & p->conf.allowed_encodings;

				if (matched_encodings) {
					static const char dflt_gzip[] = "gzip";
					static const char dflt_x_gzip[] = "x-gzip";
					static const char dflt_deflate[] = "deflate";
					static const char dflt_bzip2[] = "bzip2";
					static const char dflt_x_bzip2[] = "x-bzip2";

					const char *compression_name = NULL;
					int compression_type = 0;

					mtime = strftime_cache_get(srv, sce->st.st_mtime);

					/* try matching original etag of uncompressed version */
					if (use_etag) {
						etag_mutate(con->physical.etag, sce->etag);
						if (HANDLER_FINISHED == http_response_handle_cachable(srv, con, mtime)) {
							response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(sce->content_type));
							response_header_overwrite(srv, con, CONST_STR_LEN("Last-Modified"), CONST_BUF_LEN(mtime));
							response_header_overwrite(srv, con, CONST_STR_LEN("ETag"), CONST_BUF_LEN(con->physical.etag));
							return HANDLER_FINISHED;
						}
					}

					/* select best matching encoding */
					if (matched_encodings & HTTP_ACCEPT_ENCODING_BZIP2) {
						compression_type = HTTP_ACCEPT_ENCODING_BZIP2;
						compression_name = dflt_bzip2;
					} else if (matched_encodings & HTTP_ACCEPT_ENCODING_X_BZIP2) {
						compression_type = HTTP_ACCEPT_ENCODING_X_BZIP2;
						compression_name = dflt_x_bzip2;
					} else if (matched_encodings & HTTP_ACCEPT_ENCODING_GZIP) {
						compression_type = HTTP_ACCEPT_ENCODING_GZIP;
						compression_name = dflt_gzip;
					} else if (matched_encodings & HTTP_ACCEPT_ENCODING_X_GZIP) {
						compression_type = HTTP_ACCEPT_ENCODING_X_GZIP;
						compression_name = dflt_x_gzip;
					} else {
						force_assert(matched_encodings & HTTP_ACCEPT_ENCODING_DEFLATE);
						compression_type = HTTP_ACCEPT_ENCODING_DEFLATE;
						compression_name = dflt_deflate;
					}

					if (use_etag) {
						/* try matching etag of compressed version */
						buffer_copy_buffer(srv->tmp_buf, sce->etag);
						buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN("-"));
						buffer_append_string(srv->tmp_buf, compression_name);
						etag_mutate(con->physical.etag, srv->tmp_buf);
					}

					if (HANDLER_FINISHED == http_response_handle_cachable(srv, con, mtime)) {
						response_header_overwrite(srv, con, CONST_STR_LEN("Content-Encoding"), compression_name, strlen(compression_name));
						response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(sce->content_type));
						response_header_overwrite(srv, con, CONST_STR_LEN("Last-Modified"), CONST_BUF_LEN(mtime));
						if (use_etag) {
							response_header_overwrite(srv, con, CONST_STR_LEN("ETag"), CONST_BUF_LEN(con->physical.etag));
						}
						return HANDLER_FINISHED;
					}

					/* deflate it */
					if (use_etag && !buffer_string_is_empty(p->conf.compress_cache_dir)) {
						if (0 != deflate_file_to_file(srv, con, p, con->physical.path, sce, compression_type))
							return HANDLER_GO_ON;
					} else {
						if (0 != deflate_file_to_buffer(srv, con, p, con->physical.path, sce, compression_type))
							return HANDLER_GO_ON;
					}
					response_header_overwrite(srv, con, CONST_STR_LEN("Content-Encoding"), compression_name, strlen(compression_name));
					response_header_overwrite(srv, con, CONST_STR_LEN("Last-Modified"), CONST_BUF_LEN(mtime));
					if (use_etag) {
						response_header_overwrite(srv, con, CONST_STR_LEN("ETag"), CONST_BUF_LEN(con->physical.etag));
					}
					response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(sce->content_type));
					/* let mod_staticfile handle the cached compressed files, physical path was modified */
					return (use_etag && !buffer_string_is_empty(p->conf.compress_cache_dir)) ? HANDLER_GO_ON : HANDLER_FINISHED;
				}
			}
		}
	}

	return HANDLER_GO_ON;
}
#ifndef APP_IPKG
int mod_compress_plugin_init(plugin *p);
int mod_compress_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("compress");

	p->init        = mod_compress_init;
	p->set_defaults = mod_compress_setdefaults;
	p->handle_subrequest_start  = mod_compress_physical;
	p->cleanup     = mod_compress_free;

	p->data        = NULL;

	return 0;
}
#else
int aicloud_mod_compress_plugin_init(plugin *p);
int aicloud_mod_compress_plugin_init(plugin *p) {
    p->version     = LIGHTTPD_VERSION_ID;
    p->name        = buffer_init_string("compress");

    p->init        = mod_compress_init;
    p->set_defaults = mod_compress_setdefaults;
    p->handle_subrequest_start  = mod_compress_physical;
    p->cleanup     = mod_compress_free;

    p->data        = NULL;

    return 0;
}
#endif
