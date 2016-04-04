#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include "response.h"
#include "stat_cache.h"
#include "stream.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

/**
 * this is a dirlisting for a lighttpd plugin
 */

#ifdef HAVE_ATTR_ATTRIBUTES_H
#include <attr/attributes.h>
#endif

#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#endif

#include "version.h"

/* plugin config for all request/connections */

typedef struct {
#ifdef HAVE_PCRE_H
	pcre *regex;
#endif
	buffer *string;
} excludes;

typedef struct {
	excludes **ptr;

	size_t used;
	size_t size;
} excludes_buffer;

typedef struct {
	unsigned short dir_listing;
	unsigned short hide_dot_files;
	unsigned short show_readme;
	unsigned short hide_readme_file;
	unsigned short encode_readme;
	unsigned short show_header;
	unsigned short hide_header_file;
	unsigned short encode_header;
	unsigned short auto_layout;

	excludes_buffer *excludes;

	buffer *external_css;
	buffer *encoding;
	buffer *set_footer;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *tmp_buf;
	buffer *content_charset;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

static excludes_buffer *excludes_buffer_init(void) {
	excludes_buffer *exb;

	exb = calloc(1, sizeof(*exb));

	return exb;
}

static int excludes_buffer_append(excludes_buffer *exb, buffer *string) {
#ifdef HAVE_PCRE_H
	size_t i;
	const char *errptr;
	int erroff;

	if (!string) return -1;

	if (exb->size == 0) {
		exb->size = 4;
		exb->used = 0;

		exb->ptr = malloc(exb->size * sizeof(*exb->ptr));

		for(i = 0; i < exb->size ; i++) {
			exb->ptr[i] = calloc(1, sizeof(**exb->ptr));
		}
	} else if (exb->used == exb->size) {
		exb->size += 4;

		exb->ptr = realloc(exb->ptr, exb->size * sizeof(*exb->ptr));

		for(i = exb->used; i < exb->size; i++) {
			exb->ptr[i] = calloc(1, sizeof(**exb->ptr));
		}
	}


	if (NULL == (exb->ptr[exb->used]->regex = pcre_compile(string->ptr, 0,
						    &errptr, &erroff, NULL))) {
		return -1;
	}

	exb->ptr[exb->used]->string = buffer_init();
	buffer_copy_buffer(exb->ptr[exb->used]->string, string);

	exb->used++;

	return 0;
#else
	UNUSED(exb);
	UNUSED(string);

	return -1;
#endif
}

static void excludes_buffer_free(excludes_buffer *exb) {
#ifdef HAVE_PCRE_H
	size_t i;

	for (i = 0; i < exb->size; i++) {
		if (exb->ptr[i]->regex) pcre_free(exb->ptr[i]->regex);
		if (exb->ptr[i]->string) buffer_free(exb->ptr[i]->string);
		free(exb->ptr[i]);
	}

	if (exb->ptr) free(exb->ptr);
#endif

	free(exb);
}

/* init the plugin data */
INIT_FUNC(mod_dirlisting_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->tmp_buf = buffer_init();
	p->content_charset = buffer_init();

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_dirlisting_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (!s) continue;

			excludes_buffer_free(s->excludes);
			buffer_free(s->external_css);
			buffer_free(s->encoding);
			buffer_free(s->set_footer);

			free(s);
		}
		free(p->config_storage);
	}

	buffer_free(p->tmp_buf);
	buffer_free(p->content_charset);

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

#define CONFIG_EXCLUDE          "dir-listing.exclude"
#define CONFIG_ACTIVATE         "dir-listing.activate"
#define CONFIG_HIDE_DOTFILES    "dir-listing.hide-dotfiles"
#define CONFIG_EXTERNAL_CSS     "dir-listing.external-css"
#define CONFIG_ENCODING         "dir-listing.encoding"
#define CONFIG_SHOW_README      "dir-listing.show-readme"
#define CONFIG_HIDE_README_FILE "dir-listing.hide-readme-file"
#define CONFIG_SHOW_HEADER      "dir-listing.show-header"
#define CONFIG_HIDE_HEADER_FILE "dir-listing.hide-header-file"
#define CONFIG_DIR_LISTING      "server.dir-listing"
#define CONFIG_SET_FOOTER       "dir-listing.set-footer"
#define CONFIG_ENCODE_README    "dir-listing.encode-readme"
#define CONFIG_ENCODE_HEADER    "dir-listing.encode-header"
#define CONFIG_AUTO_LAYOUT      "dir-listing.auto-layout"


SETDEFAULTS_FUNC(mod_dirlisting_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ CONFIG_EXCLUDE,          NULL, T_CONFIG_LOCAL, T_CONFIG_SCOPE_CONNECTION },   /* 0 */
		{ CONFIG_ACTIVATE,         NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 1 */
		{ CONFIG_HIDE_DOTFILES,    NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 2 */
		{ CONFIG_EXTERNAL_CSS,     NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 3 */
		{ CONFIG_ENCODING,         NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 4 */
		{ CONFIG_SHOW_README,      NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 5 */
		{ CONFIG_HIDE_README_FILE, NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 6 */
		{ CONFIG_SHOW_HEADER,      NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 7 */
		{ CONFIG_HIDE_HEADER_FILE, NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 8 */
		{ CONFIG_DIR_LISTING,      NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 9 */
		{ CONFIG_SET_FOOTER,       NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 10 */
		{ CONFIG_ENCODE_README,    NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 11 */
		{ CONFIG_ENCODE_HEADER,    NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 12 */
		{ CONFIG_AUTO_LAYOUT,      NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 13 */

		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;
		data_unset *du_excludes;

		s = calloc(1, sizeof(plugin_config));
		s->excludes = excludes_buffer_init();
		s->dir_listing = 0;
		s->external_css = buffer_init();
		s->hide_dot_files = 0;
		s->show_readme = 0;
		s->hide_readme_file = 0;
		s->show_header = 0;
		s->hide_header_file = 0;
		s->encode_readme = 1;
		s->encode_header = 1;
		s->auto_layout = 1;

		s->encoding = buffer_init();
		s->set_footer = buffer_init();

		cv[0].destination = s->excludes;
		cv[1].destination = &(s->dir_listing);
		cv[2].destination = &(s->hide_dot_files);
		cv[3].destination = s->external_css;
		cv[4].destination = s->encoding;
		cv[5].destination = &(s->show_readme);
		cv[6].destination = &(s->hide_readme_file);
		cv[7].destination = &(s->show_header);
		cv[8].destination = &(s->hide_header_file);
		cv[9].destination = &(s->dir_listing); /* old name */
		cv[10].destination = s->set_footer;
		cv[11].destination = &(s->encode_readme);
		cv[12].destination = &(s->encode_header);
		cv[13].destination = &(s->auto_layout);

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}

		if (NULL != (du_excludes = array_get_element(config->value, CONFIG_EXCLUDE))) {
			array *excludes_list;
			size_t j;

			if (du_excludes->type != TYPE_ARRAY) {
				log_error_write(srv, __FILE__, __LINE__, "sss",
					"unexpected type for key: ", CONFIG_EXCLUDE, "array of strings");
				return HANDLER_ERROR;
			}

			excludes_list = ((data_array*)du_excludes)->value;

#ifndef HAVE_PCRE_H
			if (excludes_list->used > 0) {
				log_error_write(srv, __FILE__, __LINE__, "sss",
					"pcre support is missing for: ", CONFIG_EXCLUDE, ", please install libpcre and the headers");
				return HANDLER_ERROR;
			}
#else
			for (j = 0; j < excludes_list->used; j++) {
				data_unset *du_exclude = excludes_list->data[j];

				if (du_exclude->type != TYPE_STRING) {
					log_error_write(srv, __FILE__, __LINE__, "sssbs",
						"unexpected type for key: ", CONFIG_EXCLUDE, "[",
						du_exclude->key, "](string)");
					return HANDLER_ERROR;
				}

				if (0 != excludes_buffer_append(s->excludes, ((data_string*)(du_exclude))->value)) {
					log_error_write(srv, __FILE__, __LINE__, "sb",
						"pcre-compile failed for", ((data_string*)(du_exclude))->value);
					return HANDLER_ERROR;
				}
			}
#endif
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_dirlisting_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(dir_listing);
	PATCH(external_css);
	PATCH(hide_dot_files);
	PATCH(encoding);
	PATCH(show_readme);
	PATCH(hide_readme_file);
	PATCH(show_header);
	PATCH(hide_header_file);
	PATCH(excludes);
	PATCH(set_footer);
	PATCH(encode_readme);
	PATCH(encode_header);
	PATCH(auto_layout);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_ACTIVATE)) ||
			    buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_DIR_LISTING))) {
				PATCH(dir_listing);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_HIDE_DOTFILES))) {
				PATCH(hide_dot_files);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_EXTERNAL_CSS))) {
				PATCH(external_css);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_ENCODING))) {
				PATCH(encoding);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_SHOW_README))) {
				PATCH(show_readme);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_HIDE_README_FILE))) {
				PATCH(hide_readme_file);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_SHOW_HEADER))) {
				PATCH(show_header);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_HIDE_HEADER_FILE))) {
				PATCH(hide_header_file);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_SET_FOOTER))) {
				PATCH(set_footer);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_EXCLUDE))) {
				PATCH(excludes);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_ENCODE_README))) {
				PATCH(encode_readme);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_ENCODE_HEADER))) {
				PATCH(encode_header);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(CONFIG_AUTO_LAYOUT))) {
				PATCH(auto_layout);
			}
		}
	}

	return 0;
}
#undef PATCH

typedef struct {
	size_t  namelen;
	time_t  mtime;
	off_t   size;
} dirls_entry_t;

typedef struct {
	dirls_entry_t **ent;
	size_t used;
	size_t size;
} dirls_list_t;

#define DIRLIST_ENT_NAME(ent)	((char*)(ent) + sizeof(dirls_entry_t))
#define DIRLIST_BLOB_SIZE		16

/* simple combsort algorithm */
static void http_dirls_sort(dirls_entry_t **ent, int num) {
	int gap = num;
	int i, j;
	int swapped;
	dirls_entry_t *tmp;

	do {
		gap = (gap * 10) / 13;
		if (gap == 9 || gap == 10)
			gap = 11;
		if (gap < 1)
			gap = 1;
		swapped = 0;

		for (i = 0; i < num - gap; i++) {
			j = i + gap;
			if (strcmp(DIRLIST_ENT_NAME(ent[i]), DIRLIST_ENT_NAME(ent[j])) > 0) {
				tmp = ent[i];
				ent[i] = ent[j];
				ent[j] = tmp;
				swapped = 1;
			}
		}

	} while (gap > 1 || swapped);
}

/* buffer must be able to hold "999.9K"
 * conversion is simple but not perfect
 */
static int http_list_directory_sizefmt(char *buf, off_t size) {
	const char unit[] = "KMGTPE";	/* Kilo, Mega, Tera, Peta, Exa */
	const char *u = unit - 1;		/* u will always increment at least once */
	int remain;
	char *out = buf;

	if (size < 100)
		size += 99;
	if (size < 100)
		size = 0;

	while (1) {
		remain = (int) size & 1023;
		size >>= 10;
		u++;
		if ((size & (~0 ^ 1023)) == 0)
			break;
	}

	remain /= 100;
	if (remain > 9)
		remain = 9;
	if (size > 999) {
		size   = 0;
		remain = 9;
		u++;
	}

	li_itostr(out, size);
	out += strlen(out);
	out[0] = '.';
	out[1] = remain + '0';
	out[2] = *u;
	out[3] = '\0';

	return (out + 3 - buf);
}

static void http_list_directory_header(server *srv, connection *con, plugin_data *p, buffer *out) {
	UNUSED(srv);

	if (p->conf.auto_layout) {
		buffer_append_string_len(out, CONST_STR_LEN(
			"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
			"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">\n"
			"<head>\n"
			"<title>Index of "
		));
		buffer_append_string_encoded(out, CONST_BUF_LEN(con->uri.path), ENCODING_MINIMAL_XML);
		buffer_append_string_len(out, CONST_STR_LEN("</title>\n"));

		if (!buffer_string_is_empty(p->conf.external_css)) {
			buffer_append_string_len(out, CONST_STR_LEN("<link rel=\"stylesheet\" type=\"text/css\" href=\""));
			buffer_append_string_buffer(out, p->conf.external_css);
			buffer_append_string_len(out, CONST_STR_LEN("\" />\n"));
		} else {
			buffer_append_string_len(out, CONST_STR_LEN(
				"<style type=\"text/css\">\n"
				"a, a:active {text-decoration: none; color: blue;}\n"
				"a:visited {color: #48468F;}\n"
				"a:hover, a:focus {text-decoration: underline; color: red;}\n"
				"body {background-color: #F5F5F5;}\n"
				"h2 {margin-bottom: 12px;}\n"
				"table {margin-left: 12px;}\n"
				"th, td {"
				" font: 90% monospace;"
				" text-align: left;"
				"}\n"
				"th {"
				" font-weight: bold;"
				" padding-right: 14px;"
				" padding-bottom: 3px;"
				"}\n"
				"td {padding-right: 14px;}\n"
				"td.s, th.s {text-align: right;}\n"
				"div.list {"
				" background-color: white;"
				" border-top: 1px solid #646464;"
				" border-bottom: 1px solid #646464;"
				" padding-top: 10px;"
				" padding-bottom: 14px;"
				"}\n"
				"div.foot {"
				" font: 90% monospace;"
				" color: #787878;"
				" padding-top: 4px;"
				"}\n"
				"</style>\n"
			));
		}

		buffer_append_string_len(out, CONST_STR_LEN("</head>\n<body>\n"));
	}

	/* HEADER.txt */
	if (p->conf.show_header) {
		stream s;
		/* if we have a HEADER file, display it in <pre class="header"></pre> */

		buffer_copy_buffer(p->tmp_buf, con->physical.path);
		buffer_append_slash(p->tmp_buf);
		buffer_append_string_len(p->tmp_buf, CONST_STR_LEN("HEADER.txt"));

		if (-1 != stream_open(&s, p->tmp_buf)) {
			if (p->conf.encode_header) {
				buffer_append_string_len(out, CONST_STR_LEN("<pre class=\"header\">"));
				buffer_append_string_encoded(out, s.start, s.size, ENCODING_MINIMAL_XML);
				buffer_append_string_len(out, CONST_STR_LEN("</pre>"));
			} else {
				buffer_append_string_len(out, s.start, s.size);
			}
		}
		stream_close(&s);
	}

	buffer_append_string_len(out, CONST_STR_LEN("<h2>Index of "));
	buffer_append_string_encoded(out, CONST_BUF_LEN(con->uri.path), ENCODING_MINIMAL_XML);
	buffer_append_string_len(out, CONST_STR_LEN(
		"</h2>\n"
		"<div class=\"list\">\n"
		"<table summary=\"Directory Listing\" cellpadding=\"0\" cellspacing=\"0\">\n"
		"<thead>"
		"<tr>"
			"<th class=\"n\">Name</th>"
			"<th class=\"m\">Last Modified</th>"
			"<th class=\"s\">Size</th>"
			"<th class=\"t\">Type</th>"
		"</tr>"
		"</thead>\n"
		"<tbody>\n"
		"<tr>"
			"<td class=\"n\"><a href=\"../\">Parent Directory</a>/</td>"
			"<td class=\"m\">&nbsp;</td>"
			"<td class=\"s\">- &nbsp;</td>"
			"<td class=\"t\">Directory</td>"
		"</tr>\n"
	));
}

static void http_list_directory_footer(server *srv, connection *con, plugin_data *p, buffer *out) {
	UNUSED(srv);

	buffer_append_string_len(out, CONST_STR_LEN(
		"</tbody>\n"
		"</table>\n"
		"</div>\n"
	));

	if (p->conf.show_readme) {
		stream s;
		/* if we have a README file, display it in <pre class="readme"></pre> */

		buffer_copy_buffer(p->tmp_buf,  con->physical.path);
		buffer_append_slash(p->tmp_buf);
		buffer_append_string_len(p->tmp_buf, CONST_STR_LEN("README.txt"));

		if (-1 != stream_open(&s, p->tmp_buf)) {
			if (p->conf.encode_readme) {
				buffer_append_string_len(out, CONST_STR_LEN("<pre class=\"readme\">"));
				buffer_append_string_encoded(out, s.start, s.size, ENCODING_MINIMAL_XML);
				buffer_append_string_len(out, CONST_STR_LEN("</pre>"));
			} else {
				buffer_append_string_len(out, s.start, s.size);
			}
		}
		stream_close(&s);
	}

	if(p->conf.auto_layout) {
		buffer_append_string_len(out, CONST_STR_LEN(
			"<div class=\"foot\">"
		));

		if (!buffer_string_is_empty(p->conf.set_footer)) {
			buffer_append_string_buffer(out, p->conf.set_footer);
		} else if (buffer_is_empty(con->conf.server_tag)) {
			buffer_append_string_len(out, CONST_STR_LEN(PACKAGE_DESC));
		} else {
			buffer_append_string_buffer(out, con->conf.server_tag);
		}

		buffer_append_string_len(out, CONST_STR_LEN(
			"</div>\n"
			"</body>\n"
			"</html>\n"
		));
	}
}

static int http_list_directory(server *srv, connection *con, plugin_data *p, buffer *dir) {
	DIR *dp;
	buffer *out;
	struct dirent *dent;
	struct stat st;
	char *path, *path_file;
	size_t i;
	int hide_dotfiles = p->conf.hide_dot_files;
	dirls_list_t dirs, files, *list;
	dirls_entry_t *tmp;
	char sizebuf[sizeof("999.9K")];
	char datebuf[sizeof("2005-Jan-01 22:23:24")];
	size_t k;
	const char *content_type;
	long name_max;
#if defined(HAVE_XATTR) || defined(HAVE_EXTATTR)
	char attrval[128];
	int attrlen;
#endif
#ifdef HAVE_LOCALTIME_R
	struct tm tm;
#endif

	if (buffer_string_is_empty(dir)) return -1;

	i = buffer_string_length(dir);

#ifdef HAVE_PATHCONF
	if (0 >= (name_max = pathconf(dir->ptr, _PC_NAME_MAX))) {
		/* some broken fs (fuse) return 0 instead of -1 */
#ifdef NAME_MAX
		name_max = NAME_MAX;
#else
		name_max = 255; /* stupid default */
#endif
	}
#elif defined __WIN32
	name_max = FILENAME_MAX;
#else
	name_max = NAME_MAX;
#endif

	path = malloc(buffer_string_length(dir) + name_max + 1);
	force_assert(NULL != path);
	strcpy(path, dir->ptr);
	path_file = path + i;

	if (NULL == (dp = opendir(path))) {
		log_error_write(srv, __FILE__, __LINE__, "sbs",
			"opendir failed:", dir, strerror(errno));

		free(path);
		return -1;
	}

	dirs.ent   = (dirls_entry_t**) malloc(sizeof(dirls_entry_t*) * DIRLIST_BLOB_SIZE);
	force_assert(dirs.ent);
	dirs.size  = DIRLIST_BLOB_SIZE;
	dirs.used  = 0;
	files.ent  = (dirls_entry_t**) malloc(sizeof(dirls_entry_t*) * DIRLIST_BLOB_SIZE);
	force_assert(files.ent);
	files.size = DIRLIST_BLOB_SIZE;
	files.used = 0;

	while ((dent = readdir(dp)) != NULL) {
		unsigned short exclude_match = 0;

		if (dent->d_name[0] == '.') {
			if (hide_dotfiles)
				continue;
			if (dent->d_name[1] == '\0')
				continue;
			if (dent->d_name[1] == '.' && dent->d_name[2] == '\0')
				continue;
		}

		if (p->conf.hide_readme_file) {
			if (strcmp(dent->d_name, "README.txt") == 0)
				continue;
		}
		if (p->conf.hide_header_file) {
			if (strcmp(dent->d_name, "HEADER.txt") == 0)
				continue;
		}

		/* compare d_name against excludes array
		 * elements, skipping any that match.
		 */
#ifdef HAVE_PCRE_H
		for(i = 0; i < p->conf.excludes->used; i++) {
			int n;
#define N 10
			int ovec[N * 3];
			pcre *regex = p->conf.excludes->ptr[i]->regex;

			if ((n = pcre_exec(regex, NULL, dent->d_name,
				    strlen(dent->d_name), 0, 0, ovec, 3 * N)) < 0) {
				if (n != PCRE_ERROR_NOMATCH) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
						"execution error while matching:", n);

					/* aborting would require a lot of manual cleanup here.
					 * skip instead (to not leak names that break pcre matching)
					 */
					exclude_match = 1;
					break;
				}
			}
			else {
				exclude_match = 1;
				break;
			}
		}

		if (exclude_match) {
			continue;
		}
#endif

		i = strlen(dent->d_name);

		/* NOTE: the manual says, d_name is never more than NAME_MAX
		 *       so this should actually not be a buffer-overflow-risk
		 */
		if (i > (size_t)name_max) continue;

		memcpy(path_file, dent->d_name, i + 1);
		if (stat(path, &st) != 0)
			continue;

		list = &files;
		if (S_ISDIR(st.st_mode))
			list = &dirs;

		if (list->used == list->size) {
			list->size += DIRLIST_BLOB_SIZE;
			list->ent   = (dirls_entry_t**) realloc(list->ent, sizeof(dirls_entry_t*) * list->size);
			force_assert(list->ent);
		}

		tmp = (dirls_entry_t*) malloc(sizeof(dirls_entry_t) + 1 + i);
		tmp->mtime = st.st_mtime;
		tmp->size  = st.st_size;
		tmp->namelen = i;
		memcpy(DIRLIST_ENT_NAME(tmp), dent->d_name, i + 1);

		list->ent[list->used++] = tmp;
	}
	closedir(dp);

	if (dirs.used) http_dirls_sort(dirs.ent, dirs.used);

	if (files.used) http_dirls_sort(files.ent, files.used);

	out = buffer_init();
	buffer_copy_string_len(out, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\""));
	if (buffer_string_is_empty(p->conf.encoding)) {
		buffer_append_string_len(out, CONST_STR_LEN("iso-8859-1"));
	} else {
		buffer_append_string_buffer(out, p->conf.encoding);
	}
	buffer_append_string_len(out, CONST_STR_LEN("\"?>\n"));
	http_list_directory_header(srv, con, p, out);

	/* directories */
	for (i = 0; i < dirs.used; i++) {
		tmp = dirs.ent[i];

#ifdef HAVE_LOCALTIME_R
		localtime_r(&(tmp->mtime), &tm);
		strftime(datebuf, sizeof(datebuf), "%Y-%b-%d %H:%M:%S", &tm);
#else
		strftime(datebuf, sizeof(datebuf), "%Y-%b-%d %H:%M:%S", localtime(&(tmp->mtime)));
#endif

		buffer_append_string_len(out, CONST_STR_LEN("<tr><td class=\"n\"><a href=\""));
		buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_REL_URI_PART);
		buffer_append_string_len(out, CONST_STR_LEN("/\">"));
		buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);
		buffer_append_string_len(out, CONST_STR_LEN("</a>/</td><td class=\"m\">"));
		buffer_append_string_len(out, datebuf, sizeof(datebuf) - 1);
		buffer_append_string_len(out, CONST_STR_LEN("</td><td class=\"s\">- &nbsp;</td><td class=\"t\">Directory</td></tr>\n"));

		free(tmp);
	}

	/* files */
	for (i = 0; i < files.used; i++) {
		tmp = files.ent[i];

		content_type = NULL;
#if defined(HAVE_XATTR)
		if (con->conf.use_xattr) {
			memcpy(path_file, DIRLIST_ENT_NAME(tmp), tmp->namelen + 1);
			attrlen = sizeof(attrval) - 1;
			if (attr_get(path, "Content-Type", attrval, &attrlen, 0) == 0) {
				attrval[attrlen] = '\0';
				content_type = attrval;
			}
		}
#elif defined(HAVE_EXTATTR)
		if (con->conf.use_xattr) {
			memcpy(path_file, DIRLIST_ENT_NAME(tmp), tmp->namelen + 1);
			if(-1 != (attrlen = extattr_get_file(path, EXTATTR_NAMESPACE_USER, "Content-Type", attrval, sizeof(attrval)-1))) {
				attrval[attrlen] = '\0';
				content_type = attrval;
			}
		}
#endif

		if (content_type == NULL) {
			content_type = "application/octet-stream";
			for (k = 0; k < con->conf.mimetypes->used; k++) {
				data_string *ds = (data_string *)con->conf.mimetypes->data[k];
				size_t ct_len;

				if (buffer_is_empty(ds->key))
					continue;

				ct_len = buffer_string_length(ds->key);
				if (tmp->namelen < ct_len)
					continue;

				if (0 == strncasecmp(DIRLIST_ENT_NAME(tmp) + tmp->namelen - ct_len, ds->key->ptr, ct_len)) {
					content_type = ds->value->ptr;
					break;
				}
			}
		}

#ifdef HAVE_LOCALTIME_R
		localtime_r(&(tmp->mtime), &tm);
		strftime(datebuf, sizeof(datebuf), "%Y-%b-%d %H:%M:%S", &tm);
#else
		strftime(datebuf, sizeof(datebuf), "%Y-%b-%d %H:%M:%S", localtime(&(tmp->mtime)));
#endif
		http_list_directory_sizefmt(sizebuf, tmp->size);

		buffer_append_string_len(out, CONST_STR_LEN("<tr><td class=\"n\"><a href=\""));
		buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_REL_URI_PART);
		buffer_append_string_len(out, CONST_STR_LEN("\">"));
		buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);
		buffer_append_string_len(out, CONST_STR_LEN("</a></td><td class=\"m\">"));
		buffer_append_string_len(out, datebuf, sizeof(datebuf) - 1);
		buffer_append_string_len(out, CONST_STR_LEN("</td><td class=\"s\">"));
		buffer_append_string(out, sizebuf);
		buffer_append_string_len(out, CONST_STR_LEN("</td><td class=\"t\">"));
		buffer_append_string(out, content_type);
		buffer_append_string_len(out, CONST_STR_LEN("</td></tr>\n"));

		free(tmp);
	}

	free(files.ent);
	free(dirs.ent);
	free(path);

	http_list_directory_footer(srv, con, p, out);

	/* Insert possible charset to Content-Type */
	if (buffer_string_is_empty(p->conf.encoding)) {
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html"));
	} else {
		buffer_copy_string_len(p->content_charset, CONST_STR_LEN("text/html; charset="));
		buffer_append_string_buffer(p->content_charset, p->conf.encoding);
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(p->content_charset));
	}

	con->file_finished = 1;
	chunkqueue_append_buffer(con->write_queue, out);
	buffer_free(out);

	return 0;
}



URIHANDLER_FUNC(mod_dirlisting_subrequest) {
	plugin_data *p = p_d;
	stat_cache_entry *sce = NULL;

	UNUSED(srv);

	/* we only handle GET, POST and HEAD */
	switch(con->request.http_method) {
	case HTTP_METHOD_GET:
	case HTTP_METHOD_POST:
	case HTTP_METHOD_HEAD:
		break;
	default:
		return HANDLER_GO_ON;
	}

	if (con->mode != DIRECT) return HANDLER_GO_ON;

	if (buffer_is_empty(con->physical.path)) return HANDLER_GO_ON;
	if (buffer_is_empty(con->uri.path)) return HANDLER_GO_ON;
	if (con->uri.path->ptr[buffer_string_length(con->uri.path) - 1] != '/') return HANDLER_GO_ON;

	mod_dirlisting_patch_connection(srv, con, p);

	if (!p->conf.dir_listing) return HANDLER_GO_ON;

	if (con->conf.log_request_handling) {
		log_error_write(srv, __FILE__, __LINE__,  "s",  "-- handling the request as Dir-Listing");
		log_error_write(srv, __FILE__, __LINE__,  "sb", "URI          :", con->uri.path);
	}

	if (HANDLER_ERROR == stat_cache_get_entry(srv, con, con->physical.path, &sce)) {
		log_error_write(srv, __FILE__, __LINE__,  "SB", "stat_cache_get_entry failed: ", con->physical.path);
		SEGFAULT();
	}

	if (!S_ISDIR(sce->st.st_mode)) return HANDLER_GO_ON;

	if (http_list_directory(srv, con, p, con->physical.path)) {
		/* dirlisting failed */
		con->http_status = 403;
	}

	buffer_reset(con->physical.path);

	/* not found */
	return HANDLER_FINISHED;
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_dirlisting_plugin_init(plugin *p);
int mod_dirlisting_plugin_init(plugin *p) {
	
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("dirlisting");

	p->init        = mod_dirlisting_init;
	p->handle_subrequest_start  = mod_dirlisting_subrequest;
	p->set_defaults  = mod_dirlisting_set_defaults;
	p->cleanup     = mod_dirlisting_free;

	p->data        = NULL;

	return 0;
}
