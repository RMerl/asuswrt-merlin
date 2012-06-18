#include "plugin.h"
#include "log.h"

#include <string.h>
#include <stdlib.h>

#include <stdio.h>

#ifdef HAVE_VALGRIND_VALGRIND_H
# include <valgrind/valgrind.h>
#endif

#ifndef __WIN32
# include <dlfcn.h>
#endif
/*
 *
 * if you change this enum to add a new callback, be sure
 * - that PLUGIN_FUNC_SIZEOF is the last entry
 * - that you add PLUGIN_TO_SLOT twice:
 *   1. as callback-dispatcher
 *   2. in plugins_call_init()
 *
 */

typedef struct {
	PLUGIN_DATA;
} plugin_data;

typedef enum {
	PLUGIN_FUNC_UNSET,
		PLUGIN_FUNC_HANDLE_URI_CLEAN,
		PLUGIN_FUNC_HANDLE_URI_RAW,
		PLUGIN_FUNC_HANDLE_REQUEST_DONE,
		PLUGIN_FUNC_HANDLE_CONNECTION_CLOSE,
		PLUGIN_FUNC_HANDLE_TRIGGER,
		PLUGIN_FUNC_HANDLE_SIGHUP,
		PLUGIN_FUNC_HANDLE_SUBREQUEST,
		PLUGIN_FUNC_HANDLE_SUBREQUEST_START,
		PLUGIN_FUNC_HANDLE_JOBLIST,
		PLUGIN_FUNC_HANDLE_DOCROOT,
		PLUGIN_FUNC_HANDLE_PHYSICAL,
		PLUGIN_FUNC_CONNECTION_RESET,
		PLUGIN_FUNC_INIT,
		PLUGIN_FUNC_CLEANUP,
		PLUGIN_FUNC_SET_DEFAULTS,

		PLUGIN_FUNC_SIZEOF
} plugin_t;

static plugin *plugin_init(void) {
	plugin *p;

	p = calloc(1, sizeof(*p));

	return p;
}

static void plugin_free(plugin *p) {
	int use_dlclose = 1;
	if (p->name) buffer_free(p->name);
#ifdef HAVE_VALGRIND_VALGRIND_H
	/*if (RUNNING_ON_VALGRIND) use_dlclose = 0;*/
#endif

#ifndef LIGHTTPD_STATIC
	if (use_dlclose && p->lib) {
#ifdef __WIN32
		FreeLibrary(p->lib);
#else
		dlclose(p->lib);
#endif
	}
#endif

	free(p);
}

static int plugins_register(server *srv, plugin *p) {
	plugin **ps;
	if (0 == srv->plugins.size) {
		srv->plugins.size = 4;
		srv->plugins.ptr  = malloc(srv->plugins.size * sizeof(*ps));
		srv->plugins.used = 0;
	} else if (srv->plugins.used == srv->plugins.size) {
		srv->plugins.size += 4;
		srv->plugins.ptr   = realloc(srv->plugins.ptr, srv->plugins.size * sizeof(*ps));
	}

	ps = srv->plugins.ptr;
	ps[srv->plugins.used++] = p;

	return 0;
}

/**
 *
 *
 *
 */

#ifdef LIGHTTPD_STATIC
int plugins_load(server *srv) {
	plugin *p;
#define PLUGIN_INIT(x)\
	p = plugin_init(); \
	if (x ## _plugin_init(p)) { \
		log_error_write(srv, __FILE__, __LINE__, "ss", #x, "plugin init failed" ); \
		plugin_free(p); \
		return -1;\
	}\
	plugins_register(srv, p);

#include "plugin-static.h"

	return 0;
}
#else
int plugins_load(server *srv) {
	plugin *p;
	int (*init)(plugin *pl);
	const char *error;
	size_t i, j;

	for (i = 0; i < srv->srvconf.modules->used; i++) {
		data_string *d = (data_string *)srv->srvconf.modules->data[i];
		char *modules = d->value->ptr;

		for (j = 0; j < i; j++) {
			if (buffer_is_equal(d->value, ((data_string *) srv->srvconf.modules->data[j])->value)) {
				log_error_write(srv, __FILE__, __LINE__, "sbs", "Cannot load plugin", d->value, "more than once, please fix your config (we may not accept such configs in future releases");
				continue;
			}
		}

		buffer_copy_string_buffer(srv->tmp_buf, srv->srvconf.modules_dir);

		buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN("/"));
		buffer_append_string(srv->tmp_buf, modules);
#if defined(__WIN32) || defined(__CYGWIN__)
		buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN(".dll"));
#else
		buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN(".so"));
#endif

		p = plugin_init();
#ifdef __WIN32
		if (NULL == (p->lib = LoadLibrary(srv->tmp_buf->ptr))) {
			LPVOID lpMsgBuf;
			FormatMessage(
		        	FORMAT_MESSAGE_ALLOCATE_BUFFER |
		       		FORMAT_MESSAGE_FROM_SYSTEM,
		        	NULL,
		        	GetLastError(),
		        	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		        	(LPTSTR) &lpMsgBuf,
		        	0, NULL );

			log_error_write(srv, __FILE__, __LINE__, "ssb", "LoadLibrary() failed",
					lpMsgBuf, srv->tmp_buf);

			plugin_free(p);

			return -1;

		}
#else
		if (NULL == (p->lib = dlopen(srv->tmp_buf->ptr, RTLD_NOW|RTLD_GLOBAL))) {
			log_error_write(srv, __FILE__, __LINE__, "sbs", "dlopen() failed for:",
					srv->tmp_buf, dlerror());

			plugin_free(p);

			return -1;
		}

#endif
		buffer_reset(srv->tmp_buf);
		buffer_copy_string(srv->tmp_buf, modules);
		buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN("_plugin_init"));

#ifdef __WIN32
		init = GetProcAddress(p->lib, srv->tmp_buf->ptr);

		if (init == NULL)  {
			LPVOID lpMsgBuf;
			FormatMessage(
		        	FORMAT_MESSAGE_ALLOCATE_BUFFER |
		       		FORMAT_MESSAGE_FROM_SYSTEM,
		        	NULL,
		        	GetLastError(),
		        	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		        	(LPTSTR) &lpMsgBuf,
		        	0, NULL );

			log_error_write(srv, __FILE__, __LINE__, "sbs", "getprocaddress failed:", srv->tmp_buf, lpMsgBuf);

			plugin_free(p);
			return -1;
		}

#else
#if 1
		init = (int (*)(plugin *))(intptr_t)dlsym(p->lib, srv->tmp_buf->ptr);
#else
		*(void **)(&init) = dlsym(p->lib, srv->tmp_buf->ptr);
#endif
		if ((error = dlerror()) != NULL)  {
			log_error_write(srv, __FILE__, __LINE__, "s", error);

			plugin_free(p);
			return -1;
		}

#endif
		if ((*init)(p)) {
			log_error_write(srv, __FILE__, __LINE__, "ss", modules, "plugin init failed" );

			plugin_free(p);
			return -1;
		}
#if 0
		log_error_write(srv, __FILE__, __LINE__, "ss", modules, "plugin loaded" );
#endif
		plugins_register(srv, p);
	}

	return 0;
}
#endif

#define PLUGIN_TO_SLOT(x, y) \
	handler_t plugins_call_##y(server *srv, connection *con) {\
		plugin **slot;\
		size_t j;\
                if (!srv->plugin_slots) return HANDLER_GO_ON;\
                slot = ((plugin ***)(srv->plugin_slots))[x];\
		if (!slot) return HANDLER_GO_ON;\
		for (j = 0; j < srv->plugins.used && slot[j]; j++) { \
			plugin *p = slot[j];\
			handler_t r;\
			switch(r = p->y(srv, con, p->data)) {\
			case HANDLER_GO_ON:\
				break;\
			case HANDLER_FINISHED:\
			case HANDLER_COMEBACK:\
			case HANDLER_WAIT_FOR_EVENT:\
			case HANDLER_WAIT_FOR_FD:\
			case HANDLER_ERROR:\
				return r;\
			default:\
				log_error_write(srv, __FILE__, __LINE__, "sbs", #x, p->name, "unknown state");\
				return HANDLER_ERROR;\
			}\
		}\
		return HANDLER_GO_ON;\
	}

/**
 * plugins that use
 *
 * - server *srv
 * - connection *con
 * - void *p_d (plugin_data *)
 */

PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_URI_CLEAN, handle_uri_clean)
PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_URI_RAW, handle_uri_raw)
PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_REQUEST_DONE, handle_request_done)
PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_CONNECTION_CLOSE, handle_connection_close)
PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_SUBREQUEST, handle_subrequest)
PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_SUBREQUEST_START, handle_subrequest_start)
PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_JOBLIST, handle_joblist)
PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_DOCROOT, handle_docroot)
PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_PHYSICAL, handle_physical)
PLUGIN_TO_SLOT(PLUGIN_FUNC_CONNECTION_RESET, connection_reset)

#undef PLUGIN_TO_SLOT

#define PLUGIN_TO_SLOT(x, y) \
	handler_t plugins_call_##y(server *srv) {\
		plugin **slot;\
		size_t j;\
                if (!srv->plugin_slots) return HANDLER_GO_ON;\
                slot = ((plugin ***)(srv->plugin_slots))[x];\
		if (!slot) return HANDLER_GO_ON;\
		for (j = 0; j < srv->plugins.used && slot[j]; j++) { \
			plugin *p = slot[j];\
			handler_t r;\
			switch(r = p->y(srv, p->data)) {\
			case HANDLER_GO_ON:\
				break;\
			case HANDLER_FINISHED:\
			case HANDLER_COMEBACK:\
			case HANDLER_WAIT_FOR_EVENT:\
			case HANDLER_WAIT_FOR_FD:\
			case HANDLER_ERROR:\
				return r;\
			default:\
				log_error_write(srv, __FILE__, __LINE__, "sbsd", #x, p->name, "unknown state:", r);\
				return HANDLER_ERROR;\
			}\
		}\
		return HANDLER_GO_ON;\
	}

/**
 * plugins that use
 *
 * - server *srv
 * - void *p_d (plugin_data *)
 */

PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_TRIGGER, handle_trigger)
PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_SIGHUP, handle_sighup)
PLUGIN_TO_SLOT(PLUGIN_FUNC_CLEANUP, cleanup)
PLUGIN_TO_SLOT(PLUGIN_FUNC_SET_DEFAULTS, set_defaults)

#undef PLUGIN_TO_SLOT

#if 0
/**
 *
 * special handler
 *
 */
handler_t plugins_call_handle_fdevent(server *srv, const fd_conn *fdc) {
	size_t i;
	plugin **ps;

	ps = srv->plugins.ptr;

	for (i = 0; i < srv->plugins.used; i++) {
		plugin *p = ps[i];
		if (p->handle_fdevent) {
			handler_t r;
			switch(r = p->handle_fdevent(srv, fdc, p->data)) {
			case HANDLER_GO_ON:
				break;
			case HANDLER_FINISHED:
			case HANDLER_COMEBACK:
			case HANDLER_WAIT_FOR_EVENT:
			case HANDLER_ERROR:
				return r;
			default:
				log_error_write(srv, __FILE__, __LINE__, "d", r);
				break;
			}
		}
	}

	return HANDLER_GO_ON;
}
#endif
/**
 *
 * - call init function of all plugins to init the plugin-internals
 * - added each plugin that supports has callback to the corresponding slot
 *
 * - is only called once.
 */

handler_t plugins_call_init(server *srv) {
	size_t i;
	plugin **ps;

	ps = srv->plugins.ptr;

	/* fill slots */

	srv->plugin_slots = calloc(PLUGIN_FUNC_SIZEOF, sizeof(ps));

	for (i = 0; i < srv->plugins.used; i++) {
		size_t j;
		/* check which calls are supported */

		plugin *p = ps[i];

#define PLUGIN_TO_SLOT(x, y) \
	if (p->y) { \
		plugin **slot = ((plugin ***)(srv->plugin_slots))[x]; \
		if (!slot) { \
			slot = calloc(srv->plugins.used, sizeof(*slot));\
			((plugin ***)(srv->plugin_slots))[x] = slot; \
		} \
		for (j = 0; j < srv->plugins.used; j++) { \
			if (slot[j]) continue;\
			slot[j] = p;\
			break;\
		}\
	}


		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_URI_CLEAN, handle_uri_clean);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_URI_RAW, handle_uri_raw);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_REQUEST_DONE, handle_request_done);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_CONNECTION_CLOSE, handle_connection_close);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_TRIGGER, handle_trigger);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_SIGHUP, handle_sighup);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_SUBREQUEST, handle_subrequest);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_SUBREQUEST_START, handle_subrequest_start);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_JOBLIST, handle_joblist);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_DOCROOT, handle_docroot);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_HANDLE_PHYSICAL, handle_physical);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_CONNECTION_RESET, connection_reset);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_CLEANUP, cleanup);
		PLUGIN_TO_SLOT(PLUGIN_FUNC_SET_DEFAULTS, set_defaults);
#undef PLUGIN_TO_SLOT

		if (p->init) {
			if (NULL == (p->data = p->init())) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"plugin-init failed for module", p->name);
				return HANDLER_ERROR;
			}

			/* used for con->mode, DIRECT == 0, plugins above that */
			((plugin_data *)(p->data))->id = i + 1;

			if (p->version != LIGHTTPD_VERSION_ID) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"plugin-version doesn't match lighttpd-version for", p->name);
				return HANDLER_ERROR;
			}
		} else {
			p->data = NULL;
		}
	}

	return HANDLER_GO_ON;
}

void plugins_free(server *srv) {
	size_t i;
	plugins_call_cleanup(srv);

	for (i = 0; i < srv->plugins.used; i++) {
		plugin *p = ((plugin **)srv->plugins.ptr)[i];

		plugin_free(p);
	}

	for (i = 0; srv->plugin_slots && i < PLUGIN_FUNC_SIZEOF; i++) {
		plugin **slot = ((plugin ***)(srv->plugin_slots))[i];

		if (slot) free(slot);
	}

	free(srv->plugin_slots);
	srv->plugin_slots = NULL;

	free(srv->plugins.ptr);
	srv->plugins.ptr = NULL;
	srv->plugins.used = 0;
}
