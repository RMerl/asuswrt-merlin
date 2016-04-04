/* $Id: jbsim.c 3816 2011-10-14 04:15:15Z bennylp $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

/* jbsim:

    This program emulates various system and network impairment
    conditions as well as application parameters and apply it to
    an input WAV file. The output is another WAV file as well as
    a detailed log file (in CSV format) for troubleshooting.
 */


/* Include PJMEDIA and PJLIB */
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjlib.h>
#include <pjlib-util.h>

#define THIS_FILE   "jbsim.c"

/* Timer resolution in ms (must be NONZERO!) */
#define WALL_CLOCK_TICK	    1

/* Defaults settings */
#define CODEC		"PCMU"
#define LOG_FILE	"jbsim.csv"
#define WAV_REF		"../../tests/pjsua/wavs/input.8.wav"
#define WAV_OUT		"jbsim.wav"
#define DURATION	60
#define DTX		PJ_TRUE
#define PLC		PJ_TRUE
#define MIN_LOST_BURST	0
#define MAX_LOST_BURST	20
#define LOSS_CORR	0
#define LOSS_EXTRA	2
#define SILENT		1

/*
   Test setup:

   Input WAV --> TX Stream --> Loop transport --> RX Stream --> Out WAV
 */

/* Stream settings */
struct stream_cfg
{
    const char	*name;		/* for logging purposes */
    pjmedia_dir	 dir;		/* stream direction	*/
    pj_str_t	 codec;		/* codec name		*/
    unsigned	 ptime;		/* zero for default	*/
    pj_bool_t	 dtx;		/* DTX enabled?		*/
    pj_bool_t	 plc;		/* PLC enabled?		*/
};

/* Stream instance. We will instantiate two streams, TX and RX */
struct stream
{
    pj_pool_t		*pool;
    pjmedia_stream	*strm;
    pjmedia_port	*port;

    /*
     * Running states: 
     */
    union {
	/* TX stream state */
	struct {
	    pj_time_val	next_schedule;	/* Time to send next packet */
	    unsigned	total_tx;	/* # of TX packets so far   */
	    int		total_lost;	/* # of dropped pkts so far */
	    unsigned	cur_lost_burst;	/* current # of lost bursts */
	    unsigned	drop_prob;	/* drop probability value   */
				        
	} tx;

	/* RX stream state */
	struct {
	    pj_time_val	next_schedule;	/* Time to fetch next pkt   */
	} rx;
    } state;
};

/* 
 * Logging 
 */

/* Events names */
#define EVENT_LOG	""
#define EVENT_TX	"TX/PUT"
#define EVENT_TX_DROP	"*** LOSS ***"
#define EVENT_GET_PRE	"GET (pre)"
#define EVENT_GET_POST	"GET (post)"


/* Logging entry */
struct log_entry
{
    pj_time_val			 wall_clock;	/* Wall clock time	    */
    const char			*event;		/* Event name		    */
    pjmedia_jb_state		*jb_state;	/* JB state, optional	    */
    pjmedia_rtcp_stat		*stat;		/* Stream stat, optional    */
    const char			*log;		/* Log message, optional    */
};

/* Test settings, taken from command line */
struct test_cfg
{
    /* General options */
    pj_bool_t	     silent;		/* Write little to stdout   */
    const char	    *log_file;		/* The output log file	    */

    /* Test settings */
    pj_str_t	     codec;		/* Codec to be used	    */
    unsigned	     duration_msec;	/* Test duration	    */

    /* Transmitter setting */
    const char	    *tx_wav_in;		/* Input/reference WAV	    */
    unsigned	     tx_ptime;		/* TX stream ptime	    */
    unsigned	     tx_min_jitter;	/* Minimum jitter in ms	    */
    unsigned	     tx_max_jitter;	/* Max jitter in ms	    */
    unsigned	     tx_dtx;		/* DTX enabled?		    */
    unsigned	     tx_pct_avg_lost;	/* Average loss in percent  */
    unsigned	     tx_min_lost_burst;	/* Min lost burst in #pkt   */
    unsigned	     tx_max_lost_burst;	/* Max lost burst in #pkt   */
    unsigned	     tx_pct_loss_corr;	/* Loss correlation in pct  */

    /* Receiver setting */
    const char	    *rx_wav_out;	/* Output WAV file	    */
    unsigned	     rx_ptime;		/* RX stream ptime	    */
    unsigned	     rx_snd_burst;	/* RX sound burst	    */
    pj_bool_t	     rx_plc;		/* RX PLC enabled?	    */
    int		     rx_jb_init;	/* if > 0 will enable prefetch (ms) */
    int		     rx_jb_min_pre;	/* JB minimum prefetch (ms) */
    int		     rx_jb_max_pre;	/* JB maximum prefetch (ms) */
    int		     rx_jb_max;		/* JB maximum size (ms)	    */
};

/*
 * Global var
 */
struct global_app
{
    pj_caching_pool	 cp;
    pj_pool_t		*pool;
    pj_int16_t		*framebuf;
    pjmedia_endpt	*endpt;
    pjmedia_transport	*loop;

    pj_oshandle_t	 log_fd;

    struct test_cfg	 cfg;

    struct stream	*tx;
    pjmedia_port	*tx_wav;

    struct stream	*rx;
    pjmedia_port	*rx_wav;

    pj_time_val		 wall_clock;
};

static struct global_app g_app;


#ifndef MAX
#   define MAX(a,b)	(a<b ? b : a)
#endif

#ifndef MIN
#   define MIN(a,b)	(a<b ? a : b)
#endif

/*****************************************************************************
 * Logging
 */
static void write_log(struct log_entry *entry, pj_bool_t to_stdout)
{
    /* Format (CSV): */
    const char *format = "TIME;EVENT;#RX packets;#packets lost;#JB prefetch;#JB size;#JBDISCARD;#JBEMPTY;Log Message";
    static char log[2000];
    enum { D = 20 };
    char s_jbprefetch[D],
	 s_jbsize[D],
	 s_rxpkt[D],
	 s_losspkt[D],
	 s_jbdiscard[D],
	 s_jbempty[D];
    static pj_bool_t header_written;

    if (!header_written) {
	pj_ansi_snprintf(log, sizeof(log),
			 "%s\n", format);
	if (g_app.log_fd != NULL) {
	    pj_ssize_t size = strlen(log);
	    pj_file_write(g_app.log_fd, log, &size);
	}
	if (to_stdout && !g_app.cfg.silent)
	    printf("%s", log);
	header_written = PJ_TRUE;
    }

    if (entry->jb_state) {
	sprintf(s_jbprefetch, "%d", entry->jb_state->prefetch);
	sprintf(s_jbsize, "%d", entry->jb_state->size);
	sprintf(s_jbdiscard, "%d", entry->jb_state->discard);
	sprintf(s_jbempty, "%d", entry->jb_state->empty);
    } else {
	strcpy(s_jbprefetch, "");
	strcpy(s_jbsize, "");
	strcpy(s_jbdiscard, "");
	strcpy(s_jbempty, "");
    }

    if (entry->stat) {
	sprintf(s_rxpkt, "%d", entry->stat->rx.pkt);
	sprintf(s_losspkt, "%d", entry->stat->rx.loss);
    } else {
	strcpy(s_rxpkt, "");
	strcpy(s_losspkt, "");
    }

    if (entry->log == NULL)
	entry->log = "";

    pj_ansi_snprintf(log, sizeof(log),
		     "'%d.%03d;"	    /* time */
		     "%s;"	    /* event */
		     "%s;"	    /* rxpkt */
		     "%s;"	    /* jb prefetch */
		     "%s;"	    /* jbsize */
		     "%s;"	    /* losspkt */
		     "%s;"	    /* jbdiscard */
		     "%s;"	    /* jbempty */
		     "%s\n"	    /* logmsg */,

		     (int)entry->wall_clock.sec, (int)entry->wall_clock.msec, /* time */
		     entry->event,
		     s_rxpkt,
		     s_losspkt,
		     s_jbprefetch,
		     s_jbsize,
		     s_jbdiscard,
		     s_jbempty,
		     entry->log
		     );
    if (g_app.log_fd != NULL) {
	pj_ssize_t size = strlen(log);
	pj_file_write(g_app.log_fd, log, &size);
    }

    if (to_stdout && !g_app.cfg.silent)
	printf("%s", log);
}

static void log_cb(int inst_id, int level, const char *data, int len)
{
    struct log_entry entry;

    /* Write to stdout */
    pj_log_write(inst_id, level, data, len, 0);
    puts("");

    /* Also add to CSV file */
    pj_bzero(&entry, sizeof(entry));
    entry.event = EVENT_LOG;
    entry.log = data;
    entry.wall_clock = g_app.wall_clock;
    write_log(&entry, PJ_FALSE);
}

static void jbsim_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(THIS_FILE, "%s: %s", title, errmsg));
}

/*****************************************************************************
 * stream
 */

static void stream_destroy(struct stream *stream)
{
    if (stream->strm)
	pjmedia_stream_destroy(stream->strm);
    if (stream->pool)
	pj_pool_release(stream->pool);
}

static pj_status_t stream_init(const struct stream_cfg *cfg, struct stream **p_stream)
{
    pj_pool_t *pool = NULL;
    struct stream *stream = NULL;
    pjmedia_codec_mgr *cm;
    unsigned count;
    const pjmedia_codec_info *ci;
    pjmedia_stream_info si;
    pj_status_t status;

    /* Create instance */
    pool = pj_pool_create(&g_app.cp.factory, cfg->name, 512, 512, NULL);
    stream = PJ_POOL_ZALLOC_T(pool, struct stream);
    stream->pool = pool;
    
    /* Create stream info */
    pj_bzero(&si, sizeof(si));
    si.type = PJMEDIA_TYPE_AUDIO;
    si.proto = PJMEDIA_TP_PROTO_RTP_AVP;
    si.dir = cfg->dir;
    pj_sockaddr_in_init(&si.rem_addr.ipv4, NULL, 4000);	/* dummy */
    pj_sockaddr_in_init(&si.rem_rtcp.ipv4, NULL, 4001);	/* dummy */

    /* Apply JB settings if this is RX direction */
    if (cfg->dir == PJMEDIA_DIR_DECODING) {
	si.jb_init = g_app.cfg.rx_jb_init;
	si.jb_min_pre = g_app.cfg.rx_jb_min_pre;
	si.jb_max_pre = g_app.cfg.rx_jb_max_pre;
	si.jb_max = g_app.cfg.rx_jb_max;
    }

    /* Get the codec info and param */
    cm = pjmedia_endpt_get_codec_mgr(g_app.endpt);
    count = 1;
    status = pjmedia_codec_mgr_find_codecs_by_id(cm, &cfg->codec, &count, &ci, NULL);
    if (status != PJ_SUCCESS) {
	jbsim_perror("Unable to find codec", status);
	goto on_error;
    }

    pj_memcpy(&si.fmt, ci, sizeof(*ci));

    si.param = PJ_POOL_ALLOC_T(pool, struct pjmedia_codec_param);
    status = pjmedia_codec_mgr_get_default_param(cm, &si.fmt, si.param);
    if (status != PJ_SUCCESS) {
	jbsim_perror("Unable to get codec defaults", status);
	goto on_error;
    }

    si.tx_pt = si.fmt.pt;

    /* Apply ptime setting */
    if (cfg->ptime) {
	si.param->setting.frm_per_pkt = (pj_uint8_t)
					((cfg->ptime + si.param->info.frm_ptime - 1) /
					 si.param->info.frm_ptime);
    }
    /* Apply DTX setting */
    si.param->setting.vad = cfg->dtx;

    /* Apply PLC setting */
    si.param->setting.plc = cfg->plc;

    /* Create stream */
    status = pjmedia_stream_create(g_app.endpt, pool, &si, g_app.loop, NULL, &stream->strm);
    if (status != PJ_SUCCESS) {
	jbsim_perror("Error creating stream", status);
	goto on_error;
    }

    status = pjmedia_stream_get_port(stream->strm, &stream->port);
    if (status != PJ_SUCCESS) {
	jbsim_perror("Error retrieving stream", status);
	goto on_error;
    }

    /* Start stream */
    status = pjmedia_stream_start(stream->strm);
    if (status != PJ_SUCCESS) {
	jbsim_perror("Error starting stream", status);
	goto on_error;
    }

    /* Done */
    *p_stream = stream;
    return PJ_SUCCESS;

on_error:
    if (stream) {
	stream_destroy(stream);
    } else {
	if (pool)
	    pj_pool_release(pool);
    }
    return status;
}


/*****************************************************************************
 * The test session
 */
static void test_destroy(void)
{
    if (g_app.tx)
	stream_destroy(g_app.tx);
    if (g_app.tx_wav)
	pjmedia_port_destroy(g_app.tx_wav);
    if (g_app.rx)
	stream_destroy(g_app.rx);
    if (g_app.rx_wav)
	pjmedia_port_destroy(g_app.rx_wav);
    if (g_app.loop)
	pjmedia_transport_close(g_app.loop);
    if (g_app.endpt)
	pjmedia_endpt_destroy( g_app.endpt );
    if (g_app.log_fd) {
	pj_log_set_log_func(&pj_log_write);
	pj_log_set_decor(pj_log_get_decor() | PJ_LOG_HAS_NEWLINE);
	pj_file_close(g_app.log_fd);
	g_app.log_fd = NULL;
    }
    if (g_app.pool)
	pj_pool_release(g_app.pool);
    pj_caching_pool_destroy( &g_app.cp );
    pj_shutdown(0);
}


static pj_status_t test_init(void)
{
    struct stream_cfg strm_cfg;
    pj_status_t status;

    /* Must init PJLIB first: */
    status = pj_init(0);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(0, &g_app.cp, &pj_pool_factory_default_policy, 0);

    /* Pool */
    g_app.pool = pj_pool_create(&g_app.cp.factory, "g_app", 512, 512, NULL);

    /* Log file */
    if (g_app.cfg.log_file) {
	status = pj_file_open(g_app.pool, g_app.cfg.log_file, 
			      PJ_O_WRONLY,
			      &g_app.log_fd, NULL);
	if (status != PJ_SUCCESS) {
	    jbsim_perror("Error writing output file", status);
	    goto on_error;
	}

	pj_log_set_decor(PJ_LOG_HAS_SENDER | PJ_LOG_HAS_COLOR | PJ_LOG_HAS_LEVEL_TEXT);
	pj_log_set_log_func(&log_cb);
    }

    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
    //status = pjmedia_endpt_create(&g_app.cp.factory, NULL, 0, &g_app.endpt);
    status = pjmedia_endpt_create(0, &g_app.cp.factory, NULL, 0,0, &g_app.endpt);
    if (status != PJ_SUCCESS) {
	jbsim_perror("Error creating media endpoint", status);
	goto on_error;
    }

    /* Register codecs */
#if defined(PJMEDIA_HAS_GSM_CODEC) && PJMEDIA_HAS_GSM_CODEC != 0
    pjmedia_codec_gsm_init(g_app.endpt);
#endif
#if defined(PJMEDIA_HAS_G711_CODEC) && PJMEDIA_HAS_G711_CODEC!=0
    pjmedia_codec_g711_init(g_app.endpt);
#endif
#if defined(PJMEDIA_HAS_SPEEX_CODEC) && PJMEDIA_HAS_SPEEX_CODEC!=0
    pjmedia_codec_speex_init(g_app.endpt, 0, PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY,
			     PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY);
#endif
#if defined(PJMEDIA_HAS_G722_CODEC) && (PJMEDIA_HAS_G722_CODEC != 0)
    pjmedia_codec_g722_init(g_app.endpt);
#endif
#if defined(PJMEDIA_HAS_ILBC_CODEC) && PJMEDIA_HAS_ILBC_CODEC != 0
    /* Init ILBC with mode=20 to make the losts occur at the same
     * places as other codecs.
     */
    pjmedia_codec_ilbc_init(g_app.endpt, 20);
#endif
#if defined(PJMEDIA_HAS_INTEL_IPP) && PJMEDIA_HAS_INTEL_IPP != 0
    pjmedia_codec_ipp_init(g_app.endpt);
#endif
#if defined(PJMEDIA_HAS_OPENCORE_AMRNB_CODEC) && (PJMEDIA_HAS_OPENCORE_AMRNB_CODEC != 0)
    pjmedia_codec_opencore_amrnb_init(g_app.endpt);
#endif
#if defined(PJMEDIA_HAS_L16_CODEC) && PJMEDIA_HAS_L16_CODEC != 0
    pjmedia_codec_l16_init(g_app.endpt, 0);
#endif

    /* Create the loop transport */
    status = pjmedia_transport_loop_create(g_app.endpt, &g_app.loop);
    if (status != PJ_SUCCESS) {
	jbsim_perror("Error creating loop transport", status);
	goto on_error;
    }

    /* Create transmitter stream */
    pj_bzero(&strm_cfg, sizeof(strm_cfg));
    strm_cfg.name = "tx";
    strm_cfg.dir = PJMEDIA_DIR_ENCODING;
    strm_cfg.codec = g_app.cfg.codec;
    strm_cfg.ptime = g_app.cfg.tx_ptime;
    strm_cfg.dtx = g_app.cfg.tx_dtx;
    strm_cfg.plc = PJ_TRUE;
    status = stream_init(&strm_cfg, &g_app.tx);
    if (status != PJ_SUCCESS) 
	goto on_error;

    /* Create transmitter WAV */
    status = pjmedia_wav_player_port_create(g_app.pool, 
					    g_app.cfg.tx_wav_in,
					    g_app.cfg.tx_ptime,
					    0,
					    0,
					    &g_app.tx_wav);
    if (status != PJ_SUCCESS) {
	jbsim_perror("Error reading input WAV file", status);
	goto on_error;
    }

    /* Make sure stream and WAV parameters match */
    if (g_app.tx_wav->info.clock_rate != g_app.tx->port->info.clock_rate ||
	g_app.tx_wav->info.channel_count != g_app.tx->port->info.channel_count)
    {
	jbsim_perror("Error: Input WAV file has different clock rate "
		     "or number of channels than the codec", PJ_SUCCESS);
	goto on_error;
    }


    /* Create receiver */
    pj_bzero(&strm_cfg, sizeof(strm_cfg));
    strm_cfg.name = "rx";
    strm_cfg.dir = PJMEDIA_DIR_DECODING;
    strm_cfg.codec = g_app.cfg.codec;
    strm_cfg.ptime = g_app.cfg.rx_ptime;
    strm_cfg.dtx = PJ_TRUE;
    strm_cfg.plc = g_app.cfg.rx_plc;
    status = stream_init(&strm_cfg, &g_app.rx);
    if (status != PJ_SUCCESS) 
	goto on_error;

    /* Create receiver WAV */
    status = pjmedia_wav_writer_port_create(g_app.pool, 
					    g_app.cfg.rx_wav_out,
					    g_app.rx->port->info.clock_rate,
					    g_app.rx->port->info.channel_count,
					    g_app.rx->port->info.samples_per_frame,
					    g_app.rx->port->info.bits_per_sample,
					    0,
					    0,
					    &g_app.rx_wav);
    if (status != PJ_SUCCESS) {
	jbsim_perror("Error creating output WAV file", status);
	goto on_error;
    }


    /* Frame buffer */
    g_app.framebuf = (pj_int16_t*)
		     pj_pool_alloc(g_app.pool,
				   MAX(g_app.rx->port->info.samples_per_frame,
				       g_app.tx->port->info.samples_per_frame) * sizeof(pj_int16_t));


    /* Set the receiver in the loop transport */
    pjmedia_transport_loop_disable_rx(g_app.loop, g_app.tx->strm, PJ_TRUE);

    /* Done */
    return PJ_SUCCESS;

on_error:
    test_destroy();
    return status;
}

static void run_one_frame(pjmedia_port *src, pjmedia_port *dst,
			  pj_bool_t *has_frame)
{
    pjmedia_frame frame;
    pj_status_t status;

    pj_bzero(&frame, sizeof(frame));
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame.buf = g_app.framebuf;
    frame.size = dst->info.samples_per_frame * 2;
    
    status = pjmedia_port_get_frame(src, &frame);
    pj_assert(status == PJ_SUCCESS);

    if (status!= PJ_SUCCESS || frame.type != PJMEDIA_FRAME_TYPE_AUDIO) {
	frame.buf = g_app.framebuf;
	pjmedia_zero_samples(g_app.framebuf, src->info.samples_per_frame);
	frame.size = src->info.samples_per_frame * 2;
	if (has_frame)
	    *has_frame = PJ_FALSE;
    } else {
	if (has_frame)
	    *has_frame = PJ_TRUE;
    }


    status = pjmedia_port_put_frame(dst, &frame);
    pj_assert(status == PJ_SUCCESS);
}


/* This is the transmission "tick".
 * This function is called periodically every "tick" milliseconds, and
 * it will determine whether to transmit packet(s) (or to drop it).
 */
static void tx_tick(const pj_time_val *t)
{
    struct stream *strm = g_app.tx;
    static char log_msg[120];
    pjmedia_port *port = g_app.tx->port;
    long pkt_interval; 

    /* packet interval, without jitter */
    pkt_interval = port->info.samples_per_frame * 1000 /
		   port->info.clock_rate;

    while (PJ_TIME_VAL_GTE(*t, strm->state.tx.next_schedule)) {
	struct log_entry entry;
	pj_bool_t drop_this_pkt = PJ_FALSE;
	int jitter;

	/* Init log entry */
	pj_bzero(&entry, sizeof(entry));
	entry.wall_clock = *t;

	/* 
	 * Determine whether to drop this packet 
	 */
	if (strm->state.tx.cur_lost_burst) {
	    /* We are currently dropping packet */

	    /* Make it comply to minimum lost burst */
	    if (strm->state.tx.cur_lost_burst < g_app.cfg.tx_min_lost_burst) {
		drop_this_pkt = PJ_TRUE;
	    }

	    /* Correlate the next packet loss */
	    if (!drop_this_pkt && 
		strm->state.tx.cur_lost_burst < g_app.cfg.tx_max_lost_burst &&
		MAX(strm->state.tx.total_lost-LOSS_EXTRA,0) * 100 / MAX(strm->state.tx.total_tx,1) < g_app.cfg.tx_pct_avg_lost
	       ) 
	    {
		strm->state.tx.drop_prob = ((g_app.cfg.tx_pct_loss_corr * strm->state.tx.drop_prob) +
					     ((100-g_app.cfg.tx_pct_loss_corr) * (pj_rand()%100))
					   ) / 100;
		if (strm->state.tx.drop_prob >= 100)
		    strm->state.tx.drop_prob = 99;

		if (strm->state.tx.drop_prob >= 100 - g_app.cfg.tx_pct_avg_lost)
		    drop_this_pkt = PJ_TRUE;
	    }
	}

	/* If we're not dropping packet then use randomly distributed loss */
	if (!drop_this_pkt &&
	    MAX(strm->state.tx.total_lost-LOSS_EXTRA,0) * 100 / MAX(strm->state.tx.total_tx,1) < g_app.cfg.tx_pct_avg_lost)
	{
	    strm->state.tx.drop_prob = pj_rand() % 100;

	    if (strm->state.tx.drop_prob >= 100 - g_app.cfg.tx_pct_avg_lost)
		drop_this_pkt = PJ_TRUE;
	}

	if (drop_this_pkt) {
	    /* Drop the frame */
	    pjmedia_transport_simulate_lost(g_app.loop, PJMEDIA_DIR_ENCODING, 100);
	    run_one_frame(g_app.tx_wav, g_app.tx->port, NULL);
	    pjmedia_transport_simulate_lost(g_app.loop, PJMEDIA_DIR_ENCODING, 0);

	    entry.event = EVENT_TX_DROP;
	    entry.log = "** This packet was lost **";

	    ++strm->state.tx.total_lost;
	    ++strm->state.tx.cur_lost_burst;

	} else {
	    pjmedia_rtcp_stat stat;
	    pjmedia_jb_state jstate;
	    unsigned last_discard;

	    pjmedia_stream_get_stat_jbuf(g_app.rx->strm, &jstate);
	    last_discard = jstate.discard;

	    run_one_frame(g_app.tx_wav, g_app.tx->port, NULL);

	    pjmedia_stream_get_stat(g_app.rx->strm, &stat);
	    pjmedia_stream_get_stat_jbuf(g_app.rx->strm, &jstate);

	    entry.event = EVENT_TX;
	    entry.jb_state = &jstate;
	    entry.stat = &stat;
	    entry.log = log_msg;

	    if (jstate.discard > last_discard)
		strcat(log_msg, "** Note: packet was discarded by jitter buffer **");

	    strm->state.tx.cur_lost_burst = 0;
	}

	write_log(&entry, PJ_TRUE);

	++strm->state.tx.total_tx;

	/* Calculate next schedule */
	strm->state.tx.next_schedule.sec = 0;
	strm->state.tx.next_schedule.msec = (strm->state.tx.total_tx + 1) * pkt_interval;

	/* Apply jitter */
	if (g_app.cfg.tx_max_jitter || g_app.cfg.tx_min_jitter) {

	    if (g_app.cfg.tx_max_jitter == g_app.cfg.tx_min_jitter) {
		/* Fixed jitter */
		switch (pj_rand() % 3) {
		case 0:
		    jitter = 0 - g_app.cfg.tx_min_jitter;
		    break;
		case 2:
		    jitter = g_app.cfg.tx_min_jitter;
		    break;
		default:
		    jitter = 0;
		    break;
		}
	    } else {
		int jitter_range;
		jitter_range = (g_app.cfg.tx_max_jitter-g_app.cfg.tx_min_jitter)*2;
		jitter = pj_rand() % jitter_range;
		if (jitter < jitter_range/2) {
		    jitter = 0 - g_app.cfg.tx_min_jitter - (jitter/2);
		} else {
		    jitter = g_app.cfg.tx_min_jitter + (jitter/2);
		}
	    }

	} else {
	    jitter = 0;
	}

	pj_time_val_normalize(&strm->state.tx.next_schedule);

	sprintf(log_msg, "** Packet #%u tick is at %d.%03d, %d ms jitter applied **", 
		strm->state.tx.total_tx+1,
		(int)strm->state.tx.next_schedule.sec, (int)strm->state.tx.next_schedule.msec,
		jitter);

	strm->state.tx.next_schedule.msec += jitter;
	pj_time_val_normalize(&strm->state.tx.next_schedule);

    } /* while */
}


/* This is the RX "tick".
 * This function is called periodically every "tick" milliseconds, and
 * it will determine whether to call get_frame() from the RX stream.
 */
static void rx_tick(const pj_time_val *t)
{
    struct stream *strm = g_app.rx;
    pjmedia_port *port = g_app.rx->port;
    long pkt_interval;

    pkt_interval = port->info.samples_per_frame * 1000 /
		   port->info.clock_rate *
		   g_app.cfg.rx_snd_burst;

    if (PJ_TIME_VAL_GTE(*t, strm->state.rx.next_schedule)) {
	unsigned i;
	for (i=0; i<g_app.cfg.rx_snd_burst; ++i) {
	    struct log_entry entry;
	    pjmedia_rtcp_stat stat;
	    pjmedia_jb_state jstate;
	    pj_bool_t has_frame;
	    char msg[120];
	    unsigned last_empty;

	    pjmedia_stream_get_stat(g_app.rx->strm, &stat);
	    pjmedia_stream_get_stat_jbuf(g_app.rx->strm, &jstate);
	    last_empty = jstate.empty;

	    /* Pre GET event */
	    pj_bzero(&entry, sizeof(entry));
	    entry.event = EVENT_GET_PRE;
	    entry.wall_clock = *t;
	    entry.stat = &stat;
	    entry.jb_state = &jstate;

	    write_log(&entry, PJ_TRUE);

	    /* GET */
	    run_one_frame(g_app.rx->port, g_app.rx_wav, &has_frame);

	    /* Post GET event */
	    pjmedia_stream_get_stat(g_app.rx->strm, &stat);
	    pjmedia_stream_get_stat_jbuf(g_app.rx->strm, &jstate);

	    pj_bzero(&entry, sizeof(entry));
	    entry.event = EVENT_GET_POST;
	    entry.wall_clock = *t;
	    entry.stat = &stat;
	    entry.jb_state = &jstate;

	    msg[0] = '\0';
	    entry.log = msg;

	    if (jstate.empty > last_empty)
		strcat(msg, "** JBUF was empty **");
	    if (!has_frame)
		strcat(msg, "** NULL frame was returned **");

	    write_log(&entry, PJ_TRUE);

	}


	strm->state.rx.next_schedule.msec += pkt_interval;
	pj_time_val_normalize(&strm->state.rx.next_schedule);
    }
	    
}

static void test_loop(long duration)
{
    g_app.wall_clock.sec = 0;
    g_app.wall_clock.msec = 0;

    while (PJ_TIME_VAL_MSEC(g_app.wall_clock) <= duration) {

	/* Run TX tick */
	tx_tick(&g_app.wall_clock);

	/* Run RX tick */
	rx_tick(&g_app.wall_clock);

	/* Increment tick */
	g_app.wall_clock.msec += WALL_CLOCK_TICK;
	pj_time_val_normalize(&g_app.wall_clock);
    }
}


/*****************************************************************************
 * usage()
 */
enum {
    OPT_CODEC	    = 'c',
    OPT_INPUT	    = 'i',
    OPT_OUTPUT	    = 'o',
    OPT_DURATION    = 'd',
    OPT_LOG_FILE    = 'l',
    OPT_LOSS	    = 'x',
    OPT_MIN_JITTER  = 'j',
    OPT_MAX_JITTER  = 'J',
    OPT_SND_BURST   = 'b',
    OPT_TX_PTIME    = 't',
    OPT_RX_PTIME    = 'r',
    OPT_NO_VAD	    = 'U',
    OPT_NO_PLC	    = 'p',
    OPT_JB_PREFETCH = 'P', 
    OPT_JB_MIN_PRE  = 'm',
    OPT_JB_MAX_PRE  = 'M',
    OPT_JB_MAX	    = 'X',
    OPT_HELP	    = 'h',
    OPT_MIN_LOST_BURST = 1,
    OPT_MAX_LOST_BURST,
    OPT_LOSS_CORR,
};


static void usage(void)
{
    printf("jbsim - System and network impairments simulator\n");
    printf("Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)\n");
    printf("\n");
    printf("This program emulates various system and network impairment\n");
    printf("conditions as well as application parameters and apply it to\n");
    printf("an input WAV file. The output is another WAV file as well as\n");
    printf("a detailed log file (in CSV format) for troubleshooting.\n");
    printf("\n");
    printf("Usage:\n");
    printf(" jbsim [OPTIONS]\n");
    printf("\n");
    printf("General OPTIONS:\n");
    printf("  --codec, -%c NAME       Set the audio codec\n", OPT_CODEC);
    printf("                         Default: %s\n", CODEC);
    printf("  --input, -%c FILE       Set WAV reference file to FILE\n", OPT_INPUT);
    printf("                         Default: " WAV_REF "\n");
    printf("  --output, -%c FILE      Set WAV output file to FILE\n", OPT_OUTPUT);
    printf("                         Default: " WAV_OUT "\n");
    printf("  --duration, -%c SEC     Set test duration to SEC seconds\n", OPT_DURATION);
    printf("                         Default: %d\n", DURATION);
    printf("  --log-file, -%c FILE    Save simulation log file to FILE\n", OPT_LOG_FILE);
    printf("                         Note: FILE will be in CSV format with semicolon separator\n");
    printf("                         Default: %s\n", LOG_FILE);
    printf("  --help, -h             Display this screen\n");
    printf("\n");
    printf("Simulation OPTIONS:\n");
    printf("  --loss, -%c PCT         Set packet average loss to PCT percent\n", OPT_LOSS);
    printf("                         Default: 0\n");
    printf("  --loss-corr PCT        Set the loss correlation to PCT percent. Default: 0\n");
    printf("  --min-lost-burst N     Set minimum packet lost burst (default:%d)\n", MIN_LOST_BURST);
    printf("  --max-lost-burst N     Set maximum packet lost burst (default:%d)\n", MAX_LOST_BURST);
    printf("  --min-jitter, -%c MSEC  Set minimum network jitter to MSEC\n", OPT_MIN_JITTER);
    printf("                         Default: 0\n");
    printf("  --max-jitter, -%c MSEC  Set maximum network jitter to MSEC\n", OPT_MAX_JITTER);
    printf("                         Default: 0\n");
    printf("  --snd-burst, -%c VAL    Set RX sound burst value to VAL frames.\n", OPT_SND_BURST);
    printf("                         Default: 1\n");
    printf("  --tx-ptime, -%c MSEC    Set transmitter ptime to MSEC\n", OPT_TX_PTIME);
    printf("                         Default: 0 (not set, use default)\n");
    printf("  --rx-ptime, -%c MSEC    Set receiver ptime to MSEC\n", OPT_RX_PTIME);
    printf("                         Default: 0 (not set, use default)\n");
    printf("  --no-vad, -%c           Disable VAD/DTX in transmitter\n", OPT_NO_VAD);
    printf("  --no-plc, -%c           Disable PLC in receiver\n", OPT_NO_PLC);
    printf("  --jb-prefetch, -%c      Enable prefetch bufferring in jitter buffer\n", OPT_JB_PREFETCH);
    printf("  --jb-min-pre, -%c MSEC  Jitter buffer minimum prefetch delay in msec\n", OPT_JB_MIN_PRE);
    printf("  --jb-max-pre, -%c MSEC  Jitter buffer maximum prefetch delay in msec\n", OPT_JB_MAX_PRE);
    printf("  --jb-max, -%c MSEC      Set maximum delay that can be accomodated by the\n", OPT_JB_MAX);
    printf("                         jitter buffer msec.\n");
}


static int init_options(int argc, char *argv[])
{
    struct pj_getopt_option long_options[] = {
	{ "codec",	    1, 0, OPT_CODEC },
	{ "input",	    1, 0, OPT_INPUT },
	{ "output",	    1, 0, OPT_OUTPUT },
	{ "duration",	    1, 0, OPT_DURATION },
	{ "log-file",	    1, 0, OPT_LOG_FILE},
	{ "loss",	    1, 0, OPT_LOSS },
	{ "min-lost-burst", 1, 0, OPT_MIN_LOST_BURST},
	{ "max-lost-burst", 1, 0, OPT_MAX_LOST_BURST},
	{ "loss-corr",	    1, 0, OPT_LOSS_CORR},
	{ "min-jitter",	    1, 0, OPT_MIN_JITTER },
	{ "max-jitter",	    1, 0, OPT_MAX_JITTER },
	{ "snd-burst",	    1, 0, OPT_SND_BURST },
	{ "tx-ptime",	    1, 0, OPT_TX_PTIME },
	{ "rx-ptime",	    1, 0, OPT_RX_PTIME },
	{ "no-vad",	    0, 0, OPT_NO_VAD },
	{ "no-plc",	    0, 0, OPT_NO_PLC },
	{ "jb-prefetch",    0, 0, OPT_JB_PREFETCH },
	{ "jb-min-pre",     1, 0, OPT_JB_MIN_PRE },
	{ "jb-max-pre",     1, 0, OPT_JB_MAX_PRE },
	{ "jb-max",	    1, 0, OPT_JB_MAX },
	{ "help",	    0, 0, OPT_HELP},
	{ NULL, 0, 0, 0 },
    };
    int c;
    int option_index;
    char format[128];

    /* Init default config */
    g_app.cfg.codec = pj_str(CODEC);
    g_app.cfg.duration_msec = DURATION * 1000;
    g_app.cfg.silent = SILENT;
    g_app.cfg.log_file = LOG_FILE;
    g_app.cfg.tx_wav_in = WAV_REF;
    g_app.cfg.tx_ptime = 0;
    g_app.cfg.tx_min_jitter = 0;
    g_app.cfg.tx_max_jitter = 0;
    g_app.cfg.tx_dtx = DTX;
    g_app.cfg.tx_pct_avg_lost = 0;
    g_app.cfg.tx_min_lost_burst = MIN_LOST_BURST;
    g_app.cfg.tx_max_lost_burst = MAX_LOST_BURST;
    g_app.cfg.tx_pct_loss_corr = LOSS_CORR;

    g_app.cfg.rx_wav_out = WAV_OUT;
    g_app.cfg.rx_ptime = 0;
    g_app.cfg.rx_plc = PLC;
    g_app.cfg.rx_snd_burst = 1;
    g_app.cfg.rx_jb_init = -1;
    g_app.cfg.rx_jb_min_pre = -1;
    g_app.cfg.rx_jb_max_pre = -1;
    g_app.cfg.rx_jb_max = -1;

    /* Build format */
    format[0] = '\0';
    for (c=0; c<PJ_ARRAY_SIZE(long_options)-1; ++c) {
	if (long_options[c].has_arg) {
	    char cmd[10];
	    pj_ansi_snprintf(cmd, sizeof(cmd), "%c:", long_options[c].val);
	    pj_ansi_strcat(format, cmd);
	}
    }
    for (c=0; c<PJ_ARRAY_SIZE(long_options)-1; ++c) {
	if (long_options[c].has_arg == 0) {
	    char cmd[10];
	    pj_ansi_snprintf(cmd, sizeof(cmd), "%c", long_options[c].val);
	    pj_ansi_strcat(format, cmd);
	}
    }

    /* Parse options */
    pj_optind = 0;
    while((c=pj_getopt_long(argc,argv, format, 
			    long_options, &option_index))!=-1) 
    {
	switch (c) {
	case OPT_CODEC:
	    g_app.cfg.codec = pj_str(pj_optarg);
	    break;
	case OPT_INPUT:
	    g_app.cfg.tx_wav_in = pj_optarg;
	    break;
	case OPT_OUTPUT:
	    g_app.cfg.rx_wav_out = pj_optarg;
	    break;
	case OPT_DURATION:
	    g_app.cfg.duration_msec = atoi(pj_optarg) * 1000;
	    break;
	case OPT_LOG_FILE:
	    g_app.cfg.log_file = pj_optarg;
	    break;
	case OPT_LOSS:
	    g_app.cfg.tx_pct_avg_lost = atoi(pj_optarg);
	    if (g_app.cfg.tx_pct_avg_lost > 100) {
		puts("Error: Invalid loss value?");
		return 1;
	    }
	    break;
	case OPT_MIN_LOST_BURST:
	    g_app.cfg.tx_min_lost_burst = atoi(pj_optarg);
	    break;
	case OPT_MAX_LOST_BURST:
	    g_app.cfg.tx_max_lost_burst = atoi(pj_optarg);
	    break;
	case OPT_LOSS_CORR:
	    g_app.cfg.tx_pct_loss_corr = atoi(pj_optarg);
	    if (g_app.cfg.tx_pct_avg_lost > 100) {
		puts("Error: Loss correlation is in percentage, value is not valid?");
		return 1;
	    }
	    break;
	case OPT_MIN_JITTER:
	    g_app.cfg.tx_min_jitter = atoi(pj_optarg);
	    break;
	case OPT_MAX_JITTER:
	    g_app.cfg.tx_max_jitter = atoi(pj_optarg);
	    break;
	case OPT_SND_BURST:
	    g_app.cfg.rx_snd_burst = atoi(pj_optarg);
	    break;
	case OPT_TX_PTIME:
	    g_app.cfg.tx_ptime = atoi(pj_optarg);
	    break;
	case OPT_RX_PTIME:
	    g_app.cfg.rx_ptime = atoi(pj_optarg);
	    break;
	case OPT_NO_VAD:
	    g_app.cfg.tx_dtx = PJ_FALSE;
	    break;
	case OPT_NO_PLC:
	    g_app.cfg.rx_plc = PJ_FALSE;
	    break;
	case OPT_JB_PREFETCH:
	    g_app.cfg.rx_jb_init = 1;
	    break;
	case OPT_JB_MIN_PRE:
	    g_app.cfg.rx_jb_min_pre = atoi(pj_optarg);
	    break;
	case OPT_JB_MAX_PRE:
	    g_app.cfg.rx_jb_max_pre = atoi(pj_optarg);
	    break;
	case OPT_JB_MAX:
	    g_app.cfg.rx_jb_max = atoi(pj_optarg);
	    break;
	case OPT_HELP:
	    usage();
	    return 1;
	default:
	    usage();
	    return 1;
	}
    }

    /* Check for orphaned params */
    if (pj_optind < argc) {
	usage();
	return 1;
    }

    /* Normalize options */
    if (g_app.cfg.rx_jb_init < g_app.cfg.rx_jb_min_pre)
	g_app.cfg.rx_jb_init = g_app.cfg.rx_jb_min_pre;
    else if (g_app.cfg.rx_jb_init > g_app.cfg.rx_jb_max_pre)
	g_app.cfg.rx_jb_init = g_app.cfg.rx_jb_max_pre;

    if (g_app.cfg.tx_max_jitter < g_app.cfg.tx_min_jitter)
	g_app.cfg.tx_max_jitter = g_app.cfg.tx_min_jitter;
    return 0;
}

/*****************************************************************************
 * main()
 */
int main(int argc, char *argv[])
{
    pj_status_t status;

    if (init_options(argc, argv) != 0)
	return 1;


    /* Init */
    status = test_init();
    if (status != PJ_SUCCESS)
	return 1;

    /* Print parameters */
    PJ_LOG(3,(THIS_FILE, "Starting simulation. Parameters: "));
    PJ_LOG(3,(THIS_FILE, "  Codec=%.*s, tx_ptime=%d, rx_ptime=%d",
	      (int)g_app.cfg.codec.slen,
	      g_app.cfg.codec.ptr,
	      g_app.cfg.tx_ptime,
	      g_app.cfg.rx_ptime));
    PJ_LOG(3,(THIS_FILE, " Loss avg=%d%%, min_burst=%d, max_burst=%d",
	      g_app.cfg.tx_pct_avg_lost,
	      g_app.cfg.tx_min_lost_burst,
	      g_app.cfg.tx_max_lost_burst));
    PJ_LOG(3,(THIS_FILE, " TX jitter min=%dms, max=%dms",
	      g_app.cfg.tx_min_jitter, 
	      g_app.cfg.tx_max_jitter));
    PJ_LOG(3,(THIS_FILE, " RX jb init:%dms, min_pre=%dms, max_pre=%dms, max=%dms",
	      g_app.cfg.rx_jb_init,
	      g_app.cfg.rx_jb_min_pre,
	      g_app.cfg.rx_jb_max_pre,
	      g_app.cfg.rx_jb_max));
    PJ_LOG(3,(THIS_FILE, " RX sound burst:%d frames",
	      g_app.cfg.rx_snd_burst));
    PJ_LOG(3,(THIS_FILE, " DTX=%d, PLC=%d",
	      g_app.cfg.tx_dtx, g_app.cfg.rx_plc));

    /* Run test loop */
    test_loop(g_app.cfg.duration_msec);

    /* Print statistics */
    PJ_LOG(3,(THIS_FILE, "Simulation done"));
    PJ_LOG(3,(THIS_FILE, " TX packets=%u, dropped=%u/%5.1f%%",
	      g_app.tx->state.tx.total_tx,
	      g_app.tx->state.tx.total_lost,
	      (float)(g_app.tx->state.tx.total_lost * 100.0 / g_app.tx->state.tx.total_tx)));

    /* Done */
    test_destroy();

    return 0;
}
