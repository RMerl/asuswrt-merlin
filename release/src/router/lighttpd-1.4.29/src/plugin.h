#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include "base.h"
#include "buffer.h"

#define SERVER_FUNC(x) \
		static handler_t x(server *srv, void *p_d)

#define CONNECTION_FUNC(x) \
		static handler_t x(server *srv, connection *con, void *p_d)

#define INIT_FUNC(x) \
		static void *x()

#define FREE_FUNC          SERVER_FUNC
#define TRIGGER_FUNC       SERVER_FUNC
#define SETDEFAULTS_FUNC   SERVER_FUNC
#define SIGHUP_FUNC        SERVER_FUNC

#define SUBREQUEST_FUNC    CONNECTION_FUNC
#define JOBLIST_FUNC       CONNECTION_FUNC
#define PHYSICALPATH_FUNC  CONNECTION_FUNC
#define REQUESTDONE_FUNC   CONNECTION_FUNC
#define URIHANDLER_FUNC    CONNECTION_FUNC

#define PLUGIN_DATA        size_t id

typedef struct {
	size_t version;

	buffer *name; /* name of the plugin */

	void *(* init)                       ();
	handler_t (* set_defaults)           (server *srv, void *p_d);
	handler_t (* cleanup)                (server *srv, void *p_d);
	                                                                                   /* is called ... */
	handler_t (* handle_trigger)         (server *srv, void *p_d);                     /* once a second */
	handler_t (* handle_sighup)          (server *srv, void *p_d);                     /* at a signup */

	handler_t (* handle_uri_raw)         (server *srv, connection *con, void *p_d);    /* after uri_raw is set */
	handler_t (* handle_uri_clean)       (server *srv, connection *con, void *p_d);    /* after uri is set */
	handler_t (* handle_docroot)         (server *srv, connection *con, void *p_d);    /* getting the document-root */
	handler_t (* handle_physical)        (server *srv, connection *con, void *p_d);    /* mapping url to physical path */
	handler_t (* handle_request_done)    (server *srv, connection *con, void *p_d);    /* at the end of a request */
	handler_t (* handle_connection_close)(server *srv, connection *con, void *p_d);    /* at the end of a connection */
	handler_t (* handle_joblist)         (server *srv, connection *con, void *p_d);    /* after all events are handled */



	handler_t (* handle_subrequest_start)(server *srv, connection *con, void *p_d);

	                                                                                   /* when a handler for the request
											    * has to be found
											    */
	handler_t (* handle_subrequest)      (server *srv, connection *con, void *p_d);    /* */
	handler_t (* connection_reset)       (server *srv, connection *con, void *p_d);    /* */
	void *data;

	/* dlopen handle */
	void *lib;
} plugin;

int plugins_load(server *srv);
void plugins_free(server *srv);

handler_t plugins_call_handle_uri_raw(server *srv, connection *con);
handler_t plugins_call_handle_uri_clean(server *srv, connection *con);
handler_t plugins_call_handle_subrequest_start(server *srv, connection *con);
handler_t plugins_call_handle_subrequest(server *srv, connection *con);
handler_t plugins_call_handle_request_done(server *srv, connection *con);
handler_t plugins_call_handle_docroot(server *srv, connection *con);
handler_t plugins_call_handle_physical(server *srv, connection *con);
handler_t plugins_call_handle_connection_close(server *srv, connection *con);
handler_t plugins_call_handle_joblist(server *srv, connection *con);
handler_t plugins_call_connection_reset(server *srv, connection *con);

handler_t plugins_call_handle_trigger(server *srv);
handler_t plugins_call_handle_sighup(server *srv);

handler_t plugins_call_init(server *srv);
handler_t plugins_call_set_defaults(server *srv);
handler_t plugins_call_cleanup(server *srv);

int config_insert_values_global(server *srv, array *ca, const config_values_t *cv);
int config_insert_values_internal(server *srv, array *ca, const config_values_t *cv);
int config_setup_connection(server *srv, connection *con);
int config_patch_connection(server *srv, connection *con, comp_key_t comp);
int config_check_cond(server *srv, connection *con, data_config *dc);
int config_append_cond_match_buffer(connection *con, data_config *dc, buffer *buf, int n);

#endif
