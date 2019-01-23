#ifndef AA_RTSP_H
#define AA_RTSP_H

#include <client.h>

//#define HANDLE_REPLY

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum rtsp_state_e
	{
		RTSP_UNKOWN_STATE,    /**< UNKOWN state, for initializing RTSP handshake.   */
		RTSP_OPTIONS_STATE,    /**< OPTIONS state, for establishing dialogs.   */
		RTSP_DESCRIBE_STATE,    /**< DESCRIBE state, for cancelling request.	    */
		RTSP_SETUP_STATE,	    /**< SETUP state.				    */
		RTSP_PLAY_STATE,	    /**< PLAY state, for terminating dialog.	    */
		RTSP_TEARDOWN_STATE,	    /**< PLAY state, for terminating dialog.	    */

	} rtsp_state_e;

	typedef enum rtsp_method_e
	{
		RTSP_OPTIONS_METHOD,    /**< OPTIONS method, for getting request types the server will accept.   */
		RTSP_DESCRIBE_METHOD,    /**< DESCRIBE method, for advertising the media.	    */
		RTSP_SETUP_METHOD,	    /**< SETUP method, for setting up client and server ports.				    */
		RTSP_PLAY_METHOD,	    /**< PLAY method, for playing media.	    */
		RTSP_PAUSE_METHOD,  /**< PAUSE method.			    */
		RTSP_RECORD_METHOD,  /**< RECORD method.			    */
		RTSP_ANNOUNCE_METHOD,  /**< ANNOUNCE method.			    */
		RTSP_TEARDOWN_METHOD,   /**< TEARDOWN method.			    */
		RTSP_GET_PARAMETER_METHOD,  /**< GET_PARAMETER method.			    */
		RTSP_SET_PARAMETER_METHOD,  /**< SET_PARAMETER method.			    */
		RTSP_REDIRECT_METHOD,  /**< REDIRECT method.			    */

		RTSP_OTHER_METHOD	    /**< Other method.				    */

	} rtsp_method_e;

	typedef struct rtsp_method
	{
		rtsp_method_e id;	    /**< Method ID, from \a rtsp_method_e. */
		char	   *name;    /**< Method name, which will always contain the
						 method string. */
		int         name_len;   /**< The length of method name.*/
	} rtsp_method;

	const rtsp_method rtsp_options_method = { RTSP_OPTIONS_METHOD, "OPTIONS", 7 };
	const rtsp_method rtsp_decribe_method = { RTSP_DESCRIBE_METHOD, "DESCRIBE", 8 };
	const rtsp_method rtsp_setup_method = { RTSP_SETUP_METHOD, "SETUP", 5 };
	const rtsp_method rtsp_play_method = { RTSP_PLAY_METHOD, "PLAY", 4 };
	const rtsp_method rtsp_pause_method = { RTSP_PAUSE_METHOD, "PAUSE", 5 };
	const rtsp_method rtsp_record_method = { RTSP_RECORD_METHOD, "RECORD", 6 };
	const rtsp_method rtsp_announce_method = { RTSP_ANNOUNCE_METHOD, "ANNOUNCE", 8 };
	const rtsp_method rtsp_teardown_method = { RTSP_TEARDOWN_METHOD, "TEARDOWN", 8 };
	const rtsp_method rtsp_get_parameter_method = { RTSP_GET_PARAMETER_METHOD, "GET_PARAMETER", 13 };
	const rtsp_method rtsp_set_parameter_method = { RTSP_SET_PARAMETER_METHOD, "SET_PARAMETER", 13 };
	const rtsp_method rtsp_redirect_method = { RTSP_REDIRECT_METHOD, "EDIRECT", 7 };

#define RTSP_IS_METHOD(data, data_len, method) (data_len >= method.name_len && strncmp(data, method.name, method.name_len) == 0)
#define IP_REPLACEMENT "127.0.0.1"

	typedef struct rtsp_line {
		char *line;
		int len;
	} rtsp_line;

	void rtsp_free(rtsp_line **line);
#ifdef HANDLE_REPLY
	int rtsp_handle_describe_reply(void *pkt, int *pkt_len);
	int rtsp_handle_setup_reply(void *pkt, int *pkt_len);
#endif
	int add_rtsp_tnl_ports(int inst_id, int call_id, char *port1, char *port2, int parent_client_id);
	int rtsp_handle_setup_request(client_t *c, void *pkt, int *pkt_len);
	int rtsp_handle_teardown_request(client_t *c, void *pkt, int *pkt_len);


#define p_rtsp_free ((void (*)(void **))&rtsp_free)

#ifdef __cplusplus
}
#endif

#endif AA_RTSP_H