/* $Id */
/* 
 * Copyright (C) 2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2011 Dan Arrhenius <dan@keystream.se>
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

/* 
 * AMR-NB codec implementation with OpenCORE AMRNB library
 */
#include <pjmedia-codec/g722.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/plc.h>
#include <pjmedia/port.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>
#include <pj/math.h>

#if defined(PJMEDIA_HAS_OPENCORE_AMRNB_CODEC) && \
    (PJMEDIA_HAS_OPENCORE_AMRNB_CODEC != 0)

#include <opencore-amrnb/interf_enc.h>
#include <opencore-amrnb/interf_dec.h>
#include <pjmedia-codec/amr_helper.h>
#include <pjmedia-codec/opencore_amrnb.h>

#define THIS_FILE "opencore_amrnb.c"

/* Tracing */
#define PJ_TRACE    0

#if PJ_TRACE
#   define TRACE_(expr)	PJ_LOG(4,expr)
#else
#   define TRACE_(expr)
#endif

/* Use PJMEDIA PLC */
#define USE_PJMEDIA_PLC	    1



/* Prototypes for AMR-NB factory */
static pj_status_t amr_test_alloc(pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *id );
static pj_status_t amr_default_attr(pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr );
static pj_status_t amr_enum_codecs(pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[]);
static pj_status_t amr_alloc_codec(pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id, 
				    pjmedia_codec **p_codec);
static pj_status_t amr_dealloc_codec(pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec );

/* Prototypes for AMR-NB implementation. */
static pj_status_t  amr_codec_init(pjmedia_codec *codec, 
				    pj_pool_t *pool );
static pj_status_t  amr_codec_open(pjmedia_codec *codec, 
				    pjmedia_codec_param *attr );
static pj_status_t  amr_codec_close(pjmedia_codec *codec );
static pj_status_t  amr_codec_modify(pjmedia_codec *codec, 
				      const pjmedia_codec_param *attr );
static pj_status_t  amr_codec_parse(pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[]);
static pj_status_t  amr_codec_encode(pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  amr_codec_decode(pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  amr_codec_recover(pjmedia_codec *codec,
				      unsigned output_buf_len,
				      struct pjmedia_frame *output);



/* Definition for AMR-NB codec operations. */
static pjmedia_codec_op amr_op = 
{
    &amr_codec_init,
    &amr_codec_open,
    &amr_codec_close,
    &amr_codec_modify,
    &amr_codec_parse,
    &amr_codec_encode,
    &amr_codec_decode,
    &amr_codec_recover
};

/* Definition for AMR-NB codec factory operations. */
static pjmedia_codec_factory_op amr_factory_op =
{
    &amr_test_alloc,
    &amr_default_attr,
    &amr_enum_codecs,
    &amr_alloc_codec,
    &amr_dealloc_codec
};


/* AMR-NB factory */
static struct amr_codec_factory
{
    pjmedia_codec_factory    base;
    pjmedia_endpt	    *endpt;
    pj_pool_t		    *pool;
} amr_codec_factory;


/* AMR-NB codec private data. */
struct amr_data
{
    pj_pool_t		*pool;
    void		*encoder;
    void		*decoder;
    pj_bool_t		 plc_enabled;
    pj_bool_t		 vad_enabled;
    int			 enc_mode;
    pjmedia_codec_amr_pack_setting enc_setting;
    pjmedia_codec_amr_pack_setting dec_setting;
#if USE_PJMEDIA_PLC
    pjmedia_plc		*plc;
#endif
    pj_timestamp	 last_tx;
};

static pjmedia_codec_amrnb_config def_config =
{
    PJ_FALSE,	    /* octet align	*/
    5900	    /* bitrate		*/
};



/*
 * Initialize and register AMR-NB codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_opencore_amrnb_init( pjmedia_endpt *endpt )
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (amr_codec_factory.pool != NULL)
	return PJ_SUCCESS;

    /* Create AMR-NB codec factory. */
    amr_codec_factory.base.op = &amr_factory_op;
    amr_codec_factory.base.factory_data = NULL;
    amr_codec_factory.endpt = endpt;

    amr_codec_factory.pool = pjmedia_endpt_create_pool(endpt, "amrnb", 1000, 
						       1000);
    if (!amr_codec_factory.pool)
	return PJ_ENOMEM;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	status = PJ_EINVALIDOP;
	goto on_error;
    }

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr, 
						&amr_codec_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(amr_codec_factory.pool);
    amr_codec_factory.pool = NULL;
    return status;
}


/*
 * Unregister AMR-NB codec factory from pjmedia endpoint and deinitialize
 * the AMR-NB codec library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_opencore_amrnb_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (amr_codec_factory.pool == NULL)
	return PJ_SUCCESS;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(amr_codec_factory.endpt);
    if (!codec_mgr) {
	pj_pool_release(amr_codec_factory.pool);
	amr_codec_factory.pool = NULL;
	return PJ_EINVALIDOP;
    }

    /* Unregister AMR-NB codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &amr_codec_factory.base);
    
    /* Destroy pool. */
    pj_pool_release(amr_codec_factory.pool);
    amr_codec_factory.pool = NULL;
    
    return status;
}


PJ_DEF(pj_status_t) pjmedia_codec_opencore_amrnb_set_config(
			    const pjmedia_codec_amrnb_config *config)
{
    unsigned nbitrates;


    def_config = *config;

    /* Normalize bitrate. */
    nbitrates = PJ_ARRAY_SIZE(pjmedia_codec_amrnb_bitrates);
    if (def_config.bitrate < pjmedia_codec_amrnb_bitrates[0])
	def_config.bitrate = pjmedia_codec_amrnb_bitrates[0];
    else if (def_config.bitrate > pjmedia_codec_amrnb_bitrates[nbitrates-1])
	def_config.bitrate = pjmedia_codec_amrnb_bitrates[nbitrates-1];
    else
    {
	unsigned i;
	
	for (i = 0; i < nbitrates; ++i) {
	    if (def_config.bitrate <= pjmedia_codec_amrnb_bitrates[i])
		break;
	}
	def_config.bitrate = pjmedia_codec_amrnb_bitrates[i];
    }

    return PJ_SUCCESS;
}

/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t amr_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *info )
{
    PJ_UNUSED_ARG(factory);

    /* Check payload type. */
    if (info->pt != PJMEDIA_RTP_PT_AMR)
	return PJMEDIA_CODEC_EUNSUP;

    /* Ignore the rest, since it's static payload type. */

    return PJ_SUCCESS;
}

/*
 * Generate default attribute.
 */
static pj_status_t amr_default_attr( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr )
{
    PJ_UNUSED_ARG(factory);
    PJ_UNUSED_ARG(id);

    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.clock_rate = 8000;
    attr->info.channel_cnt = 1;
    attr->info.avg_bps = def_config.bitrate;
    attr->info.max_bps = pjmedia_codec_amrnb_bitrates[7];
    attr->info.pcm_bits_per_sample = 16;
    attr->info.frm_ptime = 20;
    attr->info.pt = PJMEDIA_RTP_PT_AMR;

    attr->setting.frm_per_pkt = 2;
    attr->setting.vad = 1;
    attr->setting.plc = 1;

    if (def_config.octet_align) {
	attr->setting.dec_fmtp.cnt = 1;
	attr->setting.dec_fmtp.param[0].name = pj_str("octet-align");
	attr->setting.dec_fmtp.param[0].val = pj_str("1");
    }

    /* Default all other flag bits disabled. */

    return PJ_SUCCESS;
}


/*
 * Enum codecs supported by this factory (i.e. only AMR-NB!).
 */
static pj_status_t amr_enum_codecs( pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[])
{
    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    pj_bzero(&codecs[0], sizeof(pjmedia_codec_info));
    codecs[0].encoding_name = pj_str("AMR");
    codecs[0].pt = PJMEDIA_RTP_PT_AMR;
    codecs[0].type = PJMEDIA_TYPE_AUDIO;
    codecs[0].clock_rate = 8000;
    codecs[0].channel_cnt = 1;

    *count = 1;

    return PJ_SUCCESS;
}


/*
 * Allocate a new AMR-NB codec instance.
 */
static pj_status_t amr_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id,
				    pjmedia_codec **p_codec)
{
    pj_pool_t *pool;
    pjmedia_codec *codec;
    struct amr_data *amr_data;
    pj_status_t status;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &amr_codec_factory.base, PJ_EINVAL);

    pool = pjmedia_endpt_create_pool(amr_codec_factory.endpt, "amrnb-inst", 
				     512, 512);

    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);
    PJ_ASSERT_RETURN(codec != NULL, PJ_ENOMEM);
    codec->op = &amr_op;
    codec->factory = factory;

    amr_data = PJ_POOL_ZALLOC_T(pool, struct amr_data);
    codec->codec_data = amr_data;
    amr_data->pool = pool;

#if USE_PJMEDIA_PLC
    /* Create PLC */
    status = pjmedia_plc_create(pool, 8000, 160, 0, &amr_data->plc);
    if (status != PJ_SUCCESS) {
	return status;
    }
#else
    PJ_UNUSED_ARG(status);
#endif
    *p_codec = codec;
    return PJ_SUCCESS;
}


/*
 * Free codec.
 */
static pj_status_t amr_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec )
{
    struct amr_data *amr_data;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &amr_codec_factory.base, PJ_EINVAL);

    amr_data = (struct amr_data*) codec->codec_data;

    /* Close codec, if it's not closed. */
    amr_codec_close(codec);

    pj_pool_release(amr_data->pool);
    amr_data = NULL;

    return PJ_SUCCESS;
}

/*
 * Init codec.
 */
static pj_status_t amr_codec_init( pjmedia_codec *codec, 
				   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}


/*
 * Open codec.
 */
static pj_status_t amr_codec_open( pjmedia_codec *codec, 
				   pjmedia_codec_param *attr )
{
    struct amr_data *amr_data = (struct amr_data*) codec->codec_data;
    pjmedia_codec_amr_pack_setting *setting;
    unsigned i;
    pj_uint8_t octet_align = 0;
    pj_int8_t enc_mode;
    const pj_str_t STR_FMTP_OCTET_ALIGN = {"octet-align", 11};

    PJ_ASSERT_RETURN(codec && attr, PJ_EINVAL);
    PJ_ASSERT_RETURN(amr_data != NULL, PJ_EINVALIDOP);

    enc_mode = pjmedia_codec_amr_get_mode(attr->info.avg_bps);
    pj_assert(enc_mode >= 0 && enc_mode <= 7);

    /* Check octet-align */
    for (i = 0; i < attr->setting.dec_fmtp.cnt; ++i) {
	if (pj_stricmp(&attr->setting.dec_fmtp.param[i].name, 
		       &STR_FMTP_OCTET_ALIGN) == 0)
	{
	    octet_align = (pj_uint8_t)
			  (pj_strtoul(&attr->setting.dec_fmtp.param[i].val));
	    break;
	}
    }

    /* Check mode-set */
    for (i = 0; i < attr->setting.enc_fmtp.cnt; ++i) {
	const pj_str_t STR_FMTP_MODE_SET = {"mode-set", 8};
        
	if (pj_stricmp(&attr->setting.enc_fmtp.param[i].name, 
		       &STR_FMTP_MODE_SET) == 0)
	{
	    const char *p;
	    pj_size_t l;
	    pj_int8_t diff = 99;

	    /* Encoding mode is chosen based on local default mode setting:
	     * - if local default mode is included in the mode-set, use it
	     * - otherwise, find the closest mode to local default mode;
	     *   if there are two closest modes, prefer to use the higher
	     *   one, e.g: local default mode is 4, the mode-set param
	     *   contains '2,3,5,6', then 5 will be chosen.
	     */
	    p = pj_strbuf(&attr->setting.enc_fmtp.param[i].val);
	    l = pj_strlen(&attr->setting.enc_fmtp.param[i].val);
	    while (l--) {
		if (*p>='0' && *p<='7') {
		    pj_int8_t tmp = *p - '0' - enc_mode;

		    if (PJ_ABS(diff) > PJ_ABS(tmp) || 
			(PJ_ABS(diff) == PJ_ABS(tmp) && tmp > diff))
		    {
			diff = tmp;
			if (diff == 0) break;
		    }
		}
		++p;
	    }
	    PJ_ASSERT_RETURN(diff != 99, PJMEDIA_CODEC_EFAILED);

	    enc_mode = enc_mode + diff;

	    break;
	}
    }

    amr_data->vad_enabled = (attr->setting.vad != 0);
    amr_data->plc_enabled = (attr->setting.plc != 0);
    amr_data->enc_mode = enc_mode;

    amr_data->encoder = Encoder_Interface_init(amr_data->vad_enabled);
    if (amr_data->encoder == NULL) {
	TRACE_((THIS_FILE, "Encoder_Interface_init() failed"));
	amr_codec_close(codec);
	return PJMEDIA_CODEC_EFAILED;
    }
    setting = &amr_data->enc_setting;
    pj_bzero(setting, sizeof(pjmedia_codec_amr_pack_setting));
    setting->amr_nb = 1;
    setting->reorder = 0;
    setting->octet_aligned = octet_align;
    setting->cmr = 15;

    amr_data->decoder = Decoder_Interface_init();
    if (amr_data->decoder == NULL) {
	TRACE_((THIS_FILE, "Decoder_Interface_init() failed"));
	amr_codec_close(codec);
	return PJMEDIA_CODEC_EFAILED;
    }
    setting = &amr_data->dec_setting;
    pj_bzero(setting, sizeof(pjmedia_codec_amr_pack_setting));
    setting->amr_nb = 1;
    setting->reorder = 0;
    setting->octet_aligned = octet_align;

    TRACE_((THIS_FILE, "AMR-NB codec allocated: vad=%d, plc=%d, bitrate=%d",
			amr_data->vad_enabled, amr_data->plc_enabled, 
			pjmedia_codec_amrnb_bitrates[amr_data->enc_mode]));
    return PJ_SUCCESS;
}


/*
 * Close codec.
 */
static pj_status_t amr_codec_close( pjmedia_codec *codec )
{
    struct amr_data *amr_data;

    PJ_ASSERT_RETURN(codec, PJ_EINVAL);

    amr_data = (struct amr_data*) codec->codec_data;
    PJ_ASSERT_RETURN(amr_data != NULL, PJ_EINVALIDOP);

    if (amr_data->encoder) {
        Encoder_Interface_exit(amr_data->encoder);
        amr_data->encoder = NULL;
    }

    if (amr_data->decoder) {
        Decoder_Interface_exit(amr_data->decoder);
        amr_data->decoder = NULL;
    }
    
    TRACE_((THIS_FILE, "AMR-NB codec closed"));
    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t amr_codec_modify( pjmedia_codec *codec, 
				     const pjmedia_codec_param *attr )
{
    struct amr_data *amr_data = (struct amr_data*) codec->codec_data;
    pj_bool_t prev_vad_state;

    pj_assert(amr_data != NULL);
    pj_assert(amr_data->encoder != NULL && amr_data->decoder != NULL);

    prev_vad_state = amr_data->vad_enabled;
    amr_data->vad_enabled = (attr->setting.vad != 0);
    amr_data->plc_enabled = (attr->setting.plc != 0);

    if (prev_vad_state != amr_data->vad_enabled) {
	/* Reinit AMR encoder to update VAD setting */
	TRACE_((THIS_FILE, "Reiniting AMR encoder to update VAD setting."));
        Encoder_Interface_exit(amr_data->encoder);
        amr_data->encoder = Encoder_Interface_init(amr_data->vad_enabled);
        if (amr_data->encoder == NULL) {
	    TRACE_((THIS_FILE, "Encoder_Interface_init() failed"));
	    amr_codec_close(codec);
	    return PJMEDIA_CODEC_EFAILED;
	}
    }

    TRACE_((THIS_FILE, "AMR-NB codec modified: vad=%d, plc=%d",
			amr_data->vad_enabled, amr_data->plc_enabled));
    return PJ_SUCCESS;
}


/*
 * Get frames in the packet.
 */
static pj_status_t amr_codec_parse( pjmedia_codec *codec,
				    void *pkt,
				    pj_size_t pkt_size,
				    const pj_timestamp *ts,
				    unsigned *frame_cnt,
				    pjmedia_frame frames[])
{
    struct amr_data *amr_data = (struct amr_data*) codec->codec_data;
    pj_uint8_t cmr;
    pj_status_t status;

    status = pjmedia_codec_amr_parse(pkt, pkt_size, ts, &amr_data->dec_setting,
				     frames, frame_cnt, &cmr);
    if (status != PJ_SUCCESS)
	return status;

    /* Check for Change Mode Request. */
    if (cmr <= 7 && amr_data->enc_mode != cmr) {
	amr_data->enc_mode = cmr;
	TRACE_((THIS_FILE, "AMR-NB encoder switched mode to %d (%dbps)",
			    amr_data->enc_mode, 
			    pjmedia_codec_amrnb_bitrates[amr_data->enc_mode]));
    }

    return PJ_SUCCESS;
}


/*
 * Encode frame.
 */
static pj_status_t amr_codec_encode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct amr_data *amr_data = (struct amr_data*) codec->codec_data;
    unsigned char *bitstream;
    pj_int16_t *speech;
    unsigned nsamples, samples_per_frame;
    enum {MAX_FRAMES_PER_PACKET = 16};
    pjmedia_frame frames[MAX_FRAMES_PER_PACKET];
    pj_uint8_t *p;
    unsigned i, out_size = 0, nframes = 0;
    pj_size_t payload_len;
    unsigned dtx_cnt, sid_cnt;
    pj_status_t status;
    int size;

    pj_assert(amr_data != NULL);
    PJ_ASSERT_RETURN(input && output, PJ_EINVAL);

    nsamples = input->size >> 1;
    samples_per_frame = 160;
    PJ_ASSERT_RETURN(nsamples % samples_per_frame == 0, 
		     PJMEDIA_CODEC_EPCMFRMINLEN);

    nframes = nsamples / samples_per_frame;
    PJ_ASSERT_RETURN(nframes <= MAX_FRAMES_PER_PACKET, 
		     PJMEDIA_CODEC_EFRMTOOSHORT);

    /* Encode the frames */
    speech = (pj_int16_t*)input->buf;
    bitstream = (unsigned char*)output->buf;
    while (nsamples >= samples_per_frame) {
        size = Encoder_Interface_Encode (amr_data->encoder, amr_data->enc_mode,
                                         speech, bitstream, 0);
	if (size == 0) {
	    output->size = 0;
	    output->buf = NULL;
	    output->type = PJMEDIA_FRAME_TYPE_NONE;
	    TRACE_((THIS_FILE, "AMR-NB encode() failed"));
	    return PJMEDIA_CODEC_EFAILED;
	}
	nsamples -= 160;
	speech += samples_per_frame;
	bitstream += size;
	out_size += size;
	TRACE_((THIS_FILE, "AMR-NB encode(): mode=%d, size=%d",
		amr_data->enc_mode, out_size));
    }

    /* Pack payload */
    p = (pj_uint8_t*)output->buf + output_buf_len - out_size;
    pj_memmove(p, output->buf, out_size);
    dtx_cnt = sid_cnt = 0;
    for (i = 0; i < nframes; ++i) {
	pjmedia_codec_amr_bit_info *info = (pjmedia_codec_amr_bit_info*)
					   &frames[i].bit_info;
	info->frame_type = (pj_uint8_t)((*p >> 3) & 0x0F);
	info->good_quality = (pj_uint8_t)((*p >> 2) & 0x01);
	info->mode = (pj_int8_t)amr_data->enc_mode;
	info->start_bit = 0;
	frames[i].buf = p + 1;
	frames[i].size = (info->frame_type <= 8)? 
			 pjmedia_codec_amrnb_framelen[info->frame_type] : 0;
	p += frames[i].size + 1;

	/* Count the number of SID and DTX frames */
	if (info->frame_type == 15) /* DTX*/
	    ++dtx_cnt;
	else if (info->frame_type == 8) /* SID */
	    ++sid_cnt;
    }

    /* VA generates DTX frames as DTX+SID frames switching quickly and it
     * seems that the SID frames occur too often (assuming the purpose is 
     * only for keeping NAT alive?). So let's modify the behavior a bit.
     * Only an SID frame will be sent every PJMEDIA_CODEC_MAX_SILENCE_PERIOD
     * milliseconds.
     */
    if (sid_cnt + dtx_cnt == nframes) {
	pj_int32_t dtx_duration;

	dtx_duration = pj_timestamp_diff32(&amr_data->last_tx, 
					   &input->timestamp);
	if (PJMEDIA_CODEC_MAX_SILENCE_PERIOD == -1 ||
	    dtx_duration < PJMEDIA_CODEC_MAX_SILENCE_PERIOD*8000/1000) 
	{
	    output->size = 0;
	    output->type = PJMEDIA_FRAME_TYPE_NONE;
	    output->timestamp = input->timestamp;
	    return PJ_SUCCESS;
	}
    }

    payload_len = output_buf_len;

    status = pjmedia_codec_amr_pack(frames, nframes, &amr_data->enc_setting,
				    output->buf, &payload_len);
    if (status != PJ_SUCCESS) {
	output->size = 0;
	output->buf = NULL;
	output->type = PJMEDIA_FRAME_TYPE_NONE;
	TRACE_((THIS_FILE, "Failed to pack AMR payload, status=%d", status));
	return status;
    }

    output->size = payload_len;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    amr_data->last_tx = input->timestamp;

    return PJ_SUCCESS;
}


/*
 * Decode frame.
 */
static pj_status_t amr_codec_decode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct amr_data *amr_data = (struct amr_data*) codec->codec_data;
    pjmedia_frame input_;
    pjmedia_codec_amr_bit_info *info;
    /* VA AMR-NB decoding buffer: AMR-NB max frame size + 1 byte header. */
    unsigned char bitstream[32];

    pj_assert(amr_data != NULL);
    PJ_ASSERT_RETURN(input && output, PJ_EINVAL);

    if (output_buf_len < 320)
	return PJMEDIA_CODEC_EPCMTOOSHORT;

    input_.buf = &bitstream[1];
    input_.size = 31; /* AMR-NB max frame size */
    pjmedia_codec_amr_predecode(input, &amr_data->dec_setting, &input_);
    info = (pjmedia_codec_amr_bit_info*)&input_.bit_info;

    /* VA AMRNB decoder requires frame info in the first byte. */
    bitstream[0] = (info->frame_type << 3) | (info->good_quality << 2);

    TRACE_((THIS_FILE, "AMR-NB decode(): mode=%d, ft=%d, size=%d",
	    info->mode, info->frame_type, input_.size));

    /* Decode */
    Decoder_Interface_Decode(amr_data->decoder, bitstream,
                             (pj_int16_t*)output->buf, 0);

    output->size = 320;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

#if USE_PJMEDIA_PLC
    if (amr_data->plc_enabled)
	pjmedia_plc_save(amr_data->plc, (pj_int16_t*)output->buf);
#endif

    return PJ_SUCCESS;
}


/*
 * Recover lost frame.
 */
#if USE_PJMEDIA_PLC
/*
 * Recover lost frame.
 */
static pj_status_t  amr_codec_recover( pjmedia_codec *codec,
				       unsigned output_buf_len,
				       struct pjmedia_frame *output)
{
    struct amr_data *amr_data = codec->codec_data;

    TRACE_((THIS_FILE, "amr_codec_recover"));

    PJ_ASSERT_RETURN(amr_data->plc_enabled, PJ_EINVALIDOP);

    PJ_ASSERT_RETURN(output_buf_len >= 320,  PJMEDIA_CODEC_EPCMTOOSHORT);

    pjmedia_plc_generate(amr_data->plc, (pj_int16_t*)output->buf);

    output->size = 320;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    
    return PJ_SUCCESS;
}
#endif

#if defined(_MSC_VER) && PJMEDIA_AUTO_LINK_OPENCORE_AMR_LIBS
#  if PJMEDIA_OPENCORE_AMR_BUILT_WITH_GCC
#   pragma comment( lib, "libopencore-amrnb.a")
#  else
#   error Unsupported OpenCORE AMR library, fix here
#  endif
#endif

#endif
