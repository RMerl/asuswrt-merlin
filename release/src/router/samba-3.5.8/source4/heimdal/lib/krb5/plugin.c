/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "krb5_locl.h"

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#include <dirent.h>

struct krb5_plugin {
    void *symbol;
    struct krb5_plugin *next;
};

struct plugin {
    enum { DSO, SYMBOL } type;
    union {
	struct {
	    char *path;
	    void *dsohandle;
	} dso;
	struct {
	    enum krb5_plugin_type type;
	    char *name;
	    char *symbol;
	} symbol;
    } u;
    struct plugin *next;
};

static HEIMDAL_MUTEX plugin_mutex = HEIMDAL_MUTEX_INITIALIZER;
static struct plugin *registered = NULL;
static int plugins_needs_scan = 1;

static const char *sysplugin_dirs[] =  { 
    LIBDIR "/plugin/krb5",
#ifdef __APPLE__
    "/System/Library/KerberosPlugins/KerberosFrameworkPlugins",
#endif
    NULL
};

/*
 *
 */

void *
_krb5_plugin_get_symbol(struct krb5_plugin *p)
{
    return p->symbol;
}

struct krb5_plugin *
_krb5_plugin_get_next(struct krb5_plugin *p)
{
    return p->next;
}

/*
 *
 */

#ifdef HAVE_DLOPEN

static krb5_error_code
loadlib(krb5_context context, char *path)
{
    struct plugin *e;

    e = calloc(1, sizeof(*e));
    if (e == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	free(path);
	return ENOMEM;
    }

#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif
#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#endif
    e->type = DSO;
    /* ignore error from dlopen, and just keep it as negative cache entry */
    e->u.dso.dsohandle = dlopen(path, RTLD_LOCAL|RTLD_LAZY);
    e->u.dso.path = path;

    e->next = registered;
    registered = e;

    return 0;
}
#endif /* HAVE_DLOPEN */

/**
 * Register a plugin symbol name of specific type.
 * @param context a Keberos context
 * @param type type of plugin symbol
 * @param name name of plugin symbol
 * @param symbol a pointer to the named symbol
 * @return In case of error a non zero error com_err error is returned
 * and the Kerberos error string is set.
 *
 * @ingroup krb5_support
 */

krb5_error_code
krb5_plugin_register(krb5_context context,
		     enum krb5_plugin_type type,
		     const char *name,
		     void *symbol)
{
    struct plugin *e;

    HEIMDAL_MUTEX_lock(&plugin_mutex);

    /* check for duplicates */
    for (e = registered; e != NULL; e = e->next) {
	if (e->type == SYMBOL &&
	    strcmp(e->u.symbol.name, name) == 0 &&
	    e->u.symbol.type == type && e->u.symbol.symbol == symbol) {
	    HEIMDAL_MUTEX_unlock(&plugin_mutex);
	    return 0;
	}
    }

    e = calloc(1, sizeof(*e));
    if (e == NULL) {
	HEIMDAL_MUTEX_unlock(&plugin_mutex);
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    e->type = SYMBOL;
    e->u.symbol.type = type;
    e->u.symbol.name = strdup(name);
    if (e->u.symbol.name == NULL) {
	HEIMDAL_MUTEX_unlock(&plugin_mutex);
	free(e);
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    e->u.symbol.symbol = symbol;

    e->next = registered;
    registered = e;
    HEIMDAL_MUTEX_unlock(&plugin_mutex);

    return 0;
}

static krb5_error_code
load_plugins(krb5_context context)
{
    struct plugin *e;
    krb5_error_code ret;
    char **dirs = NULL, **di;
    struct dirent *entry;
    char *path;
    DIR *d = NULL;

    if (!plugins_needs_scan)
	return 0;
    plugins_needs_scan = 0;

#ifdef HAVE_DLOPEN

    dirs = krb5_config_get_strings(context, NULL, "libdefaults",
				   "plugin_dir", NULL);
    if (dirs == NULL)
	dirs = rk_UNCONST(sysplugin_dirs);

    for (di = dirs; *di != NULL; di++) {

	d = opendir(*di);
	if (d == NULL)
	    continue;
	rk_cloexec(dirfd(d));

	while ((entry = readdir(d)) != NULL) {
	    char *n = entry->d_name;

	    /* skip . and .. */
	    if (n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0')))
		continue;

	    path = NULL;
#ifdef __APPLE__
	    { /* support loading bundles on MacOS */
		size_t len = strlen(n);
		if (len > 7 && strcmp(&n[len - 7],  ".bundle") == 0)
		    asprintf(&path, "%s/%s/Contents/MacOS/%.*s", *di, n, (int)(len - 7), n);
	    }
#endif
	    if (path == NULL)
		asprintf(&path, "%s/%s", *di, n);

	    if (path == NULL) {
		ret = ENOMEM;
		krb5_set_error_message(context, ret, "malloc: out of memory");
		return ret;
	    }

	    /* check if already tried */
	    for (e = registered; e != NULL; e = e->next)
		if (e->type == DSO && strcmp(e->u.dso.path, path) == 0)
		    break;
	    if (e) {
		free(path);
	    } else {
		loadlib(context, path); /* store or frees path */
	    }
	}
	closedir(d);
    }
    if (dirs != rk_UNCONST(sysplugin_dirs))
	krb5_config_free_strings(dirs);
#endif /* HAVE_DLOPEN */
    return 0;
}

static krb5_error_code
add_symbol(krb5_context context, struct krb5_plugin **list, void *symbol)
{
    struct krb5_plugin *e;
    
    e = calloc(1, sizeof(*e));
    if (e == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    e->symbol = symbol;
    e->next = *list;
    *list = e;
    return 0;
}

krb5_error_code
_krb5_plugin_find(krb5_context context,
		  enum krb5_plugin_type type,
		  const char *name,
		  struct krb5_plugin **list)
{
    struct plugin *e;
    krb5_error_code ret;

    *list = NULL;

    HEIMDAL_MUTEX_lock(&plugin_mutex);
    
    load_plugins(context);

    for (ret = 0, e = registered; e != NULL; e = e->next) {
	switch(e->type) {
	case DSO: {
	    void *sym;
	    if (e->u.dso.dsohandle == NULL)
		continue;
	    sym = dlsym(e->u.dso.dsohandle, name);
	    if (sym)
		ret = add_symbol(context, list, sym);
	    break;
	}
	case SYMBOL:
	    if (strcmp(e->u.symbol.name, name) == 0 && e->u.symbol.type == type)
		ret = add_symbol(context, list, e->u.symbol.symbol);
	    break;
	}
	if (ret) {
	    _krb5_plugin_free(*list);
	    *list = NULL;
	}
    }

    HEIMDAL_MUTEX_unlock(&plugin_mutex);
    if (ret)
	return ret;

    if (*list == NULL) {
	krb5_set_error_message(context, ENOENT, "Did not find a plugin for %s", name);
	return ENOENT;
    }

    return 0;
}

void
_krb5_plugin_free(struct krb5_plugin *list)
{
    struct krb5_plugin *next;
    while (list) {
	next = list->next;
	free(list);
	list = next;
    }
}
