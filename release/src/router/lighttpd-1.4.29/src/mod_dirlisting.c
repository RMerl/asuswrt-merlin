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


#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

#ifdef HAVE_ATTR_ATTRIBUTES_H
#include <attr/attributes.h>
#endif

#include "version.h"

#define DBE 1
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

#define DBE 0

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
	buffer_copy_string_buffer(exb->ptr[exb->used]->string, string);

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

static int parse_config_entry(server *srv, plugin_config *s, array *ca, const char *option) {
	data_unset *du;

	if (NULL != (du = array_get_element(ca, option))) {
		data_array *da;
		size_t j;

		if (du->type != TYPE_ARRAY) {
			log_error_write(srv, __FILE__, __LINE__, "sss",
				"unexpected type for key: ", option, "array of strings");

			return HANDLER_ERROR;
		}

		da = (data_array *)du;

		for (j = 0; j < da->value->used; j++) {
			if (da->value->data[j]->type != TYPE_STRING) {
				log_error_write(srv, __FILE__, __LINE__, "sssbs",
					"unexpected type for key: ", option, "[",
					da->value->data[j]->key, "](string)");

				return HANDLER_ERROR;
			}

			if (0 != excludes_buffer_append(s->excludes,
				    ((data_string *)(da->value->data[j]))->value)) {
#ifdef HAVE_PCRE_H
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"pcre-compile failed for", ((data_string *)(da->value->data[j]))->value);
#else
				log_error_write(srv, __FILE__, __LINE__, "s",
						"pcre support is missing, please install libpcre and the headers");
#endif
			}
		}
	}

	return 0;
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

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;
		array *ca;

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
		ca = ((data_config *)srv->config_context->data[i])->value;

		if (0 != config_insert_values_global(srv, ca, cv)) {
			return HANDLER_ERROR;
		}

		parse_config_entry(srv, s, ca, CONFIG_EXCLUDE);
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

	out   += LI_ltostr(out, size);
	out[0] = '.';
	out[1] = remain + '0';
	out[2] = *u;
	out[3] = '\0';

	return (out + 3 - buf);
}


void http_list_directory_header(server *srv, connection *con, plugin_data *p, buffer *out) 
{
	UNUSED(srv);

	//if (p->conf.auto_layout) 
	{
		buffer_append_string_len(out, CONST_STR_LEN(
			//"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
			//"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">\n"
			"<!DOCTYPE html>\n"
			"<html>\n"
			"<head>\n"
			"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n"
			"<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">\n"
			"<meta name=\"apple-mobile-web-app-status-bar-style\" content=\"black\">\n"
			"<link rel=\"apple-touch-icon\" href=\"/smb/css/appicon.png\">\n"
			"<link rel=\"apple-touch-startup-image\" href=\"/smb/css/startup.png\">\n"
			"<title>ASUS Cloud"
		));
		//buffer_append_string_encoded(out, CONST_BUF_LEN(con->uri.path), ENCODING_MINIMAL_XML);
		buffer_append_string_len(out, CONST_STR_LEN("</title>\n"));
 
		//if (p->conf.external_css->used > 1) {
		//	buffer_append_string_len(out, CONST_STR_LEN("<link rel=\"stylesheet\" type=\"text/css\" href=\""));
		//	buffer_append_string_buffer(out, p->conf.external_css);
		//	buffer_append_string_len(out, CONST_STR_LEN("\" />\n"));
		//} else 
		{
			//- Jerry modify
			buffer_append_string_len(out, CONST_STR_LEN(
				"<link rel='stylesheet' href='/smb/css/style.css' type='text/css'/>\n"				
				"<link rel='stylesheet' href='/smb/css/tree.css' type='text/css'/>\n"
				"<script type='text/javascript' src='/smb/js/tools.js'></script>\n"
				"<script type='text/javascript' src='/smb/js/davclient_tools.js'></script>\n"
				"<script type='text/javascript' src='/smb/js/smbdav-main.min.js'></script>\n"
			));
		}

		buffer_append_string_len(out, CONST_STR_LEN("</head>\n<body>\n"));
	}

	//- Tree Region
	buffer_append_string_len(out, CONST_STR_LEN("<div id='treeRegion'>\n"));
	buffer_append_string_len(out, CONST_STR_LEN("<table id='headerRegion' width='100%' border='0' cellspacing='3' cellpadding='0'>"));
	buffer_append_string_len(out, CONST_STR_LEN("<tr>"));
	buffer_append_string_len(out, CONST_STR_LEN("</tr>"));
	buffer_append_string_len(out, CONST_STR_LEN("</table>"));
	buffer_append_string_len(out, CONST_STR_LEN("<div id='tree' class='tree'></div>\n"));
	buffer_append_string_len(out, CONST_STR_LEN("<div id='ocButton' class='ocButton'></div>\n"));
	buffer_append_string_len(out, CONST_STR_LEN("</div>\n"));
	
	//- Jerry modify
	buffer_append_string_len(out, CONST_STR_LEN("<div id=\"mainRegion\" url='"));
	buffer_append_string_encoded(out, CONST_BUF_LEN(con->uri.path), ENCODING_MINIMAL_XML);
	
	buffer_append_string_len(out, CONST_STR_LEN("' qflag='"));
	char gflag_buf[10]="\0";	

	char usbdisk_path[50] = "/usbdisk/";
	#if EMBEDDED_EANBLE
	strcpy(usbdisk_path, "/");
	strcat(usbdisk_path, nvram_get_productid());
	strcat(usbdisk_path, "/");
	#endif
		
	if(con->url.path->used && strcmp(con->url.path->ptr, usbdisk_path)==0)
		sprintf(gflag_buf, "%d", 1);
	else
		sprintf(gflag_buf, "%d", 0);
	buffer_append_string(out, gflag_buf);

	#if 0
	buffer_append_string_len(out, CONST_STR_LEN("' key='"));
	char mac[20]="\0";
	get_mac_address("eth0", &mac);
	buffer_append_string(out, ldb_base64_encode(mac, strlen(mac)));
	
	buffer_append_string_len(out, CONST_STR_LEN("' auth='"));
	char auth_buf[100]="\0";
	if(con->smb_info && con->smb_info->username->used && con->smb_info->password->used)
		sprintf(auth_buf, "%s:%s", con->smb_info->username->ptr, con->smb_info->password->ptr);
	buffer_append_string(out, ldb_base64_encode(auth_buf, strlen(auth_buf)));
	#endif
	
	buffer_append_string_len(out, CONST_STR_LEN("'>\n"));

	//- Control Region
	buffer_append_string_len(out, CONST_STR_LEN("<table id='headerRegion' width='100%' border='0' cellspacing='3' cellpadding='0'>"));
	buffer_append_string_len(out, CONST_STR_LEN("<tr>"));

	buffer_append_string_len(out, CONST_STR_LEN("<td class='albutton'><div id='homeview' class='button'></div></td>"));
	buffer_append_string_len(out, CONST_STR_LEN("<td class='arbutton'><div id='btnNewDir' class='button'></div></td>"));
	buffer_append_string_len(out, CONST_STR_LEN("<td class='arbutton'><div id='btnDeleteSel' class='button'></div></td>"));
	buffer_append_string_len(out, CONST_STR_LEN("<td class='arbutton'><div id='btnUpload' class='button'></div></td>"));		
	buffer_append_string_len(out, CONST_STR_LEN("<td class='arbutton'><div id='split'></div></td>"));	
	buffer_append_string_len(out, CONST_STR_LEN("<td class='arbutton'><div id='thumbview' class='button'></div></td>"));
	buffer_append_string_len(out, CONST_STR_LEN("<td class='arbutton'><div id='listview' class='button'></div></td>"));
	
	buffer_append_string_len(out, CONST_STR_LEN("</tr>"));
	buffer_append_string_len(out, CONST_STR_LEN("</table>"));
		
	
	//- ToolBar Region
	buffer_append_string_len(out, CONST_STR_LEN("<table id='toolbarRegion' width='100%' border='0' cellspacing='3' cellpadding='0'>"));
	buffer_append_string_len(out, CONST_STR_LEN("<tr>"));

	buffer_append_string_len(out, CONST_STR_LEN("<td class='albutton'><div id='prevview' class='button'></div></td>"));

	buffer_append_string_len(out, CONST_STR_LEN("<td ><p align='left' width='250' > Index of "));
	buffer_append_string_encoded(out, CONST_BUF_LEN(con->uri.path), ENCODING_MINIMAL_XML);
	buffer_append_string_len(out, CONST_STR_LEN("</p></td>"));

	buffer_append_string_len(out, CONST_STR_LEN("</tr>"));
	buffer_append_string_len(out, CONST_STR_LEN("</table>"));

   	buffer_append_string_len(out, CONST_STR_LEN("</tbody></table>\n"));
}

void http_list_directory_footer(server *srv, connection *con, plugin_data *p, buffer *out) 
{
	UNUSED(srv);

	buffer_append_string_len(out, CONST_STR_LEN(
		"</td>\n"
		"</tr>\n"
		"</tbody>\n"
		"</table>\n"
		"</div>\n"
		"</body>\n"
		"</html>\n"
	));
}

#if 0
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

		if (p->conf.external_css->used > 1) {
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

		buffer_copy_string_buffer(p->tmp_buf, con->physical.path);
		BUFFER_APPEND_SLASH(p->tmp_buf);
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

		buffer_copy_string_buffer(p->tmp_buf,  con->physical.path);
		BUFFER_APPEND_SLASH(p->tmp_buf);
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

		if (p->conf.set_footer->used > 1) {
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
#endif

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
	char tmpbuf[100];
	char sizebuf[sizeof("999.9K")];
	char datebuf[sizeof("2005-Jan-01 22:23:24")];
	size_t k;
	const char *content_type;
	long name_max;
#ifdef HAVE_XATTR
	char attrval[128];
	int attrlen;
#endif
#ifdef HAVE_LOCALTIME_R
	struct tm tm;
#endif
	
	if (dir->used == 0) return -1;
	
	i = dir->used - 1;

#ifdef HAVE_PATHCONF
	if (-1 == (name_max = pathconf(dir->ptr, _PC_NAME_MAX))) {
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
	
	path = malloc(dir->used + name_max);
	assert(path);
	strcpy(path, dir->ptr);
	path_file = path + i;
	
	if (NULL == (dp = opendir(path))) {
		log_error_write(srv, __FILE__, __LINE__, "sbs",
			"opendir failed:", dir, strerror(errno));

		free(path);
		return -1;
	}
	
	dirs.ent   = (dirls_entry_t**) malloc(sizeof(dirls_entry_t*) * DIRLIST_BLOB_SIZE);
	assert(dirs.ent);
	dirs.size  = DIRLIST_BLOB_SIZE;
	dirs.used  = 0;
	files.ent  = (dirls_entry_t**) malloc(sizeof(dirls_entry_t*) * DIRLIST_BLOB_SIZE);
	assert(files.ent);
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

					return -1;
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
			assert(list->ent);
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
	
	out = chunkqueue_get_append_buffer(con->write_queue);
	buffer_copy_string_len(out, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\""));
	if (buffer_is_empty(p->conf.encoding)) {
		buffer_append_string_len(out, CONST_STR_LEN("iso-8859-1"));
	} else {
		buffer_append_string_buffer(out, p->conf.encoding);
	}
	Cdbg(DBE, "http_list_directory....");
	buffer_append_string_len(out, CONST_STR_LEN("\"?>\n"));
	http_list_directory_header(srv, con, p, out);
	
	int b_list_view = 0;	
	int b_query_file = 0;
	if(con->url_options->used && strstr(con->url_options->ptr, "a=1"))
		b_list_view = 1;

	char usbdisk_path[50] = "/usbdisk/";
	#if EMBEDDED_EANBLE
	strcpy(usbdisk_path, "/");
	strcat(usbdisk_path, nvram_get_productid());
	strcat(usbdisk_path, "/");
	#endif
		
	if(con->url.path->used && strcmp(con->url.path->ptr, usbdisk_path)==0)
		b_query_file = 0;
	else
		b_query_file = 1;
	
	if(b_list_view==1){
		buffer_append_string_len(out, CONST_STR_LEN("<div id=\"div_table_content\">"));
		buffer_append_string_len(out, CONST_STR_LEN("<table id=\"ntb\" pagenum=\"0\" maxrows=\"13\" flock=\"\" field=\"time\" fval=\"\" order=\"DESC\" totalpages=\"84\" keyword=\"\" class=\"sortable\" width=\"100%\">"));

		buffer_append_string_len(out, CONST_STR_LEN("<thead class='category'>"));
		buffer_append_string_len(out, CONST_STR_LEN("<tr>"));
		buffer_append_string_len(out, CONST_STR_LEN("<td class='icon'>&nbsp;</td>"));
		buffer_append_string_len(out, CONST_STR_LEN("<td class='check' style='width:25px;'>&nbsp;</td>"));
		buffer_append_string_len(out, CONST_STR_LEN("<th class='name' popupmenu='0' style='width:350px;'>Name</th>"));
		buffer_append_string_len(out, CONST_STR_LEN("<td class='size'>Size</td>"));
		buffer_append_string_len(out, CONST_STR_LEN("<td class='time'>Time</td>"));
		buffer_append_string_len(out, CONST_STR_LEN("<td class='type'>Type</td>"));
		buffer_append_string_len(out, CONST_STR_LEN("</tr>"));
		buffer_append_string_len(out, CONST_STR_LEN("</thead>"));

		//- Parent
		buffer_append_string_len(out, CONST_STR_LEN("<tbody>"));
		buffer_append_string_len(out, CONST_STR_LEN("<tr>"));
						
		buffer_append_string_len(out, CONST_STR_LEN("<td class='icon'><div class='sparentDiv sicon'></div></td>"));
		if(b_query_file==1) 
			buffer_append_string_len(out, CONST_STR_LEN("<td class='check'>&nbsp;</td>"));
		buffer_append_string_len(out, CONST_STR_LEN("<th class='name' popupmenu='0'><a id='list_item' uhref='../'>..</a></th>"));
			
		buffer_append_string_len(out, CONST_STR_LEN("<td class='size'>"));
		buffer_append_string_len(out, CONST_STR_LEN("</td>"));

		buffer_append_string_len(out, CONST_STR_LEN("<td class='time'>"));
		buffer_append_string_len(out, CONST_STR_LEN("</td>"));
			
		buffer_append_string_len(out, CONST_STR_LEN("<td class='type'>"));
		buffer_append_string_len(out, CONST_STR_LEN("</td>"));
			
		buffer_append_string_len(out, CONST_STR_LEN("</tr></tbody>"));
	}
	else{
		buffer_append_string_len(out, CONST_STR_LEN("<div class='albumDiv'>"));
		buffer_append_string_len(out, CONST_STR_LEN("<table class='thumb-table-parent'><tbody>"));
		if(b_query_file==1) 
			buffer_append_string_len(out, CONST_STR_LEN("<tr class='thumb-top-option'><td class='thumb-check-area'></tr></td>"));
		else
			buffer_append_string_len(out, CONST_STR_LEN("<tr><td class='thumb-check-area'></tr></td>"));
		buffer_append_string_len(out, CONST_STR_LEN("<tr><td>"));
		buffer_append_string_len(out, CONST_STR_LEN("<div class='picDiv' popupmenu='0' uhref=\"../\">"));
		buffer_append_string_len(out, CONST_STR_LEN("<div class='parentDiv bicon'>"));
		buffer_append_string_len(out, CONST_STR_LEN("</div></div></tr></td>"));
		buffer_append_string_len(out, CONST_STR_LEN("<tr><td>"));
		buffer_append_string_len(out, CONST_STR_LEN("<div class='albuminfo' style='font-size:80%'>"));
		buffer_append_string_len(out, CONST_STR_LEN("<a id='list_item' qtype='0' isdir='1' uhref=\"../\">"));
		buffer_append_string_len(out, CONST_STR_LEN("..</a></div></tr></td></tbody></table></div>"));
	}
	
	/* directories */
	for (i = 0; i < dirs.used; i++) {
		tmp = dirs.ent[i];

		buffer* uhref = buffer_init();
		buffer_append_string_len(uhref, con->uri.path->ptr, con->uri.path->used -1);
		if(con->uri.path->ptr[con->uri.path->used-2] != '/')
			buffer_append_string_len(uhref, CONST_STR_LEN("/"));
		buffer_append_string_encoded(uhref, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_REL_URI_PART);
		buffer_append_string_len(uhref, CONST_STR_LEN("/"));
			
#ifdef HAVE_LOCALTIME_R
		localtime_r(&(tmp->mtime), &tm);
		strftime(datebuf, sizeof(datebuf), "%Y-%b-%d %H:%M:%S", &tm);
#else
		strftime(datebuf, sizeof(datebuf), "%Y-%b-%d %H:%M:%S", localtime(&(tmp->mtime)));
#endif

		if(b_list_view==1){
			buffer_append_string_len(out, CONST_STR_LEN("<tbody>"));
			buffer_append_string_len(out, CONST_STR_LEN("<tr class='content'>"));
						
			buffer_append_string_len(out, CONST_STR_LEN("<td class='icon'><div class='sfolderDiv sicon'></div>"));

			if(b_query_file==1)  {
				buffer_append_string_len(out, CONST_STR_LEN("<td class='check'><input type='checkbox' id='c1'></td>"));			
				//buffer_append_string_len(out, CONST_STR_LEN("<th class='name' popupmenu='1'><div id='div_item' style='position:absolute;'>"));
				buffer_append_string_len(out, CONST_STR_LEN("<th class='name' popupmenu='1'><div id='div_item'>"));
			}
			else{
				//buffer_append_string_len(out, CONST_STR_LEN("<th class='name' popupmenu='0'><div id='div_item' style='position:absolute;'>"));
				buffer_append_string_len(out, CONST_STR_LEN("<th class='name' popupmenu='0'><div id='div_item'>"));
			}
			
			buffer_append_string_len(out, CONST_STR_LEN("<a id='list_item' qtype='"));

			if(b_query_file==1) 
				buffer_append_string_len(out, CONST_STR_LEN("0"));
			else
				buffer_append_string_len(out, CONST_STR_LEN("1"));
			
			buffer_append_string_len(out, CONST_STR_LEN("' isdir='1' uhref='"));

			buffer_append_string_len(out, con->uri.path->ptr, con->uri.path->used -1);
			if(con->uri.path->ptr[con->uri.path->used-2] != '/')
				buffer_append_string_len(out, CONST_STR_LEN("/"));
			buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_REL_URI_PART);

			buffer_append_string_len(out, CONST_STR_LEN("/'"));
			buffer_append_string_len(out, CONST_STR_LEN(" title='"));
			buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);
						
			buffer_append_string_len(out, CONST_STR_LEN("'>"));

			buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);
						
			buffer_append_string_len(out, CONST_STR_LEN("</a>"));
			buffer_append_string_len(out, CONST_STR_LEN("</div></th>"));
			
			buffer_append_string_len(out, CONST_STR_LEN("<td class='size'>"));
			buffer_append_string_len(out, CONST_STR_LEN("</td>"));

			buffer_append_string_len(out, CONST_STR_LEN("<td class='time'>"));
			buffer_append_string_len(out, CONST_STR_LEN("</td>"));
			
			buffer_append_string_len(out, CONST_STR_LEN("<td class='type'>"));
			buffer_append_string_len(out, CONST_STR_LEN("Folder"));
			buffer_append_string_len(out, CONST_STR_LEN("</td>"));
				
			buffer_append_string_len(out, CONST_STR_LEN("</tr></tbody>"));
		}
		else{
			//- Jerry modify			
			buffer_append_string_len(out, CONST_STR_LEN("<div class='albumDiv'>"));
			buffer_append_string_len(out, CONST_STR_LEN("<table class='thumb-table-parent'><tbody>"));
			
			if(b_query_file==1)  {
				buffer_append_string_len(out, CONST_STR_LEN("<tr class='thumb-top-option'><td class='thumb-check-area'>"));
				buffer_append_string_len(out, CONST_STR_LEN("<input type='checkbox' id='c1'>"));
			}
			else
				buffer_append_string_len(out, CONST_STR_LEN("<tr><td class='thumb-check-area'>"));
			
			buffer_append_string_len(out, CONST_STR_LEN("</tr></td>"));

			buffer_append_string_len(out, CONST_STR_LEN("<tr><td>"));

			if(b_query_file==1)
				buffer_append_string_len(out, CONST_STR_LEN("<div class='picDiv' popupmenu='1' uhref=\""));
			else
				buffer_append_string_len(out, CONST_STR_LEN("<div class='picDiv' popupmenu='0' uhref=\""));
				
			buffer_append_string_buffer(out, uhref);
			buffer_append_string_len(out, CONST_STR_LEN("\">"));
			
			buffer_append_string_len(out, CONST_STR_LEN("<div class='folderDiv bicon'>"));

			buffer_append_string_len(out, CONST_STR_LEN("</div></div>"));
			buffer_append_string_len(out, CONST_STR_LEN("</tr></td>"));

			buffer_append_string_len(out, CONST_STR_LEN("<tr><td>"));
			buffer_append_string_len(out, CONST_STR_LEN("<div class='albuminfo' style='font-size:80%'>"));
	        	buffer_append_string_len(out, CONST_STR_LEN("<a id='list_item' qtype='"));

			if(b_query_file==1) 
				buffer_append_string_len(out, CONST_STR_LEN("0"));						
			else
				buffer_append_string_len(out, CONST_STR_LEN("1"));
			
			buffer_append_string_len(out, CONST_STR_LEN("' isdir='1' uhref=\""));
			buffer_append_string_buffer(out, uhref);			
			buffer_append_string_len(out, CONST_STR_LEN("\" title='"));
			buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);
		
			buffer_append_string_len(out, CONST_STR_LEN("'>"));
		
			if(tmp->namelen>12){
				memset(tmpbuf, 0, sizeof(tmpbuf));
				buffer_append_string_len(out, DIRLIST_ENT_NAME(tmp), 12 );
				buffer_append_string_len(out, "...", 3 );
			}
			else
				buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);

			buffer_append_string_len(out, CONST_STR_LEN("</a>"));
			buffer_append_string_len(out, CONST_STR_LEN("</div>"));
			buffer_append_string_len(out, CONST_STR_LEN("</tr></td>"));
			buffer_append_string_len(out, CONST_STR_LEN("</tbody></table></div>"));

			
		}

#if 0
		buffer_append_string_len(out, CONST_STR_LEN("<tr><td class=\"n\"><a href=\""));
		buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_REL_URI_PART);
		buffer_append_string_len(out, CONST_STR_LEN("/\">"));
		buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);
		buffer_append_string_len(out, CONST_STR_LEN("</a>/</td><td class=\"m\">"));
		buffer_append_string_len(out, datebuf, sizeof(datebuf) - 1);
		buffer_append_string_len(out, CONST_STR_LEN("</td><td class=\"s\">- &nbsp;</td><td class=\"t\">Directory</td></tr>\n"));
#endif

		buffer_free(uhref);
		free(tmp);
	}
	
	/* files */
	for (i = 0; i < files.used; i++) {
		tmp = files.ent[i];

		buffer* uhref = buffer_init();
		buffer_append_string_len(uhref, con->uri.path->ptr, con->uri.path->used -1);
		if(con->uri.path->ptr[con->uri.path->used-2] != '/')
			buffer_append_string_len(uhref, CONST_STR_LEN("/"));
		buffer_append_string_encoded(uhref, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_REL_URI_PART);
			
		content_type = NULL;
#ifdef HAVE_XATTR

		if (con->conf.use_xattr) {
			memcpy(path_file, DIRLIST_ENT_NAME(tmp), tmp->namelen + 1);
			attrlen = sizeof(attrval) - 1;
			if (attr_get(path, "Content-Type", attrval, &attrlen, 0) == 0) {
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

				if (ds->key->used == 0)
					continue;

				ct_len = ds->key->used - 1;
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

		if(b_list_view==1){
			buffer_append_string_len(out, CONST_STR_LEN("<tbody>"));
			buffer_append_string_len(out, CONST_STR_LEN("<tr class='content'>"));
					
			buffer_append_string_len(out, CONST_STR_LEN("<td class='icon'><div class='sfileDiv sicon'></div></td>"));
			
			if(b_query_file==1)  {
				buffer_append_string_len(out, CONST_STR_LEN("<td class='check'><input type='checkbox' id='c1'></td>"));
				//buffer_append_string_len(out, CONST_STR_LEN("<th class='name' popupmenu='1'><div id='div_item' style='position:absolute;'>"));
				buffer_append_string_len(out, CONST_STR_LEN("<th class='name' popupmenu='1'><div id='div_item'>"));
			}
			else
				//buffer_append_string_len(out, CONST_STR_LEN("<th class='name' popupmenu='0'><div id='div_item' style='position:absolute;'>"));
				buffer_append_string_len(out, CONST_STR_LEN("<th class='name' popupmenu='0'><div id='div_item'>"));
			
			buffer_append_string_len(out, CONST_STR_LEN("<a id='list_item' qtype='"));

			if(b_query_file==1) 
				buffer_append_string_len(out, CONST_STR_LEN("0"));
			else
				buffer_append_string_len(out, CONST_STR_LEN("1"));
			
			buffer_append_string_len(out, CONST_STR_LEN("' isdir='0' uhref='"));
			
			buffer_append_string_len(out, con->uri.path->ptr, con->uri.path->used -1);
			if(con->uri.path->ptr[con->uri.path->used-2] != '/')
				buffer_append_string_len(out, CONST_STR_LEN("/"));
			buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_REL_URI_PART);

			buffer_append_string_len(out, CONST_STR_LEN("' title='"));
			buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);			
			buffer_append_string_len(out, CONST_STR_LEN("'>"));
			
			buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);
			buffer_append_string_len(out, CONST_STR_LEN("</a>"));
			buffer_append_string_len(out, CONST_STR_LEN("</div>"));
			buffer_append_string_len(out, CONST_STR_LEN("</th>"));
			
			buffer_append_string_len(out, CONST_STR_LEN("<td class='size'>"));
			buffer_append_string(out, sizebuf);
			buffer_append_string_len(out, CONST_STR_LEN("</td>"));

			buffer_append_string_len(out, CONST_STR_LEN("<td class='time'>"));
			buffer_append_string_len(out, CONST_STR_LEN(datebuf));
			buffer_append_string_len(out, CONST_STR_LEN("</td>"));
			
			buffer_append_string_len(out, CONST_STR_LEN("<td class='type'>"));
			buffer_append_string_len(out, CONST_STR_LEN("File"));
			buffer_append_string_len(out, CONST_STR_LEN("</td>"));
				
			buffer_append_string_len(out, CONST_STR_LEN("</tr></tbody>"));
		}
		else{
			//- Jerry modify
			buffer_append_string_len(out, CONST_STR_LEN("<div class='albumDiv'>"));
			buffer_append_string_len(out, CONST_STR_LEN("<table class='thumb-table-parent'><tbody>"));
			
			if(b_query_file==1){
				buffer_append_string_len(out, CONST_STR_LEN("<tr class='thumb-top-option'><td class='thumb-check-area'>"));
				buffer_append_string_len(out, CONST_STR_LEN("<input type='checkbox' id='c1'>"));
			}
			else
				buffer_append_string_len(out, CONST_STR_LEN("<tr><td class='thumb-check-area'>"));
			
			buffer_append_string_len(out, CONST_STR_LEN("</tr></td>"));

			buffer_append_string_len(out, CONST_STR_LEN("<tr><td>"));

			if(b_query_file==1)
				buffer_append_string_len(out, CONST_STR_LEN("<div class='picDiv' popupmenu='1' uhref=\""));
			else
				buffer_append_string_len(out, CONST_STR_LEN("<div class='picDiv' popupmenu='0' uhref=\""));
				
			buffer_append_string_buffer(out, uhref);
			buffer_append_string_len(out, CONST_STR_LEN("\">"));				
			buffer_append_string_len(out, CONST_STR_LEN("<div class='fileDiv bicon'>"));	
			buffer_append_string_len(out, CONST_STR_LEN("</div></div>"));
			buffer_append_string_len(out, CONST_STR_LEN("</tr></td>"));

			buffer_append_string_len(out, CONST_STR_LEN("<tr><td>"));
			buffer_append_string_len(out, CONST_STR_LEN("<div class='albuminfo' style='font-size:80%'>"));
	        	buffer_append_string_len(out, CONST_STR_LEN("<a id='list_item' qtype='0' isdir='0' uhref=\""));
			buffer_append_string_buffer(out, uhref);
			buffer_append_string_len(out, CONST_STR_LEN("\" title=\""));
			buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);
			buffer_append_string_len(out, CONST_STR_LEN("\">"));

			if(tmp->namelen>12){

				buffer_append_string_len(out, DIRLIST_ENT_NAME(tmp), 12 );
				buffer_append_string_len(out, "...", 3 );
			}
			else
				buffer_append_string_encoded(out, DIRLIST_ENT_NAME(tmp), tmp->namelen, ENCODING_MINIMAL_XML);

			buffer_append_string_len(out, CONST_STR_LEN("</a>"));
			buffer_append_string_len(out, CONST_STR_LEN("</div>"));
			buffer_append_string_len(out, CONST_STR_LEN("</tr></td>"));
			buffer_append_string_len(out, CONST_STR_LEN("</tbody></table></div>"));
		}
#if 0
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
#endif

		free(tmp);
	}

	free(files.ent);
	free(dirs.ent);
	free(path);

	//- modal window
	buffer_append_string_len(out, CONST_STR_LEN("<div id='modalWindow' class='jqmWindow'>"));
	buffer_append_string_len(out, CONST_STR_LEN("<div id='jqmTitle'>"));
	buffer_append_string_len(out, CONST_STR_LEN("<span id='jqmTitleText'></span>"));
	buffer_append_string_len(out, CONST_STR_LEN("</div>"));
	buffer_append_string_len(out, CONST_STR_LEN("<iframe id='jqmContent' src='' frameborder='0'>"));
	buffer_append_string_len(out, CONST_STR_LEN("</div>"));
	
	http_list_directory_footer(srv, con, p, out);

	/* Insert possible charset to Content-Type */
	if (buffer_is_empty(p->conf.encoding)) {
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html"));
	} else {
		buffer_copy_string_len(p->content_charset, CONST_STR_LEN("text/html; charset="));
		buffer_append_string_buffer(p->content_charset, p->conf.encoding);
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(p->content_charset));
	}

	con->file_finished = 1;

	return 0;
}



URIHANDLER_FUNC(mod_dirlisting_subrequest) {
	plugin_data *p = p_d;
	stat_cache_entry *sce = NULL;
Cdbg(DBE, "enter..status=[%d], uri=[%s], path=[%s], mode=[%d]", con->http_status, con->uri.path->ptr, con->physical.path->ptr, con->mode);
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

	if (con->mode != DIRECT && con->mode != SMB_BASIC && con->mode != SMB_NTLM) return HANDLER_GO_ON;
	
	if (con->physical.path->used == 0) return HANDLER_GO_ON;
	if (con->uri.path->used == 0) return HANDLER_GO_ON;
	if (con->uri.path->ptr[con->uri.path->used - 2] != '/') {
		Cdbg(DBE, "not ending with '/'..");
		return HANDLER_GO_ON;
	}
	
	mod_dirlisting_patch_connection(srv, con, p);
	
	if (!p->conf.dir_listing) return HANDLER_GO_ON;
	
	if (con->conf.log_request_handling) {
		log_error_write(srv, __FILE__, __LINE__,  "s",  "-- handling the request as Dir-Listing");
		log_error_write(srv, __FILE__, __LINE__,  "sb", "URI          :", con->uri.path);
	}
	
	if (HANDLER_ERROR == stat_cache_get_entry(srv, con, smbc_wrapper_physical_url_path(srv, con), &sce)) {
		log_error_write(srv, __FILE__, __LINE__,  "SB", "stat_cache_get_entry failed: ", con->physical.path);
		SEGFAULT();
	}
	
	if (!S_ISDIR(sce->st.st_mode)) return HANDLER_GO_ON;
	
	if(con->mode == DIRECT) {		
		if (http_list_directory(srv, con, p, con->physical.path)) {
			/* dirlisting failed */
			con->http_status = 403;
		}		
	}
#ifdef HAVE_LIBSMBCLIENT
	else {
		if(smbc_list_directory(srv, con, p, con->url.path)) {
			/* dirlisting failed */
			con->http_status = 403;
		}
	}
#if 0
	else if(con->mode == SMB_BASIC) {
		if(basic_smbc_list_directory(srv, con, p, con->url.path)) {
			/* dirlisting failed */
			con->http_status = 403;
		}
	} else {
		if(ntlm_smbc_list_directory(srv, con, p, con->url.path)) {
			/* dirlisting failed */
			con->http_status = 403;
		}
	}
#endif

#endif
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
