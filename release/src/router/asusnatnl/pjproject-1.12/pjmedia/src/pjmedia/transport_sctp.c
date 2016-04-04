/* $Id: transport_sctp.c 4387 2013-02-27 10:16:08Z ming $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA 
 */

#include <pjmedia/natnl_stream.h>
#include <pjmedia/transport_sctp.h>
#include <pjmedia/endpoint.h>
#include <pjlib-util/base64.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/ssl_sock.h>
#include <pj/math.h>
#include <pjlib.h>

#ifdef WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#define in_port_t u_short
#define ssize_t int
#else
#include <sys/socket.h>
#endif

/* 
 * Include libusrsctp headers 
 */
#include <usrsctp.h>
#include <netinet/sctp_constants.h>

//#define BYPASS 1
//#define DUMP_PACKET 1

#define THIS_FILE   "transport_sctp.c"
#define DEFAULT_SCTP_LOCAL_PORT 5000

//#define USE_GLOBAL_LOCK
//#define USE_GLOBAL_CTX
#define USE_OUTPUT_THREAD

//#define ENABLE_DELAYED_SEND 0

// Copied from Network Security with OpenSSL

#if defined(WIN32)
#define MUTEX_TYPE HANDLE
#define MUTEX_SETUP(x) (x) = CreateMutex(NULL, FALSE, NULL)
#define MUTEX_CLEANUP(x) CloseHandle(x)
#define MUTEX_LOCK(x) //WaitForSingleObject((x), INFINITE)
#define MUTEX_UNLOCK(x) //ReleaseMutex(x)
#define THREAD_ID GetCurrentThreadId( )
#elif defined(PJ_LINUX) || defined(PJ_ANDROID) || defined(PJ_DARWINOS)
#include <pthread.h>
#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x) //pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x) //pthread_mutex_unlock(&(x))
#define THREAD_ID pthread_self( )
#else
#error You must define mutex operations appropriate for your platform!
#endif

static MUTEX_TYPE *mutex_buf = NULL;
#ifdef USE_GLOBAL_CTX
static SSL_CTX *g_ctx = NULL;
#endif

/*
 * SCTP state enumeration.
 */
enum sctp_state {
	SCTP_STATE_CLOSED,
	SCTP_STATE_OPENED,
	SCTP_STATE_CONNECTING,
	SCTP_STATE_CONNECTED
};

/*
 * Internal timer types.
 */
enum timer_id
{
	TIMER_NONE,
    TIMER_CLOSE
};

//#define UDT_MAX_PKT_LEN 1600
typedef struct data_buff
{
	PJ_DECL_LIST_MEMBER(struct data_buff);
	pj_uint8_t   buff[UDT_MAX_PKT_LEN];
	pj_ssize_t   len;
} sctp_data_buff;

/*
 * Structure of SSL socket read buffer.
 */
typedef struct read_data_t
{
    void		 *data;
    pj_size_t		  len;
} read_data_t;

/*
 * Get the offset of pointer to read-buffer of SSL socket from read-buffer
 * of active socket. Note that both SSL socket and active socket employ 
 * different but correlated read-buffers (as much as async_cnt for each),
 * and to make it easier/faster to find corresponding SSL socket's read-buffer
 * from known active socket's read-buffer, the pointer of corresponding 
 * SSL socket's read-buffer is stored right after the end of active socket's
 * read-buffer.
 */
#define OFFSET_OF_READ_DATA_PTR(sctp, rbuf) \
					(read_data_t**) \
					((pj_int8_t*)(rbuf) + \
					sctp->setting.read_buffer_size)

/*
 * Structure of SSL socket write data.
 */
typedef struct write_data_t {
    PJ_DECL_LIST_MEMBER(struct write_data_t);
    pj_size_t 	 	 record_len;
    pj_size_t 	 	 plain_data_len;
    pj_size_t 	 	 data_len;
    unsigned		 flags;
    union {
	char		 content[1];
	const char	*ptr;
    } data;
} write_data_t;

/*
 * Structure of SSL socket write buffer (circular buffer).
 */
typedef struct send_buf_t {
    char		*buf;
    pj_size_t		 max_len;    
    char		*start;
    pj_size_t		 len;
} send_buf_t;

typedef struct transport_sctp
{
    pjmedia_transport	 base;		    /**< Base transport interface.  */
	pj_pool_t		*pool;		    /**< Pool for transport SCTP.   */
	MUTEX_TYPE		mutex;		    /**< Mutex for libsctp contexts.*/
	//pj_lock_t		*write_mutex;		    /**< Mutex for libsctp contexts.*/
    pjmedia_sctp_setting setting;
    unsigned		media_option;
	pj_sockaddr		rem_addr;

	struct socket *sctp_sock;
	struct socket *sctp_server_sock;
	pj_uint16_t      sctp_stream_id;
	pj_uint16_t      sctp_ppid;

	/* SCTP policy */
	pj_bool_t		 sctp_inited;
	pj_bool_t		 session_inited;
    pj_bool_t		 offerer_side;
    pj_bool_t		 bypass_sctp;

    /* Stream information */
    void		*user_data;
    void		(*rtp_cb)( void *user_data,
				   void *pkt,
				   pj_ssize_t size);
    void		(*rtcp_cb)(void *user_data,
				   void *pkt,
				   pj_ssize_t size);
        
    /* Transport information */
    pjmedia_transport	*member_tp; /**< Underlying transport.       */

	sctp_data_buff       sbuff;             //used buffer list
	pj_mutex_t		    *sbuff_mutex;		//Send mutex
	pj_sem_t            *sbuff_sem;
	int					 sbuff_cnt;         //used buffer count

	pj_mutex_t		    *gcbuff_mutex;		//unused mutex
	sctp_data_buff       gcbuff;            //unused buffer list

#ifdef USE_OUTPUT_THREAD
	pj_thread_t         *output_thread;
	pj_uint8_t           thread_quit_flag; /**< Quit signal to thread */
#endif

    /* SRTP usage policy of peer. This field is updated when media is starting.
     * This is useful when SRTP is in optional mode and peer is using mandatory
     * mode, so when local is about to reinvite/update, it should offer 
     * RTP/SAVP instead of offering RTP/AVP.
     */
    pjmedia_sctp_use	 peer_use;

	unsigned long	  last_err;

	enum sctp_state	  state;
	pj_timer_entry	  timer;
	pj_size_t		  read_size;

	write_data_t	  write_pending;/* list of pending write to OpenSSL */
	write_data_t	  write_pending_empty; /* cache for write_pending   */
	pj_bool_t		  flushing_write_pend; /* flag of flushing is ongoing*/
	send_buf_t		  send_buf;
	write_data_t	  send_pending;	/* list of pending write to network */

	pj_bool_t         media_type_app; // If true, the media type is application for WebRTC.

	int				  delayed_send_size;

	pj_sockaddr *turn_mapped_addr;  // Save turn mapped address for notifying upper stack.
} transport_sctp;

/*
 * This callback is called by transport when incoming rtp is received
 */
static void sctp_rtp_cb( void *user_data, void *pkt, pj_ssize_t size);

/*
 * This callback is called by transport when incoming rtcp is received
 */
static void sctp_rtcp_cb( void *user_data, void *pkt, pj_ssize_t size);

static pj_status_t set_cipher_list(transport_sctp *sctp);

#ifdef ENABLE_DELAYED_SEND
static write_data_t* alloc_send_data(transport_sctp *sctp, pj_size_t len);
static void free_send_data(transport_sctp *sctp, write_data_t *wdata);
static pj_status_t flush_delayed_send(transport_sctp *sctp);
#endif

static const pj_str_t ID_TNL_PLAIN  = { "RTP/AVP", 7 };
static const pj_str_t ID_TNL_DTLS = { "RTP/SAVP", 8 };
static const pj_str_t ID_TNL_DTLS_SCTP = { "DTLS/SCTP", 9 };
static const pj_str_t ID_TNL_PLAIN_SCTP = { "RTP/SCTP", 8 }; // dean : RTP/SCTP is for UDT replacement

#ifdef USE_GLOBAL_LOCK
static MUTEX_TYPE global_mutex;
#endif

/*
 * These are media transport operations.
 */
static pj_status_t transport_get_info (pjmedia_transport *tp,
				       pjmedia_transport_info *info);
static pj_status_t transport_attach   (pjmedia_transport *tp,
				       void *user_data,
				       const pj_sockaddr_t *rem_addr,
				       const pj_sockaddr_t *rem_rtcp,
				       unsigned addr_len,
				       void (*rtp_cb)(void*,
						      void*,
						      pj_ssize_t),
				       void (*rtcp_cb)(void*,
						       void*,
						       pj_ssize_t));
static void	   transport_detach   (pjmedia_transport *tp,
				       void *strm);
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size);
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size);
static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
				       const pj_sockaddr_t *addr,
				       unsigned addr_len,
				       const void *pkt,
				       pj_size_t size);
static pj_status_t transport_media_create(pjmedia_transport *tp,
				       pj_pool_t *sdp_pool,
				       unsigned options,
				       const pjmedia_sdp_session *sdp_remote,
				       unsigned media_index);
static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
				       pj_pool_t *sdp_pool,
				       pjmedia_sdp_session *sdp_local,
				       const pjmedia_sdp_session *sdp_remote,
				       unsigned media_index);
static pj_status_t transport_media_start (pjmedia_transport *tp,
				       pj_pool_t *pool,
				       const pjmedia_sdp_session *sdp_local,
				       const pjmedia_sdp_session *sdp_remote,
				       unsigned media_index);
static pj_status_t transport_media_stop(pjmedia_transport *tp);
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
				       pjmedia_dir dir,
				       unsigned pct_lost);
static pj_status_t transport_destroy  (pjmedia_transport *tp);



static pjmedia_transport_op transport_sctp_op = 
{
    &transport_get_info,
    &transport_attach,
    &transport_detach,
    &transport_send_rtp,
    &transport_send_rtcp,
    &transport_send_rtcp2,
    &transport_media_create,
    &transport_encode_sdp,
    &transport_media_start,
    &transport_media_stop,
    &transport_simulate_lost,
    &transport_destroy
};
/*
 *******************************************************************
 * Static/internal functions.
 *******************************************************************
 */

#if 0

/**
 * Mapping from OpenSSL error codes to pjlib error space.
 */

#define PJ_SSL_ERRNO_START		(PJ_ERRNO_START_USER + \
					 PJ_ERRNO_SPACE_SIZE*6)

#define PJ_SSL_ERRNO_SPACE_SIZE		PJ_ERRNO_SPACE_SIZE

/* Expected maximum value of reason component in OpenSSL error code */
#define MAX_OSSL_ERR_REASON		1200

static pj_status_t STATUS_FROM_SSL_ERR(transport_sctp *sctp,
				       unsigned long err)
{
    pj_status_t status;

    /* General SSL error, dig more from OpenSSL error queue */
    if (err == SSL_ERROR_SSL)
	err = ERR_get_error();

    /* OpenSSL error range is much wider than PJLIB errno space, so
     * if it exceeds the space, only the error reason will be kept.
     * Note that the last native error will be kept as is and can be
     * retrieved via SSL socket info.
     */
    status = ERR_GET_LIB(err)*MAX_OSSL_ERR_REASON + ERR_GET_REASON(err);
    if (status > PJ_SSL_ERRNO_SPACE_SIZE)
	status = ERR_GET_REASON(err);

    status += PJ_SSL_ERRNO_START;
    sctp->last_err = err;
    return status;
}

static pj_status_t GET_SSL_STATUS(transport_sctp *sctp)
{
    return STATUS_FROM_SSL_ERR(sctp, ERR_get_error());
}


/*
 * Get error string of OpenSSL.
 */
static pj_str_t ssl_strerror(pj_status_t status, 
			     char *buf, pj_size_t bufsize)
{
    pj_str_t errstr;
    unsigned long ssl_err = status;

    if (ssl_err) {
	unsigned long l, r;
	ssl_err -= PJ_SSL_ERRNO_START;
	l = ssl_err / MAX_OSSL_ERR_REASON;
	r = ssl_err % MAX_OSSL_ERR_REASON;
	ssl_err = ERR_PACK(l, 0, r);
    }

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)

    {
	const char *tmp = NULL;
	    tmp = ERR_reason_error_string(ssl_err);
	if (tmp) {
	    pj_ansi_strncpy(buf, tmp, bufsize);
	    errstr = pj_str(buf);
	    return errstr;
	}
    }

#endif	/* PJ_HAS_ERROR_STRING */

    errstr.ptr = buf;
    errstr.slen = pj_ansi_snprintf(buf, bufsize, 
				   "Unknown OpenSSL error %lu",
				   ssl_err);

    return errstr;
}
#endif

/* usrsctp library initialization counter */
static int usrsctp_init_count;

static int sctp_data_input(struct socket* sock, union sctp_sockstore addr,
							void *data, size_t datalen,
							struct sctp_rcvinfo rcv, int flags, void *ulp_info);

static void sctp_server_create(pjmedia_transport *tp)
{
	int mode, rc;
	pj_status_t status;
	char *openssl_ver;
	struct sctp_initmsg initmsg;
	struct linger l;
	struct sctp_assoc_value av;
	struct sctp_event event;
	uint32_t i;
	socklen_t len;
	struct sockaddr_conn addr;
	transport_sctp *sctp = (transport_sctp *)tp;
	//struct sockaddr_in addr;
	int r;

	uint16_t event_types[] = {SCTP_ASSOC_CHANGE,
		SCTP_PEER_ADDR_CHANGE,
		SCTP_REMOTE_ERROR,
		SCTP_SHUTDOWN_EVENT,
		SCTP_ADAPTATION_INDICATION,
		SCTP_SEND_FAILED_EVENT,
		SCTP_STREAM_RESET_EVENT,
		SCTP_STREAM_CHANGE_EVENT};

	// Open sctp with a callback
	if ((sctp->sctp_server_sock = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, 
		sctp_data_input, NULL, 0, tp)) == NULL) {
			return;
	}

	// Make non-blocking for bind/connect.  SCTP over UDP defaults to non-blocking
	// in associations for normal IO
	if (usrsctp_set_non_blocking(sctp->sctp_server_sock, 0) < 0) {
		PJ_LOG(1, (THIS_FILE, "sctp_server_create() Couldn't set non_blocking on SCTP socket"));
		// We can't handle connect() safely if it will block, not that this will
		// even happen.
		goto error_cleanup;
	}

	// Make sure when we close the socket, make sure it doesn't call us back again!
	// This would cause it try to use an invalid DataChannelConnection pointer
	l.l_onoff = 1;
	l.l_linger = 0;
	if (usrsctp_setsockopt(sctp->sctp_server_sock, SOL_SOCKET, SO_LINGER,
		(const void *)&l, (socklen_t)sizeof(struct linger)) < 0) {
			PJ_LOG(1, (THIS_FILE, "sctp_server_create() Couldn't set SO_LINGER on SCTP socket"));
			// unsafe to allow it to continue if this fails
			goto error_cleanup;
	}

	// XXX Consider disabling this when we add proper SDP negotiation.
	// We may want to leave enabled for supporting 'cloning' of SDP offers, which
	// implies re-use of the same pseudo-port number, or forcing a renegotiation.
	{
		uint32_t on = 1;
		if (usrsctp_setsockopt(sctp->sctp_server_sock, IPPROTO_SCTP, SCTP_REUSE_PORT,
			(const void *)&on, (socklen_t)sizeof(on)) < 0) {
				PJ_LOG(1, (THIS_FILE, "sctp_server_create() Couldn't set SCTP_REUSE_PORT on SCTP socket"));
		}
		if (usrsctp_setsockopt(sctp->sctp_server_sock, IPPROTO_SCTP, SCTP_NODELAY,
			(const void *)&on, (socklen_t)sizeof(on)) < 0) {
				PJ_LOG(1, (THIS_FILE, "sctp_server_create() Couldn't set SCTP_NODELAY on SCTP socket"));
		}
	}

	av.assoc_id = SCTP_ALL_ASSOC;
	av.assoc_value = SCTP_ENABLE_RESET_STREAM_REQ | SCTP_ENABLE_CHANGE_ASSOC_REQ;
	if (usrsctp_setsockopt(sctp->sctp_server_sock, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &av,
		(socklen_t)sizeof(struct sctp_assoc_value)) < 0) {
			PJ_LOG(1, (THIS_FILE, "*** sctp_server_create() failed enable stream reset errno %d", errno));
			goto error_cleanup;
	}

	/* Enable the events of interest. */
	memset(&event, 0, sizeof(event));
	event.se_assoc_id = SCTP_ALL_ASSOC;
	event.se_on = 1;
	for (i = 0; i < sizeof(event_types)/sizeof(event_types[0]); ++i) {
		event.se_type = event_types[i];
		if (usrsctp_setsockopt(sctp->sctp_server_sock, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(event)) < 0) {
			PJ_LOG(1, (THIS_FILE, "*** sctp_server_create() failed setsockopt SCTP_EVENT errno %d", errno));
			goto error_cleanup;
		}
	}

#if 0
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
#if defined(__Userspace_os_Darwin)
	addr.sconn_len = sizeof(addr);
#endif
	addr.sin_port = htons(DEFAULT_SCTP_LOCAL_PORT);
	addr.sin_addr.s_addr = htonl(ADDR_ANY);
#else
	memset(&addr, 0, sizeof(addr));
	addr.sconn_family = AF_CONN;
#if defined(__Userspace_os_Darwin)
	addr.sconn_len = sizeof(addr);
#endif
	addr.sconn_port = htons(DEFAULT_SCTP_LOCAL_PORT);
	addr.sconn_addr = tp;
#endif

	PJ_LOG(4, (THIS_FILE, "sctp_server_create() Calling usrsctp_bind"));
	r = usrsctp_bind(sctp->sctp_server_sock, (struct sockaddr *)(&addr), sizeof(addr));
	if (r < 0) {
		PJ_LOG(1, (THIS_FILE, "sctp_server_create() usrsctp_bind failed: %d", r));
	} else {
		// This is the remote addr
		//addr.sconn_port = htons(DEFAULT_SCTP_LOCAL_PORT);
		PJ_LOG(4, (THIS_FILE, "sctp_server_create() Calling usrsctp_listen"));
		r = usrsctp_listen(sctp->sctp_server_sock, 1);
		/*if (r >= 0 || errno == EINPROGRESS) {
			struct sctp_paddrparams paddrparams;
			socklen_t opt_len;

			memset(&paddrparams, 0, sizeof(struct sctp_paddrparams));
			memcpy(&paddrparams.spp_address, &addr, sizeof(struct sockaddr_conn));
			opt_len = (socklen_t)sizeof(struct sctp_paddrparams);
			r = usrsctp_getsockopt(sctp->sctp_server_sock, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
				&paddrparams, &opt_len);
			if (r < 0) {
				PJ_LOG(1, (THIS_FILE, "sctp_server_create()	usrsctp_getsockopt failed: %d", r));
			} else {
				// draft-ietf-rtcweb-data-channel-13 section 5: max initial MTU IPV4 1200, IPV6 1280
				paddrparams.spp_pathmtu = 1400; // safe for either
				paddrparams.spp_flags &= ~SPP_PMTUD_ENABLE;
				paddrparams.spp_flags |= SPP_PMTUD_DISABLE;
				opt_len = (socklen_t)sizeof(struct sctp_paddrparams);
				r = usrsctp_setsockopt(sctp->sctp_server_sock, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
					&paddrparams, opt_len);
				if (r < 0) {
					PJ_LOG(1, (THIS_FILE, "sctp_server_create() usrsctp_getsockopt failed: %d", r));
				} else {
					PJ_LOG(4, (THIS_FILE, "sctp_server_create() usrsctp: PMTUD disabled, MTU set to %u", paddrparams.spp_pathmtu));
				}
			}
		}*/
		if (r < 0) {
			if (errno == EINPROGRESS) {
				// non-blocking
				return;
			} else {
				PJ_LOG(1, (THIS_FILE, "sctp_server_create() usrsctp_listen failed: %d", errno));
			}
		} else {
			// We set Even/Odd and fire ON_CONNECTION via SCTP_COMM_UP when we get that
			// This also avoids issues with calling TransportFlow stuff on Mainthread
			return;
		}
	}

	return;

error_cleanup:
	usrsctp_close(sctp->sctp_server_sock);
	return;
}

static void sctp_debug_printf(const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	PJ_LOG(5, (THIS_FILE, format, arg));
	va_end(arg);
}

void handle_assoc_change_event(transport_sctp *sctp, const struct sctp_assoc_change *sac)
{
	uint32_t i, n;
	if (!sctp)
		return;

	switch (sac->sac_state) {
	  case SCTP_COMM_UP:
		  PJ_LOG(4, (THIS_FILE, "Association change: SCTP_COMM_UP"));
		  if (sctp->state == SCTP_STATE_OPENED || sctp->state == SCTP_STATE_CONNECTING) {
			  PJ_LOG(4, (THIS_FILE, "HandleAssocChangeEvent SCTP connected."));
			  sctp->state = SCTP_STATE_CONNECTED;
			  sctp->session_inited = PJ_TRUE;
			  if (sctp->setting.cb.on_sctp_connection_complete)
				  sctp->setting.cb.on_sctp_connection_complete(sctp, PJ_SUCCESS, sctp->turn_mapped_addr);
		  } else if (sctp->state == SCTP_STATE_CONNECTED) {
			  PJ_LOG(4, (THIS_FILE, "HandleAssocChangeEvent SCTP already connected."));
		  } else {
			  PJ_LOG(4, (THIS_FILE, "Unexpected state: %d", sctp->state));
		  }
		  break;
	  case SCTP_COMM_LOST:
		  PJ_LOG(4, (THIS_FILE, "Association change: SCTP_COMM_LOST"));
		  // This association is toast, so also close all the channels -- from mainthread!
		  /*NS_DispatchToMainThread(do_AddRef(new DataChannelOnMessageAvailable(
			  DataChannelOnMessageAvailable::ON_DISCONNECTED,
			  this)));*/
		  break;
	  case SCTP_RESTART:
		  PJ_LOG(4, (THIS_FILE, "Association change: SCTP_RESTART"));
		  break;
	  case SCTP_SHUTDOWN_COMP:
		  PJ_LOG(4, (THIS_FILE, "Association change: SCTP_SHUTDOWN_COMP"));
		  /*NS_DispatchToMainThread(do_AddRef(new DataChannelOnMessageAvailable(
			  DataChannelOnMessageAvailable::ON_DISCONNECTED,
			  this)));*/
		  break;
	  case SCTP_CANT_STR_ASSOC:
		  PJ_LOG(4, (THIS_FILE, "Association change: SCTP_CANT_STR_ASSOC"));
		  break;
	  default:
		  PJ_LOG(4, (THIS_FILE, "Association change: UNKNOWN"));
		  break;
	}
	PJ_LOG(4, (THIS_FILE, "Association change: streams (in/out) = (%u/%u)",
		sac->sac_inbound_streams, sac->sac_outbound_streams));

	//NS_ENSURE_TRUE_VOID(sac);
	n = sac->sac_length - sizeof(*sac);
	if (((sac->sac_state == SCTP_COMM_UP) ||
		(sac->sac_state == SCTP_RESTART)) && (n > 0)) {
			for (i = 0; i < n; ++i) {
				switch (sac->sac_info[i]) {
				  case SCTP_ASSOC_SUPPORTS_PR:
					  PJ_LOG(4, (THIS_FILE, "Supports: PR"));
					  break;
				  case SCTP_ASSOC_SUPPORTS_AUTH:
					  PJ_LOG(4, (THIS_FILE, "Supports: AUTH"));
					  break;
				  case SCTP_ASSOC_SUPPORTS_ASCONF:
					  PJ_LOG(4, (THIS_FILE, "Supports: ASCONF"));
					  break;
				  case SCTP_ASSOC_SUPPORTS_MULTIBUF:
					  PJ_LOG(4, (THIS_FILE, "Supports: MULTIBUF"));
					  break;
				  case SCTP_ASSOC_SUPPORTS_RE_CONFIG:
					  PJ_LOG(4, (THIS_FILE, "Supports: RE-CONFIG"));
					  break;
				  default:
					  PJ_LOG(4, (THIS_FILE, "Supports: UNKNOWN(0x%02x)", sac->sac_info[i]));
					  break;
				}
			}
	} else if (((sac->sac_state == SCTP_COMM_LOST) ||
		(sac->sac_state == SCTP_CANT_STR_ASSOC)) && (n > 0)) {
			PJ_LOG(4, (THIS_FILE, "Association: ABORT ="));
			for (i = 0; i < n; ++i) {
				PJ_LOG(4, (THIS_FILE, " 0x%02x", sac->sac_info[i]));
			}
	}
	if ((sac->sac_state == SCTP_CANT_STR_ASSOC) ||
		(sac->sac_state == SCTP_SHUTDOWN_COMP) ||
		(sac->sac_state == SCTP_COMM_LOST)) {
			return;
	}
}

void HandleSendFailedEvent(const struct sctp_send_failed_event *ssfe)
{
	size_t i, n;

	if (ssfe->ssfe_flags & SCTP_DATA_UNSENT) {
		PJ_LOG(4, (THIS_FILE, "HandleSendFailedEvent Unsent "));
	}
	if (ssfe->ssfe_flags & SCTP_DATA_SENT) {
		PJ_LOG(4, (THIS_FILE, "HandleSendFailedEvent Sent "));
	}
	if (ssfe->ssfe_flags & ~(SCTP_DATA_SENT | SCTP_DATA_UNSENT)) {
		PJ_LOG(4, (THIS_FILE, "HandleSendFailedEvent (flags = %x) ", ssfe->ssfe_flags));
	}
	PJ_LOG(4, (THIS_FILE, "HandleSendFailedEvent message with PPID = %u, SID = %d, flags: 0x%04x due to error = 0x%08x",
		ntohl(ssfe->ssfe_info.snd_ppid), ssfe->ssfe_info.snd_sid,
		ssfe->ssfe_info.snd_flags, ssfe->ssfe_error));
	n = ssfe->ssfe_length - sizeof(struct sctp_send_failed_event);
	for (i = 0; i < n; ++i) {
		PJ_LOG(4, (THIS_FILE, " 0x%02x", ssfe->ssfe_data[i]));
	}
}

void HandleRemoteErrorEvent(const struct sctp_remote_error *sre)
{
	size_t i, n;

	n = sre->sre_length - sizeof(struct sctp_remote_error);
	PJ_LOG(4, (THIS_FILE, "HandleRemoteErrorEvent Remote Error (error = 0x%04x): ", sre->sre_error));
	for (i = 0; i < n; ++i) {
		PJ_LOG(4, (THIS_FILE, " 0x%02x", sre-> sre_data[i]));
	}
}

void HandleNotification(transport_sctp *sctp, const union sctp_notification *notif, size_t n)
{
	//mLock.AssertCurrentThreadOwns();
	if (notif->sn_header.sn_length != (uint32_t)n) {
		return;
	}
	switch (notif->sn_header.sn_type) {
  case SCTP_ASSOC_CHANGE:
	  handle_assoc_change_event(sctp, &(notif->sn_assoc_change));
	  break;
  case SCTP_PEER_ADDR_CHANGE:
	  //HandlePeerAddressChangeEvent(&(notif->sn_paddr_change));
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_PEER_ADDR_CHANGE"));
	  break;
  case SCTP_REMOTE_ERROR:
	  HandleRemoteErrorEvent(&(notif->sn_remote_error));
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_REMOTE_ERROR"));
	  break;
  case SCTP_SHUTDOWN_EVENT:
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_SHUTDOWN_EVENT"));
	  break;
  case SCTP_ADAPTATION_INDICATION:
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_ADAPTATION_INDICATION"));
	  break;
  case SCTP_PARTIAL_DELIVERY_EVENT:
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_PARTIAL_DELIVERY_EVENT"));
	  break;
  case SCTP_AUTHENTICATION_EVENT:
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_AUTHENTICATION_EVENT"));
	  break;
  case SCTP_SENDER_DRY_EVENT:
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_PEER_ADDR_CHANGE"));
	  break;
  case SCTP_NOTIFICATIONS_STOPPED_EVENT:
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_NOTIFICATIONS_STOPPED_EVENT"));
	  break;
  case SCTP_SEND_FAILED_EVENT:
	  HandleSendFailedEvent(&(notif->sn_send_failed_event));
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_SEND_FAILED_EVENT"));
	  break;
  case SCTP_STREAM_RESET_EVENT:
	  //HandleStreamResetEvent(&(notif->sn_strreset_event));
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_STREAM_RESET_EVENT"));
	  break;
  case SCTP_ASSOC_RESET_EVENT:
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_ASSOC_RESET_EVENT"));
	  break;
  case SCTP_STREAM_CHANGE_EVENT:
	 // HandleStreamChangeEvent(&(notif->sn_strchange_event));
	  PJ_LOG(4, (THIS_FILE, "HandleNotification SCTP_STREAM_CHANGE_EVENT"));
	  break;
  default:
	  PJ_LOG(4, (THIS_FILE, "unknown SCTP event: %u", (uint32_t)notif->sn_header.sn_type));
	  break;
	}
}

#ifdef USE_OUTPUT_THREAD
static int sctp_output_thread(void *arg) {
	pj_thread_desc desc;
	pj_thread_t *thread = 0;
	transport_sctp *sctp = (transport_sctp *)arg;

	if (!pj_thread_is_registered(sctp->base.inst_id)) {
		pj_thread_register(sctp->base.inst_id, "sctp_output_thread", desc, &thread);
	}

	PJ_LOG(4, (THIS_FILE, "!!!SCTP OUTPUT THREAD CREATED!!! [tid=%d]", pj_gettid()));

	while (!sctp->thread_quit_flag) {
		sctp_data_buff *rb = NULL;
		pj_ssize_t sz = 0;
		char buff[3000] = {0};
		pj_bool_t is_tnl_data;

		//pj_mutex_lock(call->tnl_stream_lock3);

		pj_sem_wait(sctp->sbuff_sem);

		pj_mutex_lock(sctp->sbuff_mutex);

		if (!pj_list_empty(&sctp->sbuff)) {
			rb = sctp->sbuff.next;
			pj_list_erase(rb);

			pj_memset(buff, 0, sizeof(buff));
			pj_memcpy(buff, rb->buff, rb->len);
			sz = rb->len;
			is_tnl_data = pjmedia_natnl_sctp_packet_is_tnl_data(rb->buff, rb->len);
			if (is_tnl_data) {
				//pj_get_timestamp(&stream->last_data);  // TODO DEAN save current time 
				((pj_uint8_t*)buff)[sz] = 1;  // tunnel data flag on
			}
		}
		pj_mutex_unlock(sctp->sbuff_mutex);

		if (rb != NULL) {
			//move rb to gcbuff
			pj_mutex_lock(sctp->gcbuff_mutex);
			pj_list_push_back(&sctp->gcbuff, rb);
			pj_mutex_unlock(sctp->gcbuff_mutex);
		}
		//pj_mutex_unlock(call->tnl_stream_lock3);

		if (sctp->member_tp)
			pjmedia_transport_send_rtp(sctp->member_tp, buff, sz);
		/*if (status == PJ_SUCCESS)
			return 0;
		else {
			return -1;
		}*/
	}

	PJ_LOG(2, (THIS_FILE, "sctp_output_thread terminated. call_id=[%d]", sctp->base.call_id));
}
#endif

/************************************************************************/
/* SCTP data output callback                                            */
/************************************************************************/
static int sctp_data_output(void *addr, void *buffer, size_t length,
									  uint8_t tos, uint8_t set_df)
{
	transport_sctp *sctp = (transport_sctp* )addr;
	char *buf;
	pj_status_t status;
	pj_thread_desc desc;
	pj_thread_t *thread;
	sctp_data_buff *rb = NULL;
	pj_bool_t is_tnl_data;

	// Check if thread is registered for usrsctp timer thread.
	if (sctp && !pj_thread_is_registered(sctp->base.inst_id))
		pj_thread_register(sctp->base.inst_id, "sctp_output", desc, &thread);

#ifdef USE_OUTPUT_THREAD
	/* put the receive packet to received buffer */
	pj_mutex_lock(sctp->gcbuff_mutex);
	if (!pj_list_empty(&sctp->gcbuff)) {
		rb = sctp->gcbuff.next;
		pj_list_erase(rb);
	}
	pj_mutex_unlock(sctp->gcbuff_mutex);

	if (rb == NULL) {  // no more empty buffer
		return -1;
	}
	pj_memset(rb->buff, 0, sizeof(rb->buff));
	pj_memcpy(rb->buff, buffer, length);
	rb->len = length;

	is_tnl_data = pjmedia_natnl_sctp_packet_is_tnl_data(rb->buff, rb->len);
	if (is_tnl_data) {
		//pj_get_timestamp(&stream->last_data);  // TODO DEAN save current time 
		((pj_uint8_t*)rb->buff)[rb->len] = 1;  // tunnel data flag on
	}

	pj_mutex_lock(sctp->sbuff_mutex);
	pj_list_push_back(&sctp->sbuff, rb);

	PJ_LOG(5, (THIS_FILE, "sctp_data_output() gcbuff_size=%d", pj_list_size(&sctp->gcbuff)));
	pj_mutex_unlock(sctp->sbuff_mutex);

	pj_sem_post(sctp->sbuff_sem);

	return 0;
#else
	/*if ((buf = usrsctp_dumppacket(buffer, length, SCTP_DUMP_OUTBOUND)) != NULL) {
		PJ_LOG(6, (THIS_FILE, "%s", buf));
		usrsctp_freedumpbuffer(buf);
	}*/
	status = pjmedia_transport_send_rtp(sctp->member_tp, buffer, length);
	if (status == PJ_SUCCESS)
		return 0;
	else {
		return -1;
	}
#endif
}

/************************************************************************/
/* SCTP data input callback                                             */
/************************************************************************/
static int sctp_data_input(struct socket* sock, union sctp_sockstore addr,
							void *data, size_t datalen,
							struct sctp_rcvinfo rcv, int flags, void *ulp_info)
{
	transport_sctp *sctp = (transport_sctp *)ulp_info;

	if (!data) {
		usrsctp_close(sock); // SCTP has finished shutting down
	} else {
		//MutexAutoLock lock(mLock);
		if ((!sctp->base.remote_ua_is_sdk || sctp->offerer_side) && (flags & MSG_NOTIFICATION)) {
			HandleNotification(sctp, (const union sctp_notification *)data, datalen);
		} else {
			sctp->sctp_stream_id = rcv.rcv_sid;
			sctp->sctp_ppid = rcv.rcv_ppid;
			// Pass data to upper layer transport.
			if (sctp->rtp_cb) {
				(*sctp->rtp_cb)(sctp->user_data, data, datalen);
			}
		}
	}
	// sctp allocates 'data' with malloc(), and expects the receiver to free
	// it (presumably with free).
	// XXX future optimization: try to deliver messages without an internal
	// alloc/copy, and if so delay the free until later.
	if (datalen > 2000)
		free(data);
	// usrsctp defines the callback as returning an int, but doesn't use it
	return 1;
}

/* Initialize usrsctp library*/
static pj_status_t init_usrsctp(pj_pool_t *pool)
{
	pj_status_t status;

	PJ_UNUSED_ARG(pool);

	// Check if usrsctp is initialized.
	if (usrsctp_init_count)
		return PJ_SUCCESS;
#ifdef USE_GLOBAL_LOCK
	MUTEX_SETUP(global_mutex);
#endif

	usrsctp_init_count = 1;

	// Init usrsctp
	usrsctp_init(0, sctp_data_output, sctp_debug_printf);

	//usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);
	usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);

	usrsctp_sysctl_set_sctp_blackhole(2);
	usrsctp_sysctl_set_sctp_ecn_enable(0);

#if 0
	/* Register error subsystem */
	status = pj_register_strerror(PJ_SSL_ERRNO_START, 
		PJ_SSL_ERRNO_SPACE_SIZE, 
		&ssl_strerror);
	pj_assert(status == PJ_SUCCESS);
#endif

	return PJ_SUCCESS;
}

/* Shutdown OpenSSL */
static void shutdown_usrsctp(void)
{
	// DEAN don't shutdown openssl for multiple instances.
	return;

	if (!usrsctp_init_count) // Execute only one time.
		return;

	usrsctp_finish();

#ifdef USE_GLOBAL_LOCK
	MUTEX_CLEANUP(global_mutex);
#endif

	usrsctp_init_count = 0;
}

/* Create and initialize usrsctp and instance */
static pj_status_t create_sctp(transport_sctp *sctp)
{
    int mode, rc;
    pj_status_t status;
	char *openssl_ver;
	struct sctp_initmsg initmsg;
	struct linger l;
	struct sctp_assoc_value av;
	struct sctp_event event;
	uint32_t i;
	socklen_t len;

	uint16_t event_types[] = {SCTP_ASSOC_CHANGE,
		SCTP_PEER_ADDR_CHANGE,
		SCTP_REMOTE_ERROR,
		SCTP_SHUTDOWN_EVENT,
		SCTP_ADAPTATION_INDICATION,
		SCTP_SEND_FAILED_EVENT,
		SCTP_STREAM_RESET_EVENT,
		SCTP_STREAM_CHANGE_EVENT};
        
    pj_assert(sctp);

    /* Make sure OpenSSL library has been initialized */
	init_usrsctp(sctp->pool);

	usrsctp_register_address(sctp);
	PJ_LOG(4, (THIS_FILE, "create_sctp Registered %p within the SCTP stack.", sctp));

	if (!sctp->offerer_side && sctp->base.remote_ua_is_sdk) {
		sctp_server_create(sctp);
	} else {
		// Open sctp with a callback
		if ((sctp->sctp_sock = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, 
								sctp_data_input, NULL, 0, sctp)) == NULL) {
			return PJ_EUNKNOWN;
		}

		// Make non-blocking for bind/connect.  SCTP over UDP defaults to non-blocking
		// in associations for normal IO
		if (usrsctp_set_non_blocking(sctp->sctp_sock, 1) < 0) {
			PJ_LOG(1, (THIS_FILE, "create_sctp Couldn't set non_blocking on SCTP socket"));
			// We can't handle connect() safely if it will block, not that this will
			// even happen.
			goto error_cleanup;
		}

		// Make sure when we close the socket, make sure it doesn't call us back again!
		// This would cause it try to use an invalid DataChannelConnection pointer
		l.l_onoff = 1;
		l.l_linger = 0;
		if (usrsctp_setsockopt(sctp->sctp_sock, SOL_SOCKET, SO_LINGER,
			(const void *)&l, (socklen_t)sizeof(struct linger)) < 0) {
				PJ_LOG(1, (THIS_FILE, "create_sctp Couldn't set SO_LINGER on SCTP socket"));
				// unsafe to allow it to continue if this fails
				goto error_cleanup;
		}

		// XXX Consider disabling this when we add proper SDP negotiation.
		// We may want to leave enabled for supporting 'cloning' of SDP offers, which
		// implies re-use of the same pseudo-port number, or forcing a renegotiation.
		{
			uint32_t on = 1;
			if (usrsctp_setsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_REUSE_PORT,
				(const void *)&on, (socklen_t)sizeof(on)) < 0) {
					PJ_LOG(1, (THIS_FILE, "create_sctp Couldn't set SCTP_REUSE_PORT on SCTP socket"));
			}
			if (usrsctp_setsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_NODELAY,
				(const void *)&on, (socklen_t)sizeof(on)) < 0) {
					PJ_LOG(1, (THIS_FILE, "create_sctp Couldn't set SCTP_NODELAY on SCTP socket"));
			}
		}

		av.assoc_id = SCTP_ALL_ASSOC;
		av.assoc_value = SCTP_ENABLE_RESET_STREAM_REQ | SCTP_ENABLE_CHANGE_ASSOC_REQ;
		if (usrsctp_setsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &av,
			(socklen_t)sizeof(struct sctp_assoc_value)) < 0) {
				PJ_LOG(1, (THIS_FILE, "*** create_sctp failed enable stream reset errno %d", errno));
				goto error_cleanup;
		}

		/* Enable the events of interest. */
		memset(&event, 0, sizeof(event));
		event.se_assoc_id = SCTP_ALL_ASSOC;
		event.se_on = 1;
		for (i = 0; i < sizeof(event_types)/sizeof(event_types[0]); ++i) {
			event.se_type = event_types[i];
			if (usrsctp_setsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(event)) < 0) {
				PJ_LOG(1, (THIS_FILE, "*** create_sctp failed setsockopt SCTP_EVENT errno %d", errno));
				goto error_cleanup;
			}
		}
		
		/*// Update number of streams
		mStreams.AppendElements(aNumStreams);
		for (uint32_t i = 0; i < aNumStreams; ++i) {
			mStreams[i] = nullptr;
		}
		memset(&initmsg, 0, sizeof(initmsg));
		len = sizeof(initmsg);
		if (usrsctp_getsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, &len) < 0) {
			PJ_LOG(1, (THIS_FILE, "*** failed getsockopt SCTP_INITMSG"));
			goto error_cleanup;
		}
		PJ_LOG(1, (THIS_FILE, "Setting number of SCTP streams to %u, was %u/%u", aNumStreams,
			initmsg.sinit_num_ostreams, initmsg.sinit_max_instreams));
		initmsg.sinit_num_ostreams  = aNumStreams;
		initmsg.sinit_max_instreams = MAX_NUM_STREAMS;
		if (usrsctp_setsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
			(socklen_t)sizeof(initmsg)) < 0) {
				PJ_LOG(1, (THIS_FILE, "*** failed setsockopt SCTP_INITMSG, errno %d", errno));
				goto error_cleanup;
		}*/
	}

	sctp->state = SCTP_STATE_OPENED;

	return PJ_SUCCESS;

error_cleanup:
	usrsctp_close(sctp->sctp_sock);
	sctp->sctp_sock = NULL;
	return PJ_EUNKNOWN;
}


/* Destroy usrsctp */
static void destroy_sctp(transport_sctp *sctp)
{
	if (sctp && sctp->sctp_sock) {
		usrsctp_close(sctp->sctp_sock);
		sctp->sctp_sock = NULL;
	}

	if (sctp && sctp->sctp_server_sock) {
		usrsctp_close(sctp->sctp_server_sock);
		sctp->sctp_server_sock = NULL;
	}

#ifdef USE_OUTPUT_THREAD
	if (sctp) {
		// End output thread.
		sctp->thread_quit_flag = 1;
		if (sctp->sbuff_sem)
			pj_sem_post(sctp->sbuff_sem);

		if (sctp->output_thread) {
			pj_thread_join(sctp->output_thread);
			sctp->output_thread = NULL;
		}
	}
#endif

	/* Destroy mutex to protect jitter buffer: */
	if (sctp && sctp->sbuff_mutex) {
		pj_mutex_destroy(sctp->sbuff_mutex);
		sctp->sbuff_mutex = NULL;
	}

	if (sctp && sctp->gcbuff_mutex) {
		pj_mutex_destroy(sctp->gcbuff_mutex);
		sctp->gcbuff_mutex = NULL;
	}

	if (sctp && sctp->sbuff_sem) {
		pj_sem_destroy(sctp->sbuff_sem);
		sctp->sbuff_sem = NULL;
	}

	if (sctp)
		usrsctp_deregister_address(sctp);
}
/*
 *******************************************************************
 * API
 *******************************************************************
 */

static void dump_bin(const char *buf, unsigned len, pj_bool_t send)
{
	unsigned i;
#ifndef DUMP_PACKET
	return;
#endif
	//if (len < 1400)
	//	return;

	if (send)
		PJ_LOG(3,(THIS_FILE, "send packet!!!"));
	else
		PJ_LOG(3,(THIS_FILE, "recv packet!!!"));

	PJ_LOG(3,(THIS_FILE, "begin dump"));
	for (i=0; i<len; ++i) {
		int j;
		char bits[9];
		unsigned val = buf[i] & 0xFF;

		bits[8] = '\0';
		for (j=0; j<8; ++j) {
			if (val & (1 << (7-j)))
				bits[j] = '1';
			else
				bits[j] = '0';
		}

		PJ_LOG(3,(THIS_FILE, "%2d %s [%d]", i, bits, val));
	}
	PJ_LOG(3,(THIS_FILE, "end dump"));
}

static pj_bool_t libsctp_initialized;
static void pjmedia_sctp_deinit_lib(pjmedia_endpt *endpt);

static int sctp_accept_thread(void *arg)
{
	transport_sctp *sctp = (transport_sctp *)arg;
	struct sockaddr_conn addr;
	int r = 0;

	pj_thread_desc desc;
	pj_thread_t *thread = 0;
	if (!pj_thread_is_registered(sctp->base.inst_id)) {
		pj_thread_register(sctp->base.inst_id, "sctp_accept_thread", desc, &thread);
	}

	if (sctp->state != SCTP_STATE_OPENED /*|| !sctp->sctp_sock*/)
		return PJ_EUNKNOWN;

	sctp->sctp_sock = usrsctp_accept(sctp->sctp_server_sock, NULL, NULL);

#if 0
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
#if defined(__Userspace_os_Darwin)
	addr.sconn_len = sizeof(addr);
#endif
	addr.sin_port = htons(DEFAULT_SCTP_LOCAL_PORT);
	addr.sin_addr.s_addr = htonl("127.0.0.1");
#else
	memset(&addr, 0, sizeof(addr));
	addr.sconn_family = AF_CONN;
#if defined(__Userspace_os_Darwin)
	addr.sconn_len = sizeof(addr);
#endif
	addr.sconn_port = htons(DEFAULT_SCTP_LOCAL_PORT);
	addr.sconn_addr = sctp;
#endif

	if (sctp->sctp_sock) {
		struct sctp_paddrparams paddrparams;
		socklen_t opt_len;
		uint32_t on = 1;
		struct sctp_assoc_value av;

		/*if (usrsctp_setsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_DISABLE_FRAGMENTS,
			(const void *)&on, (socklen_t)sizeof(on)) < 0) {
				PJ_LOG(1, (THIS_FILE, "sctp_accept_thread() Couldn't set SCTP_DISABLE_FRAGMENTS on SCTP socket"));
		}

		av.assoc_id = 0;
		av.assoc_value = 1320;
		if (usrsctp_setsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_MAXSEG, &av, sizeof(struct sctp_assoc_value)) < 0)
			PJ_LOG(1, (THIS_FILE, "sctp_accept_thread() Couldn't set SCTP_MAXSEG on SCTP socket. %d", errno));*/

		PJ_LOG(4, (THIS_FILE, "HandleAssocChangeEvent SCTP connected."));
		sctp->state = SCTP_STATE_CONNECTED;
		sctp->session_inited = PJ_TRUE;
		usrsctp_register_address(sctp);
		usrsctp_set_ulpinfo(sctp->sctp_sock, sctp);

		memset(&paddrparams, 0, sizeof(struct sctp_paddrparams));
		//memcpy(&paddrparams.spp_address, &addr, sizeof(struct sockaddr_conn));
		memcpy(&paddrparams.spp_address, &addr, sizeof(addr));
		opt_len = (socklen_t)sizeof(struct sctp_paddrparams);
		r = usrsctp_getsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
			&paddrparams, &opt_len);
		if (r < 0) {			
			PJ_LOG(1, (THIS_FILE, "sctp_accept_thread() usrsctp_getsockopt failed: %d", r));
		} else {
			// draft-ietf-rtcweb-data-channel-13 section 5: max initial MTU IPV4 1200, IPV6 1280
			paddrparams.spp_pathmtu = 1400; // safe for either
			paddrparams.spp_flags &= ~SPP_PMTUD_ENABLE;
			paddrparams.spp_flags |= SPP_PMTUD_DISABLE;
			opt_len = (socklen_t)sizeof(struct sctp_paddrparams);
			r = usrsctp_setsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
				&paddrparams, opt_len);
			if (r < 0) {
				PJ_LOG(1, (THIS_FILE, "sctp_accept_thread() usrsctp_getsockopt failed: %d", r));
			} else {
				PJ_LOG(4, (THIS_FILE, "sctp_accept_thread() usrsctp: PMTUD disabled, MTU set to %u", paddrparams.spp_pathmtu));
				if (sctp->setting.cb.on_sctp_connection_complete) {
					sctp->setting.cb.on_sctp_connection_complete(sctp, PJ_SUCCESS, sctp->turn_mapped_addr);
					PJ_LOG(4, (THIS_FILE, "sctp_accept_thread() Call on_sctp_connection_complete()", paddrparams.spp_pathmtu));
				}
			}
		}
	}
	if (r < 0) {
		if (errno == EINPROGRESS) {
			// non-blocking
			return 0;
		} else {
			PJ_LOG(1, (THIS_FILE, "usrsctp_connect failed: %d", errno));
			sctp->state = SCTP_STATE_CLOSED;
		}
	} else {
		// We set Even/Odd and fire ON_CONNECTION via SCTP_COMM_UP when we get that
		// This also avoids issues with calling TransportFlow stuff on Mainthread
		return 0;
	}
}

PJ_DEF(void) pjmedia_sctp_session_create(pjmedia_transport *tp,
										 pj_sockaddr *turn_mapped_addr)
{
	transport_sctp *sctp = (transport_sctp *)tp;
	struct sockaddr_conn addr;
	int r;
	pj_thread_t *thread = 0;

	if (sctp->state != SCTP_STATE_OPENED /*|| !sctp->sctp_sock*/)
		return;

	sctp->turn_mapped_addr = turn_mapped_addr;

	memset(&addr, 0, sizeof(addr));
	addr.sconn_family = AF_CONN;
#if defined(__Userspace_os_Darwin)
	addr.sconn_len = sizeof(addr);
#endif
	addr.sconn_port = htons(DEFAULT_SCTP_LOCAL_PORT);
	addr.sconn_addr = sctp;

	if (!sctp->offerer_side && sctp->base.remote_ua_is_sdk) {
		pj_thread_create(sctp->pool, "sctp_accept_thread", &sctp_accept_thread,  
			(void *)sctp, 0, 0,
			&thread);
	} else {
		PJ_LOG(4, (THIS_FILE, "Calling usrsctp_bind"));
		r = usrsctp_bind(sctp->sctp_sock, (struct sockaddr *)(&addr), sizeof(addr));
		if (r < 0) {
			PJ_LOG(1, (THIS_FILE, "usrsctp_bind failed: %d", errno));
		} else {
			// This is the remote addr
			addr.sconn_port = htons(DEFAULT_SCTP_LOCAL_PORT);
			PJ_LOG(4, (THIS_FILE, "Calling usrsctp_connect"));
			r = usrsctp_connect(sctp->sctp_sock, (struct sockaddr *)(&addr), sizeof(addr));
			if (r >= 0 || errno == EINPROGRESS) {
				struct sctp_paddrparams paddrparams;
				socklen_t opt_len;

				sctp->state = SCTP_STATE_CONNECTING;

				memset(&paddrparams, 0, sizeof(struct sctp_paddrparams));
				memcpy(&paddrparams.spp_address, &addr, sizeof(struct sockaddr_conn));
				opt_len = (socklen_t)sizeof(struct sctp_paddrparams);
				r = usrsctp_getsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
					&paddrparams, &opt_len);
				if (r < 0) {
					PJ_LOG(1, (THIS_FILE, "usrsctp_getsockopt failed: %d", r));
				} else {
					// draft-ietf-rtcweb-data-channel-13 section 5: max initial MTU IPV4 1200, IPV6 1280
					paddrparams.spp_pathmtu = 1200; // safe for either
					paddrparams.spp_flags &= ~SPP_PMTUD_ENABLE;
					paddrparams.spp_flags |= SPP_PMTUD_DISABLE;
					opt_len = (socklen_t)sizeof(struct sctp_paddrparams);
					r = usrsctp_setsockopt(sctp->sctp_sock, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
						&paddrparams, opt_len);
					if (r < 0) {
						PJ_LOG(1, (THIS_FILE, "usrsctp_getsockopt failed: %d", r));
					} else {
						PJ_LOG(4, (THIS_FILE, "usrsctp: PMTUD disabled, MTU set to %u", paddrparams.spp_pathmtu));
					}
				}
			}
			if (r < 0) {
				if (errno == EINPROGRESS) {
					// non-blocking
					return;
				} else {
					PJ_LOG(1, (THIS_FILE, "usrsctp_connect failed: %d", errno));
					sctp->state = SCTP_STATE_CLOSED;
				}
			} else {
				// We set Even/Odd and fire ON_CONNECTION via SCTP_COMM_UP when we get that
				// This also avoids issues with calling TransportFlow stuff on Mainthread
				return;
			}
		}
	}
}

PJ_DEF(pj_status_t) pjmedia_sctp_init_lib(pjmedia_endpt *endpt)
{
    if (libsctp_initialized == PJ_FALSE) {
	pj_status_t status;

	status = init_usrsctp(pjmedia_get_pool(endpt));
	if (status != PJ_SUCCESS) { 
	    PJ_LOG(4, (THIS_FILE, "Failed to initialize libsctp: %d", 
		       status));
	    return status;
	}

	if (pjmedia_endpt_atexit(endpt, pjmedia_sctp_deinit_lib) != PJ_SUCCESS)
	{
	    /* There will be memory leak when it fails to schedule libsctp 
	     * deinitialization, however the memory leak could be harmless,
	     * since in modern OS's memory used by an application is released 
	     * when the application terminates.
	     */
	    PJ_LOG(4, (THIS_FILE, "Failed to register libsctp deinit."));
	}

	libsctp_initialized = PJ_TRUE;
    }
    
    return PJ_SUCCESS;
}

static void pjmedia_sctp_deinit_lib(pjmedia_endpt *endpt)
{
    /* Note that currently this SRTP init/deinit is not equipped with
     * reference counter, it should be safe as normally there is only
     * one single instance of media endpoint and even if it isn't, the
     * pjmedia_transport_sctp_create() will invoke SCTP init (the only
     * drawback should be the delay described by #788).
     */

    PJ_UNUSED_ARG(endpt);

    shutdown_usrsctp();

    libsctp_initialized = PJ_FALSE;
}


PJ_DEF(void) pjmedia_sctp_setting_default(pjmedia_sctp_setting *opt)
{
	pj_assert(opt);

	pj_bzero(opt, sizeof(pjmedia_sctp_setting));
	opt->close_member_tp = PJ_TRUE;
	opt->use = PJMEDIA_SCTP_DISABLED;
	opt->timeout.sec = 3;
}


/*
 * Create an SRTP media transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_sctp_create(
				       pjmedia_endpt *endpt,
				       pjmedia_transport *tp,
				       const pjmedia_sctp_setting *opt,
				       pjmedia_transport **p_tp)
{
    pj_pool_t *pool;
	transport_sctp *sctp;
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt && tp && p_tp, PJ_EINVAL);

    /* Init libusrsctp. */
    status = pjmedia_sctp_init_lib(endpt);
    if (status != PJ_SUCCESS)
	return status;

    pool = pjmedia_endpt_create_pool(endpt, "sctp%p", 30720, 2048);
    sctp = PJ_POOL_ZALLOC_T(pool, transport_sctp);

    sctp->pool = pool;
	sctp->state = SCTP_STATE_CLOSED;
	sctp->sctp_sock = NULL;
	sctp->session_inited = PJ_FALSE;
	sctp->sctp_inited = PJ_FALSE;
	sctp->media_type_app = PJ_FALSE;

	if (opt) {
		sctp->setting = *opt;
	} else {
		pjmedia_sctp_setting_default(&sctp->setting);
	}
#ifdef BYPASS
	sctp->bypass_sctp = PJ_TRUE;
#endif
#if 0
	status = pj_lock_create_simple_mutex(pool, pool->obj_name, &sctp->mutex);
	if (status != PJ_SUCCESS) {
		pj_pool_release(pool);
		return status;
	}
#else
	MUTEX_SETUP(sctp->mutex);
#endif

    /* Initialize base pjmedia_transport */
    pj_memcpy(sctp->base.name, pool->obj_name, PJ_MAX_OBJ_NAME);
	if (tp) {
		sctp->base.type = tp->type;
		sctp->base.remote_ua_is_sdk = tp->remote_ua_is_sdk;
	}
    else
		sctp->base.type = PJMEDIA_TRANSPORT_TYPE_UDP;
	sctp->base.op = &transport_sctp_op;
	//pj_strdup2(, sctp->cert_fingerprint, "");

	pj_list_init(&sctp->write_pending);
	pj_list_init(&sctp->write_pending_empty);
	pj_list_init(&sctp->send_pending);
	//pj_timer_entry_init(&sctp->timer, 0, sctp, &on_timer);

	/* Prepare write/send state */
	pj_assert(sctp->send_buf.max_len == 0);
	sctp->send_buf.buf = (char*)
		pj_pool_alloc(sctp->pool, 
		sctp->setting.send_buffer_size);
	sctp->send_buf.max_len = sctp->setting.send_buffer_size;
	sctp->send_buf.start = sctp->send_buf.buf;
	sctp->send_buf.len = 0;
	sctp->read_size = sctp->setting.send_buffer_size;

#ifdef USE_OUTPUT_THREAD
	// Init buffer list
	pj_list_init(&sctp->sbuff);
	pj_list_init(&sctp->gcbuff);
	sctp->sbuff_cnt = 0;
	sctp->thread_quit_flag = 0;
	{
		int i;
		sctp_data_buff *rb = NULL;

		// Prepare 30 buffers.
		sctp->sbuff_cnt = 30;
		for (i=0; i < sctp->sbuff_cnt; i++) {
			rb = PJ_POOL_ZALLOC_T(sctp->pool, sctp_data_buff);
			pj_list_push_back(&sctp->gcbuff, rb);
		}
	}
#endif

	/* Create mutex to protect buffer: */
	status = pj_mutex_create_simple(pool, NULL, &sctp->sbuff_mutex);
	if (status != PJ_SUCCESS) {
		return status;
	}

	status = pj_mutex_create_simple(pool, NULL, &sctp->gcbuff_mutex);
	if (status != PJ_SUCCESS) {
		return status;
	}

	/* Create semaphore */
	status = pj_sem_create(pool, "sctp_sbuff_sem", 0, 65535, &sctp->sbuff_sem);
	if (status != PJ_SUCCESS) {
		return status;
	}

#ifdef USE_OUTPUT_THREAD
	status = pj_thread_create(pool, "sctp_output_thread", &sctp_output_thread,  
		(void *)sctp, 0, 0,
		&sctp->output_thread);
	if (status != PJ_SUCCESS) {
		return status;
	}
#endif

	/* Create SSL context */
	status = create_sctp(sctp);
	if (status != PJ_SUCCESS)
		return status;

    /* Set underlying transport */
    sctp->member_tp = tp;

    /* Initialize peer's SRTP usage mode. */
    sctp->peer_use = sctp->setting.use;

    /* Done */
    *p_tp = &sctp->base;

    return PJ_SUCCESS;
}


/*
 * Initialize and start SCTP session with the given parameters.
 */
PJ_DEF(pj_status_t) pjmedia_transport_sctp_start(
			   pjmedia_transport *tp)
{
    transport_sctp  *sctp = (transport_sctp*) tp;
	pj_status_t	     status = PJ_SUCCESS;

	PJ_ASSERT_RETURN(tp, PJ_EINVAL);

#ifdef USE_GLOBAL_LOCK
	MUTEX_LOCK(global_mutex);
#endif

    MUTEX_LOCK(sctp->mutex);

    if (sctp->session_inited) {
		pjmedia_transport_sctp_stop(tp);
	}

	/* Declare SCTP session initialized */
	//sctp->session_inited = PJ_TRUE;

	MUTEX_UNLOCK(sctp->mutex);

#ifdef USE_GLOBAL_LOCK
	MUTEX_UNLOCK(global_mutex);
#endif
    return status;
}

/*
 * Stop SRTP session.
 */
PJ_DEF(pj_status_t) pjmedia_transport_sctp_stop(pjmedia_transport *sctp)
{
    transport_sctp *p_sctp = (transport_sctp*) sctp;
    //pj_status_t status;

	PJ_ASSERT_RETURN(sctp, PJ_EINVAL);

#ifdef USE_GLOBAL_LOCK
	MUTEX_LOCK(global_mutex);
#endif

	MUTEX_LOCK(p_sctp->mutex);

	destroy_sctp(p_sctp);

    p_sctp->session_inited = PJ_FALSE;

	MUTEX_UNLOCK(p_sctp->mutex);

#ifdef USE_GLOBAL_LOCK
	MUTEX_UNLOCK(global_mutex);
#endif

    return PJ_SUCCESS;
}

PJ_DEF(pjmedia_transport *) pjmedia_transport_sctp_get_member(
						pjmedia_transport *tp)
{
    transport_sctp *sctp = (transport_sctp*) tp;

    if(!tp)
		return NULL;

    return sctp->member_tp;
}


static pj_status_t transport_get_info(pjmedia_transport *tp,
				      pjmedia_transport_info *info)
{
    transport_sctp *sctp = (transport_sctp*) tp;
    pjmedia_sctp_info sctp_info;
    int spc_info_idx;

    PJ_ASSERT_RETURN(tp && info, PJ_EINVAL);
    PJ_ASSERT_RETURN(info->specific_info_cnt <
		     PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXCNT, PJ_ETOOMANY);
    PJ_ASSERT_RETURN(sizeof(pjmedia_sctp_info) <=
		     PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXSIZE, PJ_ENOMEM);

    sctp_info.active = sctp->session_inited;
    sctp_info.use = sctp->setting.use;
    sctp_info.peer_use = sctp->peer_use;

    spc_info_idx = info->specific_info_cnt++;
	if (sctp->media_type_app)
		info->spc_info[spc_info_idx].type = PJMEDIA_TRANSPORT_TYPE_DTLS_SCTP;
	else
		info->spc_info[spc_info_idx].type = PJMEDIA_TRANSPORT_TYPE_DTLS;
    info->spc_info[spc_info_idx].cbsize = sizeof(sctp_info);
    pj_memcpy(&info->spc_info[spc_info_idx].buffer, &sctp_info, 
	      sizeof(sctp_info));

    return pjmedia_transport_get_info(sctp->member_tp, info);
}

static pj_status_t transport_attach(pjmedia_transport *tp,
				    void *user_data,
				    const pj_sockaddr_t *rem_addr,
				    const pj_sockaddr_t *rem_rtcp,
				    unsigned addr_len,
				    void (*rtp_cb) (void*, void*,
						    pj_ssize_t),
				    void (*rtcp_cb)(void*, void*,
						    pj_ssize_t))
{
	transport_sctp *sctp = (transport_sctp*) tp;
	char buf[PJ_INET6_ADDRSTRLEN+10];
    pj_status_t status;

    PJ_ASSERT_RETURN(tp && rem_addr && addr_len, PJ_EINVAL);

#ifdef USE_GLOBAL_LOCK
	MUTEX_LOCK(global_mutex);
#endif
    /* Save the callbacks */
    MUTEX_LOCK(sctp->mutex);
    sctp->rtp_cb = rtp_cb;
    sctp->rtcp_cb = rtcp_cb;
	sctp->user_data = user_data;
	
	// Save remote address.
	pj_sockaddr_cp(&sctp->rem_addr, rem_addr);
	PJ_LOG(3, (THIS_FILE, "transport_attach rem_addr=[%s]", 
		pj_sockaddr_print((pj_sockaddr_t *)&sctp->rem_addr, buf, sizeof(buf), 3)));

	MUTEX_UNLOCK(sctp->mutex);

    /* Attach itself to transport */
    status = pjmedia_transport_attach(sctp->member_tp, sctp, rem_addr, 
				      rem_rtcp, addr_len, &sctp_rtp_cb,
				      &sctp_rtcp_cb);
    if (status != PJ_SUCCESS) {
	MUTEX_LOCK(sctp->mutex);
	sctp->rtp_cb = NULL;
	sctp->rtcp_cb = NULL;
	sctp->user_data = NULL;
	MUTEX_UNLOCK(sctp->mutex);
#ifdef USE_GLOBAL_LOCK
	MUTEX_UNLOCK(global_mutex);
#endif
	return status;
	}
#ifdef USE_GLOBAL_LOCK
	MUTEX_UNLOCK(global_mutex);
#endif

    return PJ_SUCCESS;
}

static void transport_detach(pjmedia_transport *tp, void *strm)
{
    transport_sctp *sctp = (transport_sctp*) tp;

    PJ_UNUSED_ARG(strm);
	PJ_ASSERT_ON_FAIL(tp, return);

#ifdef USE_GLOBAL_LOCK
	MUTEX_LOCK(global_mutex);
#endif

    if (sctp->member_tp) {
	pjmedia_transport_detach(sctp->member_tp, sctp);
    }

    /* Clear up application infos from transport */
    MUTEX_LOCK(sctp->mutex);
    sctp->rtp_cb = NULL;
    sctp->rtcp_cb = NULL;
    sctp->user_data = NULL;
	MUTEX_UNLOCK(sctp->mutex);

#ifdef USE_GLOBAL_LOCK
	MUTEX_UNLOCK(global_mutex);
#endif
}

static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    pj_status_t status = PJ_SUCCESS;
    transport_sctp *sctp = (transport_sctp*) tp;
    char *data = (char *)pkt;
	struct sctp_sendv_spa spa;
	pj_bool_t disable_flow_ctl = PJ_FALSE;

	// check if there is no flow control flag.
	disable_flow_ctl = pjmedia_natnl_disabled_flow_control(pkt, size);
	if (sctp->bypass_sctp || disable_flow_ctl) {
		dump_bin(pkt, size, 1);
		return pjmedia_transport_send_rtp(sctp->member_tp, pkt, size);
	}

	if (sctp->state == SCTP_STATE_CONNECTED)
	{
		int32_t result;
		memset(&spa, 0, sizeof(struct sctp_sendv_spa));
		spa.sendv_sndinfo.snd_ppid = htonl(SCTP_PPID_WEBRTC_BINARY);
		spa.sendv_sndinfo.snd_sid = sctp->sctp_stream_id;//channel->mStream; // DEAN SCTP INT confirmed the stream id.
		//spa.sendv_sndinfo.snd_flags = flags;
		spa.sendv_sndinfo.snd_context = 0;
		spa.sendv_sndinfo.snd_assoc_id = 0;
		spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;

		/*if (channel->mPrPolicy != SCTP_PR_SCTP_NONE) {
			spa.sendv_prinfo.pr_policy = channel->mPrPolicy;
			spa.sendv_prinfo.pr_value = channel->mPrValue;
			spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		}*/

		// Note: Main-thread IO, but doesn't block!
		// XXX FIX!  to deal with heavy overruns of JS trying to pass data in
		// (more than the buffersize) queue data onto another thread to do the
		// actual sends.  See netwerk/protocol/websocket/WebSocketChannel.cpp

		// SCTP will return EMSGSIZE if the message is bigger than the buffer
		// size (or EAGAIN if there isn't space)
		if (data && size) {
			result = usrsctp_sendv(sctp->sctp_sock, data, size,
				NULL, 0,
				(void *)&spa, (socklen_t)sizeof(struct sctp_sendv_spa),
				SCTP_SENDV_SPA, 0);
			PJ_LOG(5, (THIS_FILE, "Sent buffer (len=%u), result=%d", size, result));
		} else {
			// Fake EAGAIN if we're already buffering data
			result = -1;
			errno = EAGAIN;
		}
		if (result < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return -1;
			}
			PJ_LOG(5, (THIS_FILE, "transport_send_rtp error %d sending data", errno));
			pj_thread_sleep(1);
		}
		dump_bin(data, size, 1);
	} else {
		PJ_LOG(5, (THIS_FILE, "transport_send_rtp() SSL not ready sctp->ssl_state=[%d].", sctp->state));

		status = -1;
		//dump_bin(data, size, 1);
	}
on_return:
    return status;
}

static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    return transport_send_rtcp2(tp, NULL, 0, pkt, size);
}

static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
				        const pj_sockaddr_t *addr,
				        unsigned addr_len,
				        const void *pkt,
				        pj_size_t size)
{
    pj_status_t status = PJ_SUCCESS;
    transport_sctp *sctp = (transport_sctp*) tp;
    //int len = size;
    int err = PJ_SUCCESS;

    if (sctp->bypass_sctp) {
	return pjmedia_transport_send_rtcp2(sctp->member_tp, addr, addr_len, 
	                                    pkt, size);
    }

    //if (size > sizeof(sctp->sctp_tx_buffer))
	//return PJ_ETOOBIG;

    //pj_memcpy(sctp->sctp_tx_buffer, pkt, size);

    MUTEX_LOCK(sctp->mutex);
    if (!sctp->session_inited) {
	MUTEX_UNLOCK(sctp->mutex);
	return PJ_EINVALIDOP;
    }
    //err = srtp_protect_rtcp(sctp->srtp_tx_ctx, sctp->rtcp_tx_buffer, &len);
    MUTEX_UNLOCK(sctp->mutex);

    if (err == PJ_SUCCESS) {
	//status = pjmedia_transport_send_rtcp2(sctp->member_tp, addr, addr_len,
	//				      sctp->sctp_tx_buffer, len);
    } else {
	//status = PJMEDIA_ERRNO_FROM_LIBSRTP(err);
    }

    return status;
}


static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
					   pjmedia_dir dir,
					   unsigned pct_lost)
{
    transport_sctp *sctp = (transport_sctp *) tp;
    
    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    return pjmedia_transport_simulate_lost(sctp->member_tp, dir, pct_lost);
}

static pj_status_t transport_destroy  (pjmedia_transport *tp)
{
    transport_sctp *sctp = (transport_sctp *) tp;
    pj_status_t status;

	PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    if (sctp->setting.close_member_tp && sctp->member_tp) {
	pjmedia_transport_close(sctp->member_tp);
    }

    status = pjmedia_transport_sctp_stop(tp);

	/* In case mutex is being acquired by other thread */

	MUTEX_LOCK(sctp->mutex);
	MUTEX_UNLOCK(sctp->mutex);

	MUTEX_CLEANUP(sctp->mutex);
    pj_pool_release(sctp->pool);

    return status;
}

/*
 * This callback is called by transport when incoming rtp is received
 */
static void sctp_rtp_cb( void *user_data, void *pkt, pj_ssize_t size)
{
    transport_sctp *sctp = (transport_sctp *) user_data;
    //int len = size;
    //pj_status_t err = PJ_SUCCESS;
    void (*cb)(void*, void*, pj_ssize_t) = NULL;
	void *cb_data = NULL;
	pj_size_t nwritten;
	pj_status_t status;
	void *data = pkt;
	pj_bool_t disable_flow_ctl = PJ_FALSE;

	if (!pkt || size <= 0) {
		return;
	}

	// check if there is no flow control flag.
	disable_flow_ctl = pjmedia_natnl_disabled_flow_control(pkt, size);
	if (sctp->bypass_sctp || disable_flow_ctl) {
		dump_bin(pkt, size, 0);
		sctp->rtp_cb(sctp->user_data, pkt, size);
		return;
	}
#ifdef USE_GLOBAL_LOCK
	MUTEX_LOCK(global_mutex);
#endif

	//MUTEX_LOCK(sctp->mutex);

	PJ_LOG(6, (THIS_FILE, "sctp_rtp_cb() size=[%d], sctp=[%p]", size, sctp));

	/* Socket error or closed */
	if (data && size > 0) {
		// Pass the data to usrsctp
		usrsctp_conninput(sctp, data, size, 0);
	}

	//MUTEX_UNLOCK(sctp->mutex);

#ifdef USE_GLOBAL_LOCK
	MUTEX_UNLOCK(global_mutex);
#endif

	return;

on_error:
	if (sctp->state == SCTP_STATE_CONNECTING)
	{
		return;
	}
	sctp->state = SCTP_STATE_CLOSED;

	MUTEX_UNLOCK(sctp->mutex);

#ifdef USE_GLOBAL_LOCK
	MUTEX_UNLOCK(global_mutex);
#endif

	return;
}

/*
 * This callback is called by transport when incoming rtcp is received
 */
static void sctp_rtcp_cb( void *user_data, void *pkt, pj_ssize_t size)
{
    transport_sctp *sctp = (transport_sctp *) user_data;
    int len = size;
    pj_status_t err = PJ_SUCCESS;
    void (*cb)(void*, void*, pj_ssize_t) = NULL;
    void *cb_data = NULL;

    if (sctp->bypass_sctp) {
	sctp->rtcp_cb(sctp->user_data, pkt, size);
	return;
    }

    if (size < 0) {
	return;
    }

    MUTEX_LOCK(sctp->mutex);

    if (!sctp->session_inited) {
	MUTEX_UNLOCK(sctp->mutex);
	return;
    }
	/*// TODO SCTP decrypt data
    //err = srtp_unprotect_rtcp(sctp->srtp_rx_ctx, (pj_uint8_t*)pkt, &len);
    if (err != PJ_SUCCESS) {
	PJ_LOG(5,(sctp->pool->obj_name, 
		  "Failed to decrypt SCTP, pkt size=%d, err=%d",
		  size, GET_SSL_STATUS(sctp)));
    } else {
	cb = sctp->rtcp_cb;
	cb_data = sctp->user_data;
    }*/

    MUTEX_UNLOCK(sctp->mutex);

    if (cb) {
	(*cb)(cb_data, pkt, len);
    }
}

static pj_status_t transport_media_create(pjmedia_transport *tp,
				          pj_pool_t *sdp_pool,
					  unsigned options,
				          const pjmedia_sdp_session *sdp_remote,
					  unsigned media_index)
{
    struct transport_sctp *sctp = (struct transport_sctp*) tp;
    unsigned member_tp_option;

    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    sctp->media_option = options;
    member_tp_option = options | PJMEDIA_TPMED_NO_TRANSPORT_CHECKING;

    sctp->offerer_side = sdp_remote == NULL;

    /* Validations */
    if (sctp->offerer_side) {
	
	if (sctp->setting.use == PJMEDIA_SCTP_DISABLED)
	    goto BYPASS_SCTP;

    } else {

	pjmedia_sdp_media *m_rem;

	m_rem = sdp_remote->media[media_index];
	if (pjmedia_get_meida_type(sdp_remote, media_index) == PJMEDIA_TYPE_APPLICATION) { // dean : for WebRTC data channel.
		tp->use_sctp = 1;
	} else {

		/* Nothing to do on inactive media stream */
		//if (pjmedia_sdp_media_find_attr(m_rem, &ID_INACTIVE, NULL))
		//    goto BYPASS_SCTP;

		/* Validate remote media transport based on SRTP usage option.
		 */
		switch (sctp->setting.use) {
			case PJMEDIA_SCTP_DISABLED:
				goto BYPASS_SCTP;
			case PJMEDIA_SCTP_MANDATORY:
				tp->use_sctp = 1;
			break;
		}

	}

    }
    goto PROPAGATE_MEDIA_CREATE;

BYPASS_SCTP:
    sctp->bypass_sctp = PJ_TRUE;
    //member_tp_option &= ~PJMEDIA_TPMED_NO_TRANSPORT_CHECKING;

PROPAGATE_MEDIA_CREATE:
	// DEAN assign app's lock object.
	sctp->member_tp->app_lock = sctp->base.app_lock;
	sctp->member_tp->call_id = sctp->base.call_id;
    return pjmedia_transport_media_create(sctp->member_tp, sdp_pool, 
					  member_tp_option, sdp_remote,
					  media_index);
}

static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
					pj_pool_t *sdp_pool,
					pjmedia_sdp_session *sdp_local,
					const pjmedia_sdp_session *sdp_remote,
					unsigned media_index)
{
    struct transport_sctp *sctp = (struct transport_sctp*) tp;
    pjmedia_sdp_media *m_rem, *m_loc;
    enum { MAXLEN = 512 };
    char buffer[MAXLEN];
    int buffer_len;
    pj_status_t status;
    pjmedia_sdp_attr *attr;
    pj_str_t attr_value;
    unsigned i, j;
	pjmedia_sdp_attr *finprt_attr, *sctpmap_attr;

    PJ_ASSERT_RETURN(tp && sdp_pool && sdp_local, PJ_EINVAL);
    
    sctp->offerer_side = (sdp_remote == NULL);

    m_rem = sdp_remote ? sdp_remote->media[media_index] : NULL;
    m_loc = sdp_local->media[media_index];
    goto PROPAGATE_MEDIA_CREATE;

BYPASS_SCTP:
    sctp->bypass_sctp = PJ_TRUE;

PROPAGATE_MEDIA_CREATE:
    return pjmedia_transport_encode_sdp(sctp->member_tp, sdp_pool, 
					sdp_local, sdp_remote, media_index);
}



static pj_status_t transport_media_start(pjmedia_transport *tp,
				         pj_pool_t *pool,
				         const pjmedia_sdp_session *sdp_local,
				         const pjmedia_sdp_session *sdp_remote,
				         unsigned media_index)
{
    struct transport_sctp *sctp = (struct transport_sctp*) tp;
    pjmedia_sdp_media *m_rem, *m_loc;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp && pool && sdp_local && sdp_remote, PJ_EINVAL);

    m_rem = sdp_remote->media[media_index];
    m_loc = sdp_local->media[media_index];

	if (tp->use_sctp)
		sctp->bypass_sctp = PJ_FALSE;
	else
		sctp->bypass_sctp = PJ_TRUE;

	status = pjmedia_transport_sctp_start(tp);
	if (status != PJ_SUCCESS)
		return status;

    return pjmedia_transport_media_start(sctp->member_tp, pool, 
					 sdp_local, sdp_remote,
				         media_index);
}

static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    struct transport_sctp *sctp = (struct transport_sctp*) tp;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    status = pjmedia_transport_media_stop(sctp->member_tp);
    if (status != PJ_SUCCESS)
	PJ_LOG(4, (sctp->pool->obj_name, 
		   "SCTP failed stop underlying media transport."));

    return pjmedia_transport_sctp_stop(tp);
}


