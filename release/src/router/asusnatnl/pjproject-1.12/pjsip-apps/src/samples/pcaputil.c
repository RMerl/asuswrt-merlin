/* $Id: pcaputil.c 3816 2011-10-14 04:15:15Z bennylp $ */
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>

static const char *USAGE =
"pcaputil [options] INPUT OUTPUT\n"
"\n"
"  Convert captured RTP packets in PCAP file to WAV file or play it\n"
"  to audio device.\n"
"\n"
"  INPUT  is the PCAP file name/path.\n"
"  OUTPUT is the WAV file name/path to store the output, or set to \"-\",\n"
"         to play the output to audio device. The program will decode\n"
"         the RTP contents using codec that is available in PJMEDIA,\n"
"         and optionally decrypt the content using the SRTP crypto and\n"
"         keys below.\n"
"\n"
"Options to filter packets from PCAP file:\n"
"(you can always select the relevant packets from Wireshark of course!)\n"
"  --src-ip=IP            Only include packets from this source address\n"
"  --dst-ip=IP            Only include packets destined to this address\n"
"  --src-port=port        Only include packets from this source port number\n"
"  --dst-port=port        Only include packets destined to this port number\n"
"\n"
"Options for RTP packet processing:\n"
""
"  --codec=codec_id	  The codec ID formatted \"name/clock-rate/channel-count\"\n"
"                         must be specified for codec with dynamic PT,\n"
"                         e.g: \"Speex/8000\"\n"
"  --srtp-crypto=TAG, -c  Set crypto to be used to decrypt SRTP packets. Valid\n"
"                         tags are: \n"
"                           AES_CM_128_HMAC_SHA1_80 \n"
"                           AES_CM_128_HMAC_SHA1_32\n"
"  --srtp-key=KEY, -k     Set the base64 key to decrypt SRTP packets.\n"
"\n"
"Options for playing to audio device:\n"
""
"  --play-dev-id=dev_id   Audio device ID for playback.\n"
"\n"
"  Example:\n"
"    pcaputil file.pcap output.wav\n"
"    pcaputil -c AES_CM_128_HMAC_SHA1_80 \\\n"
"             -k VLDONbsbGl2Puqy+0PV7w/uGfpSPKFevDpxGsxN3 \\\n"
"             file.pcap output.wav\n"
"\n"
;

static struct app
{
    pj_caching_pool	 cp;
    pj_pool_t		*pool;
    pjmedia_endpt	*mept;
    pj_pcap_file	*pcap;
    pjmedia_port	*wav;
    pjmedia_codec	*codec;
    pjmedia_aud_stream  *aud_strm;
    unsigned		 pt;
    pjmedia_transport	*srtp;
    pjmedia_rtp_session	 rtp_sess;
    pj_bool_t		 rtp_sess_init;
} app;


static void cleanup()
{
    if (app.srtp) pjmedia_transport_close(app.srtp);
    if (app.wav) {
        pj_ssize_t pos = pjmedia_wav_writer_port_get_pos(app.wav);
        if (pos >= 0) {
            unsigned msec;
            msec = pos / 2 * 1000 / app.wav->info.clock_rate;
            printf("Written: %dm:%02ds.%03d\n",
                    msec / 1000 / 60,
                    (msec / 1000) % 60,
                    msec % 1000);
        }
	pjmedia_port_destroy(app.wav);
    }
    if (app.pcap) pj_pcap_close(app.pcap);
    if (app.codec) {
	pjmedia_codec_mgr *cmgr;
	app.codec->op->close(app.codec);
	cmgr = pjmedia_endpt_get_codec_mgr(app.mept);
	pjmedia_codec_mgr_dealloc_codec(cmgr, app.codec);
    }
    if (app.aud_strm) {
	pjmedia_aud_stream_stop(app.aud_strm);
	pjmedia_aud_stream_destroy(app.aud_strm);
    }
    if (app.mept) pjmedia_endpt_destroy(app.mept);
    if (app.pool) pj_pool_release(app.pool);
    pj_caching_pool_destroy(&app.cp);
    pj_shutdown(0);
}

static void err_exit(const char *title, pj_status_t status)
{
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(status, errmsg, sizeof(errmsg));
	printf("Error: %s: %s\n", title, errmsg);
    } else {
	printf("Error: %s\n", title);
    }
    cleanup();
    exit(1);
}

#define T(op)	    do { \
			status = op; \
			if (status != PJ_SUCCESS) \
    			    err_exit(#op, status); \
		    } while (0)


static void read_rtp(pj_uint8_t *buf, pj_size_t bufsize,
		     pjmedia_rtp_hdr **rtp,
		     pj_uint8_t **payload,
		     unsigned *payload_size,
		     pj_bool_t check_pt)
{
    pj_status_t status;

    /* Init RTP session */
    if (!app.rtp_sess_init) {
	T(pjmedia_rtp_session_init(&app.rtp_sess, 0, 0));
	app.rtp_sess_init = PJ_TRUE;
    }

    /* Loop reading until we have a good RTP packet */
    for (;;) {
	pj_size_t sz = bufsize;
	const pjmedia_rtp_hdr *r;
	const void *p;
	pjmedia_rtp_status seq_st;

	status = pj_pcap_read_udp(app.pcap, NULL, buf, &sz);
	if (status != PJ_SUCCESS)
	    err_exit("Error reading PCAP file", status);

	/* Decode RTP packet to make sure that this is an RTP packet.
	 * We will decode it again to get the payload after we do
	 * SRTP decoding
	 */
	status = pjmedia_rtp_decode_rtp(&app.rtp_sess, buf, sz, &r, 
					&p, payload_size);
	if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    printf("Not RTP packet, skipping packet: %s\n", errmsg);
	    continue;
	}

	/* Decrypt SRTP */
#if PJMEDIA_HAS_SRTP
	if (app.srtp) {
	    int len = sz;
	    status = pjmedia_transport_srtp_decrypt_pkt(app.srtp, PJ_TRUE, 
						        buf, &len);
	    if (status != PJ_SUCCESS) {
		char errmsg[PJ_ERR_MSG_SIZE];
		pj_strerror(status, errmsg, sizeof(errmsg));
		printf("SRTP packet decryption failed, skipping packet: %s\n", 
			errmsg);
		continue;
	    }
	    sz = len;

	    /* Decode RTP packet again */
	    status = pjmedia_rtp_decode_rtp(&app.rtp_sess, buf, sz, &r,
					    &p, payload_size);
	    if (status != PJ_SUCCESS) {
		char errmsg[PJ_ERR_MSG_SIZE];
		pj_strerror(status, errmsg, sizeof(errmsg));
		printf("Not RTP packet, skipping packet: %s\n", errmsg);
		continue;
	    }
	}
#endif

	/* Update RTP session */
	pjmedia_rtp_session_update2(&app.rtp_sess, r, &seq_st, PJ_FALSE);

	/* Skip out-of-order packet */
	if (seq_st.diff == 0) {
	    printf("Skipping out of order packet\n");
	    continue;
	}

	/* Skip if payload type is different */
	if (check_pt && r->pt != app.pt) {
	    printf("Skipping RTP packet with bad payload type\n");
	    continue;
	}

	/* Skip bad packet */
	if (seq_st.status.flag.bad) {
	    printf("Skipping bad RTP\n");
	    continue;
	}


	*rtp = (pjmedia_rtp_hdr*)r;
	*payload = (pj_uint8_t*)p;

	/* We have good packet */
	break;
    }
}

pjmedia_frame play_frm;
static pj_bool_t play_frm_copied, play_frm_ready;

static pj_status_t wait_play(pjmedia_frame *f)
{
    play_frm_copied = PJ_FALSE;
    play_frm = *f;
    play_frm_ready = PJ_TRUE;
    while (!play_frm_copied) {
	pj_thread_sleep(1);
    }
    play_frm_ready = PJ_FALSE;

    return PJ_SUCCESS;
}

static pj_status_t play_cb(void *user_data, pjmedia_frame *f)
{
    PJ_UNUSED_ARG(user_data);

    if (!play_frm_ready) {
	PJ_LOG(3, ("play_cb()", "Warning! Play frame not ready")); 
	return PJ_SUCCESS;
    }

    pj_memcpy(f->buf, play_frm.buf, play_frm.size);
    f->size = play_frm.size;

    play_frm_copied = PJ_TRUE;
    return PJ_SUCCESS;
}

static void pcap2wav(const pj_str_t *codec,
		     const pj_str_t *wav_filename,
		     pjmedia_aud_dev_index dev_id,
		     const pj_str_t *srtp_crypto,
		     const pj_str_t *srtp_key)
{
    const pj_str_t WAV = {".wav", 4};
    struct pkt
    {
	pj_uint8_t	 buffer[320];
	pjmedia_rtp_hdr	*rtp;
	pj_uint8_t	*payload;
	unsigned	 payload_len;
    } pkt0;
    pjmedia_codec_mgr *cmgr;
    const pjmedia_codec_info *ci;
    pjmedia_codec_param param;
    unsigned samples_per_frame;
    pj_status_t status;

    /* Initialize all codecs */
#if PJMEDIA_HAS_SPEEX_CODEC
    T( pjmedia_codec_speex_init(app.mept, 0, 10, 10) );
#endif /* PJMEDIA_HAS_SPEEX_CODEC */

#if PJMEDIA_HAS_ILBC_CODEC
    T( pjmedia_codec_ilbc_init(app.mept, 30) );
#endif /* PJMEDIA_HAS_ILBC_CODEC */

#if PJMEDIA_HAS_GSM_CODEC
    T( pjmedia_codec_gsm_init(app.mept) );
#endif /* PJMEDIA_HAS_GSM_CODEC */

#if PJMEDIA_HAS_G711_CODEC
    T( pjmedia_codec_g711_init(app.mept) );
#endif	/* PJMEDIA_HAS_G711_CODEC */

#if PJMEDIA_HAS_G722_CODEC
    T( pjmedia_codec_g722_init(app.mept) );
#endif	/* PJMEDIA_HAS_G722_CODEC */

#if PJMEDIA_HAS_L16_CODEC
    T( pjmedia_codec_l16_init(app.mept, 0) );
#endif	/* PJMEDIA_HAS_L16_CODEC */

#if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
    T( pjmedia_codec_opencore_amrnb_init(app.mept) );
#endif	/* PJMEDIA_HAS_L16_CODEC */

#if PJMEDIA_HAS_INTEL_IPP
    T( pjmedia_codec_ipp_init(app.mept) );
#endif

    /* Create SRTP transport is needed */
#if PJMEDIA_HAS_SRTP
    if (srtp_crypto->slen) {
	pjmedia_srtp_crypto crypto;

	pj_bzero(&crypto, sizeof(crypto));
	crypto.key = *srtp_key;
	crypto.name = *srtp_crypto;
	T( pjmedia_transport_srtp_create(app.mept, NULL, NULL, &app.srtp) );
	T( pjmedia_transport_srtp_start(app.srtp, &crypto, &crypto) );
    }
#else
    PJ_UNUSED_ARG(srtp_crypto);
    PJ_UNUSED_ARG(srtp_key);
#endif

    /* Read first packet */
    read_rtp(pkt0.buffer, sizeof(pkt0.buffer), &pkt0.rtp, 
	     &pkt0.payload, &pkt0.payload_len, PJ_FALSE);

    cmgr = pjmedia_endpt_get_codec_mgr(app.mept);

    /* Get codec info and param for the specified payload type */
    app.pt = pkt0.rtp->pt;
    if (app.pt >=0 && app.pt < 96) {
	T( pjmedia_codec_mgr_get_codec_info(cmgr, pkt0.rtp->pt, &ci) );
    } else {
	unsigned cnt = 2;
	const pjmedia_codec_info *info[2];
	T( pjmedia_codec_mgr_find_codecs_by_id(cmgr, codec, &cnt, 
					       info, NULL) );
	if (cnt != 1)
	    err_exit("Codec ID must be specified and unique!", 0);

	ci = info[0];
    }
    T( pjmedia_codec_mgr_get_default_param(cmgr, ci, &param) );

    /* Alloc and init codec */
    T( pjmedia_codec_mgr_alloc_codec(cmgr, ci, &app.codec) );
    T( app.codec->op->init(app.codec, app.pool) );
    T( app.codec->op->open(app.codec, &param) );

    /* Init audio device or WAV file */
    samples_per_frame = ci->clock_rate * param.info.frm_ptime / 1000;
    if (pj_strcmp2(wav_filename, "-") == 0) {
	pjmedia_aud_param aud_param;

	/* Open audio device */
	T( pjmedia_aud_dev_default_param(dev_id, &aud_param) );
	aud_param.dir = PJMEDIA_DIR_PLAYBACK;
	aud_param.channel_count = ci->channel_cnt;
	aud_param.clock_rate = ci->clock_rate;
	aud_param.samples_per_frame = samples_per_frame;
	T( pjmedia_aud_stream_create(&aud_param, NULL, &play_cb, 
				     NULL, &app.aud_strm) );
	T( pjmedia_aud_stream_start(app.aud_strm) );
    } else if (pj_stristr(wav_filename, &WAV)) {
	/* Open WAV file */
	T( pjmedia_wav_writer_port_create(app.pool, wav_filename->ptr,
					  ci->clock_rate, ci->channel_cnt,
					  samples_per_frame,
					  param.info.pcm_bits_per_sample, 0, 0,
					  &app.wav) );
    } else {
	err_exit("invalid output file", PJ_EINVAL);
    }

    /* Loop reading PCAP and writing WAV file */
    for (;;) {
	struct pkt pkt1;
	pj_timestamp ts;
	pjmedia_frame frames[16], pcm_frame;
	short pcm[320];
	unsigned i, frame_cnt;
	long samples_cnt, ts_gap;

	pj_assert(sizeof(pcm) >= samples_per_frame);

	/* Parse first packet */
	ts.u64 = 0;
	frame_cnt = PJ_ARRAY_SIZE(frames);
	T( app.codec->op->parse(app.codec, pkt0.payload, pkt0.payload_len, 
				&ts, &frame_cnt, frames) );

	/* Decode and write to WAV file */
	samples_cnt = 0;
	for (i=0; i<frame_cnt; ++i) {
	    pjmedia_frame pcm_frame;

	    pcm_frame.buf = pcm;
	    pcm_frame.size = samples_per_frame * 2;

	    T( app.codec->op->decode(app.codec, &frames[i], pcm_frame.size, 
				     &pcm_frame) );
	    if (app.wav) {
		T( pjmedia_port_put_frame(app.wav, &pcm_frame) );
	    }
	    if (app.aud_strm) {
		T( wait_play(&pcm_frame) );
	    }
	    samples_cnt += samples_per_frame;
	}

	/* Read next packet */
	read_rtp(pkt1.buffer, sizeof(pkt1.buffer), &pkt1.rtp,
		 &pkt1.payload, &pkt1.payload_len, PJ_TRUE);

	/* Fill in the gap (if any) between pkt0 and pkt1 */
	ts_gap = pj_ntohl(pkt1.rtp->ts) - pj_ntohl(pkt0.rtp->ts) -
		 samples_cnt;
	while (ts_gap >= (long)samples_per_frame) {

	    pcm_frame.buf = pcm;
	    pcm_frame.size = samples_per_frame * 2;

	    if (app.codec->op->recover) {
		T( app.codec->op->recover(app.codec, pcm_frame.size, 
					  &pcm_frame) );
	    } else {
		pj_bzero(pcm_frame.buf, pcm_frame.size);
	    }

	    if (app.wav) {
		T( pjmedia_port_put_frame(app.wav, &pcm_frame) );
	    }
	    if (app.aud_strm) {
		T( wait_play(&pcm_frame) );
	    }
	    ts_gap -= samples_per_frame;
	}
	
	/* Next */
	pkt0 = pkt1;
	pkt0.rtp = (pjmedia_rtp_hdr*)pkt0.buffer;
	pkt0.payload = pkt0.buffer + (pkt1.payload - pkt1.buffer);
    }
}


int main(int argc, char *argv[])
{
    pj_str_t input, output, srtp_crypto, srtp_key, codec;
    pjmedia_aud_dev_index dev_id = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;
    pj_pcap_filter filter;
    pj_status_t status;

    enum { 
	OPT_SRC_IP = 1, OPT_DST_IP, OPT_SRC_PORT, OPT_DST_PORT,
	OPT_CODEC, OPT_PLAY_DEV_ID
    };
    struct pj_getopt_option long_options[] = {
	{ "srtp-crypto",    1, 0, 'c' },
	{ "srtp-key",	    1, 0, 'k' },
	{ "src-ip",	    1, 0, OPT_SRC_IP },
	{ "dst-ip",	    1, 0, OPT_DST_IP },
	{ "src-port",	    1, 0, OPT_SRC_PORT },
	{ "dst-port",	    1, 0, OPT_DST_PORT },
	{ "codec",	    1, 0, OPT_CODEC },
	{ "play-dev-id",    1, 0, OPT_PLAY_DEV_ID },
	{ NULL, 0, 0, 0}
    };
    int c;
    int option_index;
    char key_bin[32];

    srtp_crypto.slen = srtp_key.slen = 0;
    codec.slen = 0;

    pj_pcap_filter_default(&filter);
    filter.link = PJ_PCAP_LINK_TYPE_ETH;
    filter.proto = PJ_PCAP_PROTO_TYPE_UDP;

    /* Parse arguments */
    pj_optind = 0;
    while((c=pj_getopt_long(argc,argv, "c:k:", long_options, &option_index))!=-1) {
	switch (c) {
	case 'c':
	    srtp_crypto = pj_str(pj_optarg);
	    break;
	case 'k':
	    {
		int key_len = sizeof(key_bin);
		srtp_key = pj_str(pj_optarg);
		if (pj_base64_decode(&srtp_key, (pj_uint8_t*)key_bin, &key_len)) {
		    puts("Error: invalid key");
		    return 1;
		}
		srtp_key.ptr = key_bin;
		srtp_key.slen = key_len;
	    }
	    break;
	case OPT_SRC_IP:
	    {
		pj_str_t t = pj_str(pj_optarg);
		pj_in_addr a = pj_inet_addr(&t);
		filter.ip_src = a.s_addr;
	    }
	    break;
	case OPT_DST_IP:
	    {
		pj_str_t t = pj_str(pj_optarg);
		pj_in_addr a = pj_inet_addr(&t);
		filter.ip_dst = a.s_addr;
	    }
	    break;
	case OPT_SRC_PORT:
	    filter.src_port = pj_htons((pj_uint16_t)atoi(pj_optarg));
	    break;
	case OPT_DST_PORT:
	    filter.dst_port = pj_htons((pj_uint16_t)atoi(pj_optarg));
	    break;
	case OPT_CODEC:
	    codec = pj_str(pj_optarg);
	    break;
	case OPT_PLAY_DEV_ID:
	    dev_id = atoi(pj_optarg);
	    break;
	default:
	    puts("Error: invalid option");
	    return 1;
	}
    }

    if (pj_optind != argc - 2) {
	puts(USAGE);
	return 1;
    }

    if (!(srtp_crypto.slen) != !(srtp_key.slen)) {
	puts("Error: both SRTP crypto and key must be specified");
	puts(USAGE);
	return 1;
    }

    input = pj_str(argv[pj_optind]);
    output = pj_str(argv[pj_optind+1]);
    
    T( pj_init(0) );

    pj_caching_pool_init(0, &app.cp, NULL, 0);
    app.pool = pj_pool_create(&app.cp.factory, "pcaputil", 1000, 1000, NULL);

    T( pjlib_util_init() );
	//charles modified
    //T( pjmedia_endpt_create(&app.cp.factory, NULL, 0, &app.mept) );
    T( pjmedia_endpt_create(0, &app.cp.factory, NULL, 0, 0, &app.mept) );

    T( pj_pcap_open(app.pool, input.ptr, &app.pcap) );
    T( pj_pcap_set_filter(app.pcap, &filter) );

    pcap2wav(&codec, &output, dev_id, &srtp_crypto, &srtp_key);

    cleanup();
    return 0;
}

