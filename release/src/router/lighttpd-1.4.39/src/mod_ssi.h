#ifndef _MOD_SSI_H_
#define _MOD_SSI_H_

#include "base.h"
#include "buffer.h"
#include "array.h"

#include "plugin.h"

#ifdef HAVE_PCRE_H
#include <pcre.h>
#endif

/* plugin config for all request/connections */

typedef struct {
	array *ssi_extension;
	buffer *content_type;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

#ifdef HAVE_PCRE_H
	pcre *ssi_regex;
#endif
	buffer *timefmt;
	int sizefmt;

	buffer *stat_fn;

	array *ssi_vars;
	array *ssi_cgi_env;

	int if_level, if_is_false_level, if_is_false, if_is_false_endif;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

int ssi_eval_expr(server *srv, connection *con, plugin_data *p, const char *expr);

#endif
