/* $Id: symb_vas_dev.cpp 3809 2011-10-11 03:05:34Z nanang $ */
/* 
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia-audiodev/audiodev_imp.h>
#include <pjmedia-audiodev/errno.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/resample.h>
#include <pjmedia/stereo.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/os.h>
#include <pj/string.h>

#if PJMEDIA_AUDIO_DEV_HAS_SYMB_VAS

/* VAS headers */
#include <VoIPUtilityFactory.h>
#include <VoIPDownlinkStream.h>
#include <VoIPUplinkStream.h>
#include <VoIPFormatIntfc.h>
#include <VoIPG711DecoderIntfc.h>
#include <VoIPG711EncoderIntfc.h>
#include <VoIPG729DecoderIntfc.h>
#include <VoIPILBCDecoderIntfc.h>
#include <VoIPILBCEncoderIntfc.h>

/* AMR helper */  
#include <pjmedia-codec/amr_helper.h>

/* Pack/unpack G.729 frame of S60 DSP codec, taken from:  
 * http://wiki.forum.nokia.com/index.php/TSS000776_-_Payload_conversion_for_G.729_audio_format
 */
#include "s60_g729_bitstream.h"


#define THIS_FILE			"symb_vas_dev.c"
#define BITS_PER_SAMPLE			16


/* When this macro is set, VAS will use EPCM16 format for PCM input/output,
 * otherwise VAS will use EG711 then transcode it to PCM.
 * Note that using native EPCM16 format may introduce (much) delay.
 */
//#define USE_NATIVE_PCM

#if 1
#   define TRACE_(st) PJ_LOG(3, st)
#else
#   define TRACE_(st)
#endif

/* VAS G.711 frame length */
static pj_uint8_t vas_g711_frame_len;


/* VAS factory */
struct vas_factory
{
    pjmedia_aud_dev_factory	 base;
    pj_pool_t			*pool;
    pj_pool_factory		*pf;
    pjmedia_aud_dev_info	 dev_info;
};


/* Forward declaration of CPjAudioEngine */
class CPjAudioEngine;


/* VAS stream. */
struct vas_stream
{
    // Base
    pjmedia_aud_stream	 base;			/**< Base class.	*/
    
    // Pool
    pj_pool_t		*pool;			/**< Memory pool.       */

    // Common settings.
    pjmedia_aud_param 	 param;			/**< Stream param.	*/
    pjmedia_aud_rec_cb   rec_cb;		/**< Record callback.  	*/
    pjmedia_aud_play_cb	 play_cb;		/**< Playback callback. */
    void                *user_data;		/**< Application data.  */

    // Audio engine
    CPjAudioEngine	*engine;		/**< Internal engine.	*/

    pj_timestamp  	 ts_play;		/**< Playback timestamp.*/
    pj_timestamp	 ts_rec;		/**< Record timestamp.	*/

    pj_int16_t		*play_buf;		/**< Playback buffer.	*/
    pj_uint16_t		 play_buf_len;		/**< Playback buffer length. */
    pj_uint16_t		 play_buf_start;	/**< Playback buffer start index. */
    pj_int16_t		*rec_buf;		/**< Record buffer.	*/
    pj_uint16_t		 rec_buf_len;		/**< Record buffer length. */
    void                *strm_data;		/**< Stream data.	*/

    /* Resampling is needed, in case audio device is opened with clock rate 
     * other than 8kHz (only for PCM format).
     */
    pjmedia_resample	*play_resample;		/**< Resampler for playback. */
    pjmedia_resample	*rec_resample;		/**< Resampler for recording */
    pj_uint16_t		 resample_factor;	/**< Resample factor, requested
						     clock rate / 8000	     */

    /* When stream is working in PCM format, where the samples may need to be
     * resampled from/to different clock rate and/or channel count, PCM buffer
     * is needed to perform such resampling operations.
     */
    pj_int16_t		*pcm_buf;		/**< PCM buffer.	     */
};


/* Prototypes */
static pj_status_t factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t factory_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t factory_refresh(pjmedia_aud_dev_factory *f);
static unsigned    factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t factory_get_dev_info(pjmedia_aud_dev_factory *f, 
					unsigned index,
					pjmedia_aud_dev_info *info);
static pj_status_t factory_default_param(pjmedia_aud_dev_factory *f,
					 unsigned index,
					 pjmedia_aud_param *param);
static pj_status_t factory_create_stream(pjmedia_aud_dev_factory *f,
					 const pjmedia_aud_param *param,
					 pjmedia_aud_rec_cb rec_cb,
					 pjmedia_aud_play_cb play_cb,
					 void *user_data,
					 pjmedia_aud_stream **p_aud_strm);

static pj_status_t stream_get_param(pjmedia_aud_stream *strm,
				    pjmedia_aud_param *param);
static pj_status_t stream_get_cap(pjmedia_aud_stream *strm,
				  pjmedia_aud_dev_cap cap,
				  void *value);
static pj_status_t stream_set_cap(pjmedia_aud_stream *strm,
				  pjmedia_aud_dev_cap cap,
				  const void *value);
static pj_status_t stream_start(pjmedia_aud_stream *strm);
static pj_status_t stream_stop(pjmedia_aud_stream *strm);
static pj_status_t stream_destroy(pjmedia_aud_stream *strm);


/* Operations */
static pjmedia_aud_dev_factory_op factory_op =
{
    &factory_init,
    &factory_destroy,
    &factory_get_dev_count,
    &factory_get_dev_info,
    &factory_default_param,
    &factory_create_stream,
    &factory_refresh
};

static pjmedia_aud_stream_op stream_op = 
{
    &stream_get_param,
    &stream_get_cap,
    &stream_set_cap,
    &stream_start,
    &stream_stop,
    &stream_destroy
};


/****************************************************************************
 * Internal VAS Engine
 */

/*
 * Utility: print sound device error
 */
static void snd_perror(const char *title, TInt rc)
{
    PJ_LOG(1,(THIS_FILE, "%s (error code=%d)", title, rc));
}

typedef void(*PjAudioCallback)(CVoIPDataBuffer *buf, void *user_data);

/*
 * Audio setting for CPjAudioEngine.
 */
class CPjAudioSetting
{
public:
    TVoIPCodecFormat	 format;
    TInt		 mode;
    TBool		 plc;
    TBool		 vad;
    TBool		 cng;
    TBool		 loudspk;
};

/*
 * Implementation: Symbian Input & Output Stream.
 */
class CPjAudioEngine :  public CBase,
			public MVoIPDownlinkObserver,
			public MVoIPUplinkObserver,
			public MVoIPFormatObserver
{
public:
    enum State
    {
	STATE_NULL,
	STATE_STARTING,
	STATE_READY,
	STATE_STREAMING
    };

    ~CPjAudioEngine();

    static CPjAudioEngine *NewL(struct vas_stream *parent_strm,
			        PjAudioCallback rec_cb,
				PjAudioCallback play_cb,
				void *user_data,
				const CPjAudioSetting &setting);

    TInt Start();
    void Stop();

    TInt ActivateSpeaker(TBool active);
    
    TInt SetVolume(TInt vol) { return iVoIPDnlink->SetVolume(vol); }
    TInt GetVolume() { TInt vol;iVoIPDnlink->GetVolume(vol);return vol; }
    TInt GetMaxVolume() { TInt vol;iVoIPDnlink->GetMaxVolume(vol);return vol; }
    
    TInt SetGain(TInt gain) { return iVoIPUplink->SetGain(gain); }
    TInt GetGain() { TInt gain;iVoIPUplink->GetGain(gain);return gain; }
    TInt GetMaxGain() { TInt gain;iVoIPUplink->GetMaxGain(gain);return gain; }

    TBool IsStarted();
    
private:
    CPjAudioEngine(struct vas_stream *parent_strm,
		   PjAudioCallback rec_cb,
		   PjAudioCallback play_cb,
		   void *user_data,
		   const CPjAudioSetting &setting);
    void ConstructL();

    TInt InitPlay();
    TInt InitRec();

    TInt StartPlay();
    TInt StartRec();

    // From MVoIPDownlinkObserver
    void FillBuffer(const CVoIPAudioDownlinkStream& aSrc,
                            CVoIPDataBuffer* aBuffer);
    void Event(const CVoIPAudioDownlinkStream& aSrc,
                       TInt aEventType,
                       TInt aError);

    // From MVoIPUplinkObserver
    void EmptyBuffer(const CVoIPAudioUplinkStream& aSrc,
                             CVoIPDataBuffer* aBuffer);
    void Event(const CVoIPAudioUplinkStream& aSrc,
                       TInt aEventType,
                       TInt aError);

    // From MVoIPFormatObserver
    void Event(const CVoIPFormatIntfc& aSrc, TInt aEventType);

    State			 dn_state_;
    State			 up_state_;
    struct vas_stream		*parentStrm_;
    CPjAudioSetting		 setting_;
    PjAudioCallback 		 rec_cb_;
    PjAudioCallback 		 play_cb_;
    void 			*user_data_;

    // VAS objects
    CVoIPUtilityFactory         *iFactory;
    CVoIPAudioDownlinkStream    *iVoIPDnlink;
    CVoIPAudioUplinkStream      *iVoIPUplink;
    CVoIPFormatIntfc		*enc_fmt_if;
    CVoIPFormatIntfc		*dec_fmt_if;
};


CPjAudioEngine* CPjAudioEngine::NewL(struct vas_stream *parent_strm,
				     PjAudioCallback rec_cb,
				     PjAudioCallback play_cb,
				     void *user_data,
				     const CPjAudioSetting &setting)
{
    CPjAudioEngine* self = new (ELeave) CPjAudioEngine(parent_strm,
						       rec_cb, play_cb,
						       user_data,
						       setting);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
}

void CPjAudioEngine::ConstructL()
{
    TInt err;
    const TVersion ver(1, 0, 0); /* Not really used at this time */

    err = CVoIPUtilityFactory::CreateFactory(iFactory);
    User::LeaveIfError(err);

    if (parentStrm_->param.dir != PJMEDIA_DIR_CAPTURE) {
	err = iFactory->CreateDownlinkStream(ver, 
					     CVoIPUtilityFactory::EVoIPCall,
					     iVoIPDnlink);
	User::LeaveIfError(err);
    }

    if (parentStrm_->param.dir != PJMEDIA_DIR_PLAYBACK) {
	err = iFactory->CreateUplinkStream(ver, 
					   CVoIPUtilityFactory::EVoIPCall,
					   iVoIPUplink);
	User::LeaveIfError(err);
    }
}

CPjAudioEngine::CPjAudioEngine(struct vas_stream *parent_strm,
			       PjAudioCallback rec_cb,
			       PjAudioCallback play_cb,
			       void *user_data,
			       const CPjAudioSetting &setting)
      : dn_state_(STATE_NULL),
        up_state_(STATE_NULL),
	parentStrm_(parent_strm),
	setting_(setting),
        rec_cb_(rec_cb),
        play_cb_(play_cb),
        user_data_(user_data),
        iFactory(NULL),
        iVoIPDnlink(NULL),
        iVoIPUplink(NULL),
        enc_fmt_if(NULL),
        dec_fmt_if(NULL)
{
}

CPjAudioEngine::~CPjAudioEngine()
{
    Stop();
    
    if (iVoIPUplink)
	iVoIPUplink->Close();
    
    if (iVoIPDnlink)
	iVoIPDnlink->Close();

    delete enc_fmt_if;
    delete dec_fmt_if;
    delete iVoIPDnlink;
    delete iVoIPUplink;
    delete iFactory;
    
    TRACE_((THIS_FILE, "Sound device destroyed"));
}

TBool CPjAudioEngine::IsStarted()
{
    return ((((parentStrm_->param.dir & PJMEDIA_DIR_CAPTURE) == 0) || 
	       up_state_ == STATE_STREAMING) &&
	    (((parentStrm_->param.dir & PJMEDIA_DIR_PLAYBACK) == 0) || 
	       dn_state_ == STATE_STREAMING));
}

TInt CPjAudioEngine::InitPlay()
{
    TInt err;

    pj_assert(iVoIPDnlink);

    delete dec_fmt_if;
    dec_fmt_if = NULL;
    err = iVoIPDnlink->SetFormat(setting_.format, dec_fmt_if);
    if (err != KErrNone)
	return err;
    
    err = dec_fmt_if->SetObserver(*this);
    if (err != KErrNone)
	return err;

    return iVoIPDnlink->Open(*this);
}

TInt CPjAudioEngine::InitRec()
{
    TInt err;
    
    pj_assert(iVoIPUplink);

    delete enc_fmt_if;
    enc_fmt_if = NULL;
    err = iVoIPUplink->SetFormat(setting_.format, enc_fmt_if);
    if (err != KErrNone)
	return err;
    
    err = enc_fmt_if->SetObserver(*this);
    if (err != KErrNone)
	return err;
    
    return iVoIPUplink->Open(*this);
}

TInt CPjAudioEngine::StartPlay()
{
    TInt err = KErrNone;
    
    pj_assert(iVoIPDnlink);
    pj_assert(dn_state_ == STATE_READY);

    /* Configure specific codec setting */
    switch (setting_.format) {
    case EG711:
	{
	    CVoIPG711DecoderIntfc *g711dec_if = (CVoIPG711DecoderIntfc*)
						dec_fmt_if;
	    err = g711dec_if->SetMode((CVoIPFormatIntfc::TG711CodecMode)
				      setting_.mode);
	}
	break;
	
    case EILBC:
	{
	    CVoIPILBCDecoderIntfc *ilbcdec_if = (CVoIPILBCDecoderIntfc*)
						dec_fmt_if;
	    err = ilbcdec_if->SetMode((CVoIPFormatIntfc::TILBCCodecMode)
				      setting_.mode);
	}
	break;

    case EAMR_NB:
	/* Ticket #1008: AMR playback issue on few devices, e.g: E72, E52 */
	err = dec_fmt_if->SetFrameMode(ETrue);
	break;
	
    default:
	break;
    }

    if (err != KErrNone)
	goto on_return;
    
    /* Configure audio routing */
    ActivateSpeaker(setting_.loudspk);

    /* Start player */
    err = iVoIPDnlink->Start();

on_return:
    if (err == KErrNone) {
	dn_state_ = STATE_STREAMING;
	TRACE_((THIS_FILE, "Downlink started"));
    } else {
	snd_perror("Failed starting downlink", err);
    }

    return err;
}

TInt CPjAudioEngine::StartRec()
{
    TInt err = KErrNone;
    
    pj_assert(iVoIPUplink);
    pj_assert(up_state_ == STATE_READY);

    /* Configure specific codec setting */
    switch (setting_.format) {
    case EG711:
	{
	    CVoIPG711EncoderIntfc *g711enc_if = (CVoIPG711EncoderIntfc*)
						enc_fmt_if;
	    err = g711enc_if->SetMode((CVoIPFormatIntfc::TG711CodecMode)
				      setting_.mode);
	}
	break;

    case EILBC:
	{
	    CVoIPILBCEncoderIntfc *ilbcenc_if = (CVoIPILBCEncoderIntfc*)
						enc_fmt_if;
	    err = ilbcenc_if->SetMode((CVoIPFormatIntfc::TILBCCodecMode)
				      setting_.mode);
	}
	break;
	
    case EAMR_NB:
	err = enc_fmt_if->SetBitRate(setting_.mode);
	break;
	
    default:
	break;
    }
    
    if (err != KErrNone)
	goto on_return;
    
    /* Configure general codec setting */
    enc_fmt_if->SetVAD(setting_.vad);
    
    /* Start recorder */
    err = iVoIPUplink->Start();

on_return:
    if (err == KErrNone) {
	up_state_ = STATE_STREAMING;
	TRACE_((THIS_FILE, "Uplink started"));
    } else {
	snd_perror("Failed starting uplink", err);
    }

    return err;
}

TInt CPjAudioEngine::Start()
{
    TInt err = KErrNone;
    
    if (iVoIPDnlink) {
	switch(dn_state_) {
	case STATE_READY:
	    err = StartPlay();
	    break;
	case STATE_NULL:
	    err = InitPlay();
	    if (err != KErrNone)
		return err;
	    dn_state_ = STATE_STARTING;
	    break;
	default:
	    break;
	}
    }
    
    if (iVoIPUplink) {
	switch(up_state_) {
	case STATE_READY:
	    err = StartRec();
	    break;
	case STATE_NULL:
	    err = InitRec();
	    if (err != KErrNone)
		return err;
	    up_state_ = STATE_STARTING;
	    break;
	default:
	    break;
	}
    }

    return err;
}

void CPjAudioEngine::Stop()
{
    if (iVoIPDnlink) {
	switch(dn_state_) {
	case STATE_STREAMING:
	    iVoIPDnlink->Stop();
	    dn_state_ = STATE_READY;
	    break;
	case STATE_STARTING:
	    dn_state_ = STATE_NULL;
	    break;
	default:
	    break;
	}
    }

    if (iVoIPUplink) {
	switch(up_state_) {
	case STATE_STREAMING:
	    iVoIPUplink->Stop();
	    up_state_ = STATE_READY;
	    break;
	case STATE_STARTING:
	    up_state_ = STATE_NULL;
	    break;
	default:
	    break;
	}
    }
}


TInt CPjAudioEngine::ActivateSpeaker(TBool active)
{
    TInt err = KErrNotSupported;
    
    if (iVoIPDnlink) {
	err = iVoIPDnlink->SetAudioDevice(active?
				    CVoIPAudioDownlinkStream::ELoudSpeaker :
				    CVoIPAudioDownlinkStream::EHandset);
	TRACE_((THIS_FILE, "Loudspeaker turned %s", (active? "on":"off")));
    }
    
    return err;
}

// Callback from MVoIPDownlinkObserver
void CPjAudioEngine::FillBuffer(const CVoIPAudioDownlinkStream& aSrc,
                                CVoIPDataBuffer* aBuffer)
{
    play_cb_(aBuffer, user_data_);
    iVoIPDnlink->BufferFilled(aBuffer);
}

// Callback from MVoIPUplinkObserver
void CPjAudioEngine::EmptyBuffer(const CVoIPAudioUplinkStream& aSrc,
                                 CVoIPDataBuffer* aBuffer)
{
    rec_cb_(aBuffer, user_data_);
    iVoIPUplink->BufferEmptied(aBuffer);
}

// Callback from MVoIPDownlinkObserver
void CPjAudioEngine::Event(const CVoIPAudioDownlinkStream& /*aSrc*/,
                           TInt aEventType,
                           TInt aError)
{
    switch (aEventType) {
    case MVoIPDownlinkObserver::KOpenComplete:
	if (aError == KErrNone) {
	    State last_state = dn_state_;

	    dn_state_ = STATE_READY;
	    TRACE_((THIS_FILE, "Downlink opened"));

	    if (last_state == STATE_STARTING)
		StartPlay();
	}
	break;

    case MVoIPDownlinkObserver::KDownlinkClosed:
	dn_state_ = STATE_NULL;
	TRACE_((THIS_FILE, "Downlink closed"));
	break;

    case MVoIPDownlinkObserver::KDownlinkError:
	dn_state_ = STATE_READY;
	snd_perror("Downlink problem", aError);
	break;
    default:
	break;
    }
}

// Callback from MVoIPUplinkObserver
void CPjAudioEngine::Event(const CVoIPAudioUplinkStream& /*aSrc*/,
                           TInt aEventType,
                           TInt aError)
{
    switch (aEventType) {
    case MVoIPUplinkObserver::KOpenComplete:
	if (aError == KErrNone) {
	    State last_state = up_state_;

	    up_state_ = STATE_READY;
	    TRACE_((THIS_FILE, "Uplink opened"));
	    
	    if (last_state == STATE_STARTING)
		StartRec();
	}
	break;

    case MVoIPUplinkObserver::KUplinkClosed:
	up_state_ = STATE_NULL;
	TRACE_((THIS_FILE, "Uplink closed"));
	break;

    case MVoIPUplinkObserver::KUplinkError:
	up_state_ = STATE_READY;
	snd_perror("Uplink problem", aError);
	break;
    default:
	break;
    }
}

// Callback from MVoIPFormatObserver
void CPjAudioEngine::Event(const CVoIPFormatIntfc& /*aSrc*/, 
			   TInt aEventType)
{
    snd_perror("Format event", aEventType);
}

/****************************************************************************
 * Internal VAS callbacks for PCM format
 */

#ifdef USE_NATIVE_PCM

static void RecCbPcm2(CVoIPDataBuffer *buf, void *user_data)
{
    struct vas_stream *strm = (struct vas_stream*) user_data;
    TPtr8 buffer(0, 0, 0);
    pj_int16_t *p_buf;
    unsigned buf_len;

    /* Get the buffer */
    buf->GetPayloadPtr(buffer);
    
    /* Call parent callback */
    p_buf = (pj_int16_t*) buffer.Ptr();
    buf_len = buffer.Length() >> 1;
    while (buf_len) {
	unsigned req;
	
	req = strm->param.samples_per_frame - strm->rec_buf_len;
	if (req > buf_len)
	    req = buf_len;
	pjmedia_copy_samples(strm->rec_buf + strm->rec_buf_len, p_buf, req);
	p_buf += req;
	buf_len -= req;
	strm->rec_buf_len += req;
	
	if (strm->rec_buf_len >= strm->param.samples_per_frame) {
	    pjmedia_frame f;

	    f.buf = strm->rec_buf;
	    f.type = PJMEDIA_FRAME_TYPE_AUDIO;
	    f.size = strm->param.samples_per_frame << 1;
	    strm->rec_cb(strm->user_data, &f);
	    strm->rec_buf_len = 0;
	}
    }
}

static void PlayCbPcm2(CVoIPDataBuffer *buf, void *user_data)
{
    struct vas_stream *strm = (struct vas_stream*) user_data;
    TPtr8 buffer(0, 0, 0);
    pjmedia_frame f;

    /* Get the buffer */
    buf->GetPayloadPtr(buffer);

    /* Call parent callback */
    f.buf = strm->play_buf;
    f.size = strm->param.samples_per_frame << 1;
    strm->play_cb(strm->user_data, &f);
    if (f.type != PJMEDIA_FRAME_TYPE_AUDIO) {
	pjmedia_zero_samples((pj_int16_t*)f.buf, 
			     strm->param.samples_per_frame);
    }
    f.size = strm->param.samples_per_frame << 1;

    /* Init buffer attributes and header. */
    buffer.Zero();
    buffer.Append((TUint8*)f.buf, f.size);

    /* Set the buffer */
    buf->SetPayloadPtr(buffer);
}

#else // not USE_NATIVE_PCM

static void RecCbPcm(CVoIPDataBuffer *buf, void *user_data)
{
    struct vas_stream *strm = (struct vas_stream*) user_data;
    TPtr8 buffer(0, 0, 0);

    /* Get the buffer */
    buf->GetPayloadPtr(buffer);
    
    /* Buffer has to contain normal speech. */
    pj_assert(buffer[0] == 1 && buffer[1] == 0);

    /* Detect the recorder G.711 frame size, player frame size will follow
     * this recorder frame size.
     */
    if (vas_g711_frame_len == 0) {
	vas_g711_frame_len = buffer.Length() < 160? 80 : 160;
	TRACE_((THIS_FILE, "Detected VAS G.711 frame size = %u samples",
		vas_g711_frame_len));
    }

    /* Decode VAS buffer (coded in G.711) and put the PCM result into rec_buf.
     * Whenever rec_buf is full, call parent stream callback.
     */
    unsigned samples_processed = 0;

    while (samples_processed < vas_g711_frame_len) {
	unsigned samples_to_process;
	unsigned samples_req;

	samples_to_process = vas_g711_frame_len - samples_processed;
	samples_req = (strm->param.samples_per_frame /
		       strm->param.channel_count /
		       strm->resample_factor) -
		      strm->rec_buf_len;
	if (samples_to_process > samples_req)
	    samples_to_process = samples_req;

	pjmedia_ulaw_decode(&strm->rec_buf[strm->rec_buf_len],
			    buffer.Ptr() + 2 + samples_processed,
			    samples_to_process);

	strm->rec_buf_len += samples_to_process;
	samples_processed += samples_to_process;

	/* Buffer is full, time to call parent callback */
	if (strm->rec_buf_len == strm->param.samples_per_frame / 
				 strm->param.channel_count /
				 strm->resample_factor) 
	{
	    pjmedia_frame f;

	    /* Need to resample clock rate? */
	    if (strm->rec_resample) {
		unsigned resampled = 0;
		
		while (resampled < strm->rec_buf_len) {
		    pjmedia_resample_run(strm->rec_resample, 
				&strm->rec_buf[resampled],
				strm->pcm_buf + 
				resampled * strm->resample_factor);
		    resampled += 80;
		}
		f.buf = strm->pcm_buf;
	    } else {
		f.buf = strm->rec_buf;
	    }

	    /* Need to convert channel count? */
	    if (strm->param.channel_count != 1) {
		pjmedia_convert_channel_1ton((pj_int16_t*)f.buf,
					     (pj_int16_t*)f.buf,
					     strm->param.channel_count,
					     strm->param.samples_per_frame /
					     strm->param.channel_count,
					     0);
	    }

	    /* Call parent callback */
	    f.type = PJMEDIA_FRAME_TYPE_AUDIO;
	    f.size = strm->param.samples_per_frame << 1;
	    strm->rec_cb(strm->user_data, &f);
	    strm->rec_buf_len = 0;
	}
    }
}

#endif // USE_NATIVE_PCM

static void PlayCbPcm(CVoIPDataBuffer *buf, void *user_data)
{
    struct vas_stream *strm = (struct vas_stream*) user_data;
    unsigned g711_frame_len = vas_g711_frame_len;
    TPtr8 buffer(0, 0, 0);

    /* Get the buffer */
    buf->GetPayloadPtr(buffer);

    /* Init buffer attributes and header. */
    buffer.Zero();
    buffer.Append(1);
    buffer.Append(0);

    /* Assume frame size is 10ms if frame size hasn't been known. */
    if (g711_frame_len == 0)
	g711_frame_len = 80;

    /* Call parent stream callback to get PCM samples to play,
     * encode the PCM samples into G.711 and put it into VAS buffer.
     */
    unsigned samples_processed = 0;
    
    while (samples_processed < g711_frame_len) {
	/* Need more samples to play, time to call parent callback */
	if (strm->play_buf_len == 0) {
	    pjmedia_frame f;
	    unsigned samples_got;
	    
	    f.size = strm->param.samples_per_frame << 1;
	    if (strm->play_resample || strm->param.channel_count != 1)
		f.buf = strm->pcm_buf;
	    else
		f.buf = strm->play_buf;

	    /* Call parent callback */
	    strm->play_cb(strm->user_data, &f);
	    if (f.type != PJMEDIA_FRAME_TYPE_AUDIO) {
		pjmedia_zero_samples((pj_int16_t*)f.buf, 
				     strm->param.samples_per_frame);
	    }
	    
	    samples_got = strm->param.samples_per_frame / 
			  strm->param.channel_count /
			  strm->resample_factor;

	    /* Need to convert channel count? */
	    if (strm->param.channel_count != 1) {
		pjmedia_convert_channel_nto1((pj_int16_t*)f.buf,
					     (pj_int16_t*)f.buf,
					     strm->param.channel_count,
					     strm->param.samples_per_frame,
					     PJ_FALSE,
					     0);
	    }

	    /* Need to resample clock rate? */
	    if (strm->play_resample) {
		unsigned resampled = 0;
		
		while (resampled < samples_got) 
		{
		    pjmedia_resample_run(strm->play_resample, 
				strm->pcm_buf + 
				resampled * strm->resample_factor,
				&strm->play_buf[resampled]);
		    resampled += 80;
		}
	    }
	    
	    strm->play_buf_len = samples_got;
	    strm->play_buf_start = 0;
	}

	unsigned tmp;

	tmp = PJ_MIN(strm->play_buf_len, g711_frame_len - samples_processed);
	pjmedia_ulaw_encode((pj_uint8_t*)&strm->play_buf[strm->play_buf_start],
			    &strm->play_buf[strm->play_buf_start],
			    tmp);
	buffer.Append((TUint8*)&strm->play_buf[strm->play_buf_start], tmp);
	samples_processed += tmp;
	strm->play_buf_len -= tmp;
	strm->play_buf_start += tmp;
    }

    /* Set the buffer */
    buf->SetPayloadPtr(buffer);
}

/****************************************************************************
 * Internal VAS callbacks for non-PCM format
 */

static void RecCb(CVoIPDataBuffer *buf, void *user_data)
{
    struct vas_stream *strm = (struct vas_stream*) user_data;
    pjmedia_frame_ext *frame = (pjmedia_frame_ext*) strm->rec_buf;
    TPtr8 buffer(0, 0, 0);

    /* Get the buffer */
    buf->GetPayloadPtr(buffer);
    
    switch(strm->param.ext_fmt.id) {
    case PJMEDIA_FORMAT_AMR:
	{
	    const pj_uint8_t *p = (const pj_uint8_t*)buffer.Ptr() + 1;
	    unsigned len = buffer.Length() - 1;
	    
	    pjmedia_frame_ext_append_subframe(frame, p, len << 3, 160);
	    if (frame->samples_cnt == strm->param.samples_per_frame) {
		frame->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
		strm->rec_cb(strm->user_data, (pjmedia_frame*)frame);
		frame->samples_cnt = 0;
		frame->subframe_cnt = 0;
	    }
	}
	break;
	
    case PJMEDIA_FORMAT_G729:
	{
	    /* Check if we got a normal or SID frame. */
	    if (buffer[0] != 0) {
		enum { NORMAL_LEN = 22, SID_LEN = 8 };
		TBitStream *bitstream = (TBitStream*)strm->strm_data;
		unsigned src_len = buffer.Length()- 2;
		
		pj_assert(src_len == NORMAL_LEN || src_len == SID_LEN);
		
		const TDesC8& p = bitstream->CompressG729Frame(
					    buffer.Right(src_len), 
					    src_len == SID_LEN);
		
		pjmedia_frame_ext_append_subframe(frame, p.Ptr(), 
						  p.Length() << 3, 80);
	    } else { /* We got null frame. */
		pjmedia_frame_ext_append_subframe(frame, NULL, 0, 80);
	    }
	    
	    if (frame->samples_cnt == strm->param.samples_per_frame) {
		frame->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
		strm->rec_cb(strm->user_data, (pjmedia_frame*)frame);
		frame->samples_cnt = 0;
		frame->subframe_cnt = 0;
	    }
	}
	break;

    case PJMEDIA_FORMAT_ILBC:
	{
	    unsigned samples_got;
	    
	    samples_got = strm->param.ext_fmt.bitrate == 15200? 160 : 240;
	    
	    /* Check if we got a normal or SID frame. */
	    if (buffer[0] != 0) {
		const pj_uint8_t *p = (const pj_uint8_t*)buffer.Ptr() + 2;
		unsigned len = buffer.Length() - 2;
		
		pjmedia_frame_ext_append_subframe(frame, p, len << 3,
						  samples_got);
	    } else { /* We got null frame. */
		pjmedia_frame_ext_append_subframe(frame, NULL, 0, samples_got);
	    }
	    
	    if (frame->samples_cnt == strm->param.samples_per_frame) {
		frame->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
		strm->rec_cb(strm->user_data, (pjmedia_frame*)frame);
		frame->samples_cnt = 0;
		frame->subframe_cnt = 0;
	    }
	}
	break;
	
    case PJMEDIA_FORMAT_PCMU:
    case PJMEDIA_FORMAT_PCMA:
	{
	    unsigned samples_processed = 0;
	    
	    /* Make sure it is normal frame. */
	    pj_assert(buffer[0] == 1 && buffer[1] == 0);

	    /* Detect the recorder G.711 frame size, player frame size will 
	     * follow this recorder frame size.
	     */
	    if (vas_g711_frame_len == 0) {
		vas_g711_frame_len = buffer.Length() < 160? 80 : 160;
		TRACE_((THIS_FILE, "Detected VAS G.711 frame size = %u samples",
			vas_g711_frame_len));
	    }
	    
	    /* Convert VAS buffer format into pjmedia_frame_ext. Whenever 
	     * samples count in the frame is equal to stream's samples per 
	     * frame, call parent stream callback.
	     */
	    while (samples_processed < vas_g711_frame_len) {
		unsigned tmp;
		const pj_uint8_t *pb = (const pj_uint8_t*)buffer.Ptr() +
				       2 + samples_processed;
    
		tmp = PJ_MIN(strm->param.samples_per_frame - frame->samples_cnt,
			     vas_g711_frame_len - samples_processed);
		
		pjmedia_frame_ext_append_subframe(frame, pb, tmp << 3, tmp);
		samples_processed += tmp;
    
		if (frame->samples_cnt == strm->param.samples_per_frame) {
		    frame->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
		    strm->rec_cb(strm->user_data, (pjmedia_frame*)frame);
		    frame->samples_cnt = 0;
		    frame->subframe_cnt = 0;
		}
	    }
	}
	break;
	
    default:
	break;
    }
}

static void PlayCb(CVoIPDataBuffer *buf, void *user_data)
{
    struct vas_stream *strm = (struct vas_stream*) user_data;
    pjmedia_frame_ext *frame = (pjmedia_frame_ext*) strm->play_buf;
    TPtr8 buffer(0, 0, 0);

    /* Get the buffer */
    buf->GetPayloadPtr(buffer);

    /* Init buffer attributes and header. */
    buffer.Zero();

    switch(strm->param.ext_fmt.id) {
    case PJMEDIA_FORMAT_AMR:
	{
	    if (frame->samples_cnt == 0) {
		frame->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
		strm->play_cb(strm->user_data, (pjmedia_frame*)frame);
		pj_assert(frame->base.type==PJMEDIA_FRAME_TYPE_EXTENDED ||
			  frame->base.type==PJMEDIA_FRAME_TYPE_NONE);
	    }

	    if (frame->base.type == PJMEDIA_FRAME_TYPE_EXTENDED) { 
		pjmedia_frame_ext_subframe *sf;
		unsigned samples_cnt;
		
		sf = pjmedia_frame_ext_get_subframe(frame, 0);
		samples_cnt = frame->samples_cnt / frame->subframe_cnt;
		
		if (sf->data && sf->bitlen) {
		    /* AMR header for VAS is one byte, the format (may be!):
		     * 0xxxxy00, where xxxx:frame type, y:not sure. 
		     */
		    unsigned len = (sf->bitlen+7)>>3;
		    enum {SID_FT = 8 };
		    pj_uint8_t amr_header = 4, ft = SID_FT;

		    if (len >= pjmedia_codec_amrnb_framelen[0])
			ft = pjmedia_codec_amr_get_mode2(PJ_TRUE, len);
		    
		    amr_header |= ft << 3;
		    buffer.Append(amr_header);
		    
		    buffer.Append((TUint8*)sf->data, len);
		} else {
		    enum {NO_DATA_FT = 15 };
		    pj_uint8_t amr_header = 4 | (NO_DATA_FT << 3);

		    buffer.Append(amr_header);
		}

		pjmedia_frame_ext_pop_subframes(frame, 1);
	    
	    } else { /* PJMEDIA_FRAME_TYPE_NONE */
		enum {NO_DATA_FT = 15 };
		pj_uint8_t amr_header = 4 | (NO_DATA_FT << 3);

		buffer.Append(amr_header);
		
		frame->samples_cnt = 0;
		frame->subframe_cnt = 0;
	    }
	}
	break;
	
    case PJMEDIA_FORMAT_G729:
	{
	    if (frame->samples_cnt == 0) {
		frame->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
		strm->play_cb(strm->user_data, (pjmedia_frame*)frame);
		pj_assert(frame->base.type==PJMEDIA_FRAME_TYPE_EXTENDED ||
			  frame->base.type==PJMEDIA_FRAME_TYPE_NONE);
	    }

	    if (frame->base.type == PJMEDIA_FRAME_TYPE_EXTENDED) { 
		pjmedia_frame_ext_subframe *sf;
		unsigned samples_cnt;
		
		sf = pjmedia_frame_ext_get_subframe(frame, 0);
		samples_cnt = frame->samples_cnt / frame->subframe_cnt;
		
		if (sf->data && sf->bitlen) {
		    enum { NORMAL_LEN = 10, SID_LEN = 2 };
		    pj_bool_t sid_frame = ((sf->bitlen >> 3) == SID_LEN);
		    TBitStream *bitstream = (TBitStream*)strm->strm_data;
		    const TPtrC8 src(sf->data, sf->bitlen>>3);
		    const TDesC8 &dst = bitstream->ExpandG729Frame(src,
								   sid_frame); 
		    if (sid_frame) {
			buffer.Append(2);
			buffer.Append(0);
		    } else {
			buffer.Append(1);
			buffer.Append(0);
		    }
		    buffer.Append(dst);
		} else {
		    buffer.Append(2);
		    buffer.Append(0);

		    buffer.AppendFill(0, 22);
		}

		pjmedia_frame_ext_pop_subframes(frame, 1);
	    
	    } else { /* PJMEDIA_FRAME_TYPE_NONE */
		buffer.Append(2);
		buffer.Append(0);
		
		buffer.AppendFill(0, 22);
	    }
	}
	break;
	
    case PJMEDIA_FORMAT_ILBC:
	{
	    if (frame->samples_cnt == 0) {
		frame->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
		strm->play_cb(strm->user_data, (pjmedia_frame*)frame);
		pj_assert(frame->base.type==PJMEDIA_FRAME_TYPE_EXTENDED ||
			  frame->base.type==PJMEDIA_FRAME_TYPE_NONE);
	    }

	    if (frame->base.type == PJMEDIA_FRAME_TYPE_EXTENDED) { 
		pjmedia_frame_ext_subframe *sf;
		unsigned samples_cnt;
		
		sf = pjmedia_frame_ext_get_subframe(frame, 0);
		samples_cnt = frame->samples_cnt / frame->subframe_cnt;
		
		pj_assert((strm->param.ext_fmt.bitrate == 15200 && 
			   samples_cnt == 160) ||
			  (strm->param.ext_fmt.bitrate != 15200 &&
			   samples_cnt == 240));
		
		if (sf->data && sf->bitlen) {
		    buffer.Append(1);
		    buffer.Append(0);
		    buffer.Append((TUint8*)sf->data, sf->bitlen>>3);
		} else {
		    unsigned frame_len;
		    
		    buffer.Append(1);
		    buffer.Append(0);
		    
		    /* VAS iLBC frame is 20ms or 30ms */
		    frame_len = strm->param.ext_fmt.bitrate == 15200? 38 : 50;
		    buffer.AppendFill(0, frame_len);
		}

		pjmedia_frame_ext_pop_subframes(frame, 1);
	    
	    } else { /* PJMEDIA_FRAME_TYPE_NONE */
		
		unsigned frame_len;
		
		buffer.Append(1);
		buffer.Append(0);
		
		/* VAS iLBC frame is 20ms or 30ms */
		frame_len = strm->param.ext_fmt.bitrate == 15200? 38 : 50;
		buffer.AppendFill(0, frame_len);

	    }
	}
	break;
	
    case PJMEDIA_FORMAT_PCMU:
    case PJMEDIA_FORMAT_PCMA:
	{
	    unsigned samples_ready = 0;
	    unsigned samples_req = vas_g711_frame_len;
	    
	    /* Assume frame size is 10ms if frame size hasn't been known. */
	    if (samples_req == 0)
		samples_req = 80;
	    
	    buffer.Append(1);
	    buffer.Append(0);
	    
	    /* Call parent stream callback to get samples to play. */
	    while (samples_ready < samples_req) {
		if (frame->samples_cnt == 0) {
		    frame->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
		    strm->play_cb(strm->user_data, (pjmedia_frame*)frame);
		    pj_assert(frame->base.type==PJMEDIA_FRAME_TYPE_EXTENDED ||
			      frame->base.type==PJMEDIA_FRAME_TYPE_NONE);
		}
    
		if (frame->base.type == PJMEDIA_FRAME_TYPE_EXTENDED) { 
		    pjmedia_frame_ext_subframe *sf;
		    unsigned samples_cnt;
		    
		    sf = pjmedia_frame_ext_get_subframe(frame, 0);
		    samples_cnt = frame->samples_cnt / frame->subframe_cnt;
		    if (sf->data && sf->bitlen) {
			buffer.Append((TUint8*)sf->data, sf->bitlen>>3);
		    } else {
			pj_uint8_t silc;
			silc = (strm->param.ext_fmt.id==PJMEDIA_FORMAT_PCMU)?
				pjmedia_linear2ulaw(0) : pjmedia_linear2alaw(0);
			buffer.AppendFill(silc, samples_cnt);
		    }
		    samples_ready += samples_cnt;
		    
		    pjmedia_frame_ext_pop_subframes(frame, 1);
		
		} else { /* PJMEDIA_FRAME_TYPE_NONE */
		    pj_uint8_t silc;
		    
		    silc = (strm->param.ext_fmt.id==PJMEDIA_FORMAT_PCMU)?
			    pjmedia_linear2ulaw(0) : pjmedia_linear2alaw(0);
		    buffer.AppendFill(silc, samples_req - samples_ready);

		    samples_ready = samples_req;
		    frame->samples_cnt = 0;
		    frame->subframe_cnt = 0;
		}
	    }
	}
	break;
	
    default:
	break;
    }

    /* Set the buffer */
    buf->SetPayloadPtr(buffer);
}


/****************************************************************************
 * Factory operations
 */

/*
 * C compatible declaration of VAS factory.
 */
PJ_BEGIN_DECL
PJ_DECL(pjmedia_aud_dev_factory*)pjmedia_symb_vas_factory(pj_pool_factory *pf);
PJ_END_DECL

/*
 * Init VAS audio driver.
 */
PJ_DEF(pjmedia_aud_dev_factory*) pjmedia_symb_vas_factory(pj_pool_factory *pf)
{
    struct vas_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "VAS", 1000, 1000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct vas_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;

    return &f->base;
}

/* API: init factory */
static pj_status_t factory_init(pjmedia_aud_dev_factory *f)
{
    struct vas_factory *af = (struct vas_factory*)f;
    CVoIPUtilityFactory *vas_factory_;
    CVoIPAudioUplinkStream *vas_uplink;
    CVoIPAudioDownlinkStream *vas_dnlink;
    RArray<TVoIPCodecFormat> uplink_formats, dnlink_formats;
    unsigned ext_fmt_cnt = 0;
    TVersion vas_version(1, 0, 0); /* Not really used at this time */
    TInt err;

    pj_ansi_strcpy(af->dev_info.name, "S60 VAS");
    af->dev_info.default_samples_per_sec = 8000;
    af->dev_info.caps = PJMEDIA_AUD_DEV_CAP_EXT_FORMAT |
			//PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING |
			PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING |
			PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE |
			PJMEDIA_AUD_DEV_CAP_VAD |
			PJMEDIA_AUD_DEV_CAP_CNG;
    af->dev_info.routes = PJMEDIA_AUD_DEV_ROUTE_EARPIECE | 
			  PJMEDIA_AUD_DEV_ROUTE_LOUDSPEAKER;
    af->dev_info.input_count = 1;
    af->dev_info.output_count = 1;
    af->dev_info.ext_fmt_cnt = 0;

    /* Enumerate supported formats */
    err = CVoIPUtilityFactory::CreateFactory(vas_factory_);
    if (err != KErrNone)
	goto on_error;

    /* On VAS 2.0, uplink & downlink stream should be instantiated before 
     * querying formats.
     */
    err = vas_factory_->CreateUplinkStream(vas_version, 
				          CVoIPUtilityFactory::EVoIPCall,
				          vas_uplink);
    if (err != KErrNone)
	goto on_error;
    
    err = vas_factory_->CreateDownlinkStream(vas_version, 
				            CVoIPUtilityFactory::EVoIPCall,
				            vas_dnlink);
    if (err != KErrNone)
	goto on_error;
    
    uplink_formats.Reset();
    err = vas_factory_->GetSupportedUplinkFormats(uplink_formats);
    if (err != KErrNone)
	goto on_error;

    dnlink_formats.Reset();
    err = vas_factory_->GetSupportedDownlinkFormats(dnlink_formats);
    if (err != KErrNone)
	goto on_error;

    /* Free the streams, they are just used for querying formats */
    delete vas_uplink;
    vas_uplink = NULL;
    delete vas_dnlink;
    vas_dnlink = NULL;
    delete vas_factory_;
    vas_factory_ = NULL;
    
    for (TInt i = 0; i < dnlink_formats.Count(); i++) {
	/* Format must be supported by both downlink & uplink. */
	if (uplink_formats.Find(dnlink_formats[i]) == KErrNotFound)
	    continue;
	
	switch (dnlink_formats[i]) {
	case EAMR_NB:
	    af->dev_info.ext_fmt[ext_fmt_cnt].id = PJMEDIA_FORMAT_AMR;
	    af->dev_info.ext_fmt[ext_fmt_cnt].bitrate = 7400;
	    af->dev_info.ext_fmt[ext_fmt_cnt].vad = PJ_TRUE;
	    break;

	case EG729:
	    af->dev_info.ext_fmt[ext_fmt_cnt].id = PJMEDIA_FORMAT_G729;
	    af->dev_info.ext_fmt[ext_fmt_cnt].bitrate = 8000;
	    af->dev_info.ext_fmt[ext_fmt_cnt].vad = PJ_FALSE;
	    break;

	case EILBC:
	    af->dev_info.ext_fmt[ext_fmt_cnt].id = PJMEDIA_FORMAT_ILBC;
	    af->dev_info.ext_fmt[ext_fmt_cnt].bitrate = 13333;
	    af->dev_info.ext_fmt[ext_fmt_cnt].vad = PJ_TRUE;
	    break;

	case EG711:
#if PJMEDIA_AUDIO_DEV_SYMB_VAS_VERSION==2
	case EG711_10MS:
#endif
	    af->dev_info.ext_fmt[ext_fmt_cnt].id = PJMEDIA_FORMAT_PCMU;
	    af->dev_info.ext_fmt[ext_fmt_cnt].bitrate = 64000;
	    af->dev_info.ext_fmt[ext_fmt_cnt].vad = PJ_FALSE;
	    ++ext_fmt_cnt;
	    af->dev_info.ext_fmt[ext_fmt_cnt].id = PJMEDIA_FORMAT_PCMA;
	    af->dev_info.ext_fmt[ext_fmt_cnt].bitrate = 64000;
	    af->dev_info.ext_fmt[ext_fmt_cnt].vad = PJ_FALSE;
	    break;
	
	default:
	    continue;
	}
	
	++ext_fmt_cnt;
    }
    
    af->dev_info.ext_fmt_cnt = ext_fmt_cnt;

    uplink_formats.Close();
    dnlink_formats.Close();
    
    PJ_LOG(3, (THIS_FILE, "VAS initialized"));

    return PJ_SUCCESS;
    
on_error:
    return PJ_RETURN_OS_ERROR(err);
}

/* API: destroy factory */
static pj_status_t factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct vas_factory *af = (struct vas_factory*)f;
    pj_pool_t *pool = af->pool;

    af->pool = NULL;
    pj_pool_release(pool);

    PJ_LOG(3, (THIS_FILE, "VAS destroyed"));
    
    return PJ_SUCCESS;
}

/* API: refresh the device list */
static pj_status_t factory_refresh(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_ENOTSUP;
}

/* API: get number of devices */
static unsigned factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return 1;
}

/* API: get device info */
static pj_status_t factory_get_dev_info(pjmedia_aud_dev_factory *f, 
					unsigned index,
					pjmedia_aud_dev_info *info)
{
    struct vas_factory *af = (struct vas_factory*)f;

    PJ_ASSERT_RETURN(index == 0, PJMEDIA_EAUD_INVDEV);

    pj_memcpy(info, &af->dev_info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t factory_default_param(pjmedia_aud_dev_factory *f,
					 unsigned index,
					 pjmedia_aud_param *param)
{
    struct vas_factory *af = (struct vas_factory*)f;

    PJ_ASSERT_RETURN(index == 0, PJMEDIA_EAUD_INVDEV);

    pj_bzero(param, sizeof(*param));
    param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    param->rec_id = index;
    param->play_id = index;
    param->clock_rate = af->dev_info.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = af->dev_info.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = BITS_PER_SAMPLE;
    param->flags = PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE;
    param->output_route = PJMEDIA_AUD_DEV_ROUTE_EARPIECE;

    return PJ_SUCCESS;
}


/* API: create stream */
static pj_status_t factory_create_stream(pjmedia_aud_dev_factory *f,
					 const pjmedia_aud_param *param,
					 pjmedia_aud_rec_cb rec_cb,
					 pjmedia_aud_play_cb play_cb,
					 void *user_data,
					 pjmedia_aud_stream **p_aud_strm)
{
    struct vas_factory *af = (struct vas_factory*)f;
    pj_pool_t *pool;
    struct vas_stream *strm;

    CPjAudioSetting vas_setting;
    PjAudioCallback vas_rec_cb;
    PjAudioCallback vas_play_cb;

    /* Can only support 16bits per sample */
    PJ_ASSERT_RETURN(param->bits_per_sample == BITS_PER_SAMPLE, PJ_EINVAL);

    /* Supported clock rates:
     * - for non-PCM format: 8kHz  
     * - for PCM format: 8kHz and 16kHz  
     */
    PJ_ASSERT_RETURN(param->clock_rate == 8000 ||
		     (param->clock_rate == 16000 && 
		      param->ext_fmt.id == PJMEDIA_FORMAT_L16),
		     PJ_EINVAL);

    /* Supported channels number:
     * - for non-PCM format: mono
     * - for PCM format: mono and stereo  
     */
    PJ_ASSERT_RETURN(param->channel_count == 1 || 
		     (param->channel_count == 2 &&
		      param->ext_fmt.id == PJMEDIA_FORMAT_L16),
		     PJ_EINVAL);

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(af->pf, "vas-dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct vas_stream);
    strm->pool = pool;
    strm->param = *param;

    if (strm->param.flags & PJMEDIA_AUD_DEV_CAP_EXT_FORMAT == 0)
	strm->param.ext_fmt.id = PJMEDIA_FORMAT_L16;
	
    /* Set audio engine fourcc. */
    switch(strm->param.ext_fmt.id) {
    case PJMEDIA_FORMAT_L16:
#ifdef USE_NATIVE_PCM	
	vas_setting.format = EPCM16;
#else
	vas_setting.format = EG711;
#endif
	break;
    case PJMEDIA_FORMAT_PCMU:
    case PJMEDIA_FORMAT_PCMA:
	vas_setting.format = EG711;
	break;
    case PJMEDIA_FORMAT_AMR:
	vas_setting.format = EAMR_NB;
	break;
    case PJMEDIA_FORMAT_G729:
	vas_setting.format = EG729;
	break;
    case PJMEDIA_FORMAT_ILBC:
	vas_setting.format = EILBC;
	break;
    default:
	vas_setting.format = ENULL;
	break;
    }

    /* Set audio engine mode. */
    if (strm->param.ext_fmt.id == PJMEDIA_FORMAT_L16)
    {
#ifdef USE_NATIVE_PCM	
	vas_setting.mode = 0;
#else
	vas_setting.mode = CVoIPFormatIntfc::EG711uLaw;
#endif
    } 
    else if (strm->param.ext_fmt.id == PJMEDIA_FORMAT_AMR)
    {
	vas_setting.mode = strm->param.ext_fmt.bitrate;
    } 
    else if (strm->param.ext_fmt.id == PJMEDIA_FORMAT_PCMU)
    {
	vas_setting.mode = CVoIPFormatIntfc::EG711uLaw;
    }
    else if (strm->param.ext_fmt.id == PJMEDIA_FORMAT_PCMA)
    {
	vas_setting.mode = CVoIPFormatIntfc::EG711ALaw;
    }
    else if (strm->param.ext_fmt.id == PJMEDIA_FORMAT_ILBC)
    {
	if (strm->param.ext_fmt.bitrate == 15200)
	    vas_setting.mode = CVoIPFormatIntfc::EiLBC20mSecFrame;
	else
	    vas_setting.mode = CVoIPFormatIntfc::EiLBC30mSecFrame;
    } else {
	vas_setting.mode = 0;
    }

    /* Disable VAD on L16, G711, iLBC, and also G729 (G729's SID 
     * potentially cause noise?).
     */
    if (strm->param.ext_fmt.id == PJMEDIA_FORMAT_PCMU ||
	strm->param.ext_fmt.id == PJMEDIA_FORMAT_PCMA ||
	strm->param.ext_fmt.id == PJMEDIA_FORMAT_L16 ||
	strm->param.ext_fmt.id == PJMEDIA_FORMAT_ILBC ||
	strm->param.ext_fmt.id == PJMEDIA_FORMAT_G729)
    {
	vas_setting.vad = EFalse;
    } else {
	vas_setting.vad = strm->param.ext_fmt.vad;
    }
    
    /* Set other audio engine attributes. */
    vas_setting.plc = strm->param.plc_enabled;
    vas_setting.cng = vas_setting.vad;
    vas_setting.loudspk = 
		strm->param.output_route==PJMEDIA_AUD_DEV_ROUTE_LOUDSPEAKER;

    /* Set audio engine callbacks. */
    if (strm->param.ext_fmt.id == PJMEDIA_FORMAT_L16) {
#ifdef USE_NATIVE_PCM
	vas_play_cb = &PlayCbPcm2;
	vas_rec_cb  = &RecCbPcm2;
#else
	vas_play_cb = &PlayCbPcm;
	vas_rec_cb  = &RecCbPcm;
#endif
    } else {
	vas_play_cb = &PlayCb;
	vas_rec_cb  = &RecCb;
    }

    strm->rec_cb = rec_cb;
    strm->play_cb = play_cb;
    strm->user_data = user_data;
    strm->resample_factor = strm->param.clock_rate / 8000;

    /* play_buf size is samples per frame scaled in to 8kHz mono. */
    strm->play_buf = (pj_int16_t*)pj_pool_zalloc(
					pool, 
					(strm->param.samples_per_frame / 
					strm->resample_factor /
					strm->param.channel_count) << 1);
    strm->play_buf_len = 0;
    strm->play_buf_start = 0;

    /* rec_buf size is samples per frame scaled in to 8kHz mono. */
    strm->rec_buf  = (pj_int16_t*)pj_pool_zalloc(
					pool, 
					(strm->param.samples_per_frame / 
					strm->resample_factor /
					strm->param.channel_count) << 1);
    strm->rec_buf_len = 0;

    if (strm->param.ext_fmt.id == PJMEDIA_FORMAT_G729) {
	TBitStream *g729_bitstream = new TBitStream;
	
	PJ_ASSERT_RETURN(g729_bitstream, PJ_ENOMEM);
	strm->strm_data = (void*)g729_bitstream;
    }
	
    /* Init resampler when format is PCM and clock rate is not 8kHz */
    if (strm->param.clock_rate != 8000 && 
	strm->param.ext_fmt.id == PJMEDIA_FORMAT_L16)
    {
	pj_status_t status;
	
	if (strm->param.dir & PJMEDIA_DIR_CAPTURE) {
	    /* Create resample for recorder */
	    status = pjmedia_resample_create( pool, PJ_TRUE, PJ_FALSE, 1, 
					      8000,
					      strm->param.clock_rate,
					      80,
					      &strm->rec_resample);
	    if (status != PJ_SUCCESS)
		return status;
	}
    
	if (strm->param.dir & PJMEDIA_DIR_PLAYBACK) {
	    /* Create resample for player */
	    status = pjmedia_resample_create( pool, PJ_TRUE, PJ_FALSE, 1, 
					      strm->param.clock_rate,
					      8000,
					      80 * strm->resample_factor,
					      &strm->play_resample);
	    if (status != PJ_SUCCESS)
		return status;
	}
    }

    /* Create PCM buffer, when the clock rate is not 8kHz or not mono */
    if (strm->param.ext_fmt.id == PJMEDIA_FORMAT_L16 &&
	(strm->resample_factor > 1 || strm->param.channel_count != 1)) 
    {
	strm->pcm_buf = (pj_int16_t*)pj_pool_zalloc(pool, 
					strm->param.samples_per_frame << 1);
    }

    
    /* Create the audio engine. */
    TRAPD(err, strm->engine = CPjAudioEngine::NewL(strm,
						   vas_rec_cb, vas_play_cb,
						   strm, vas_setting));
    if (err != KErrNone) {
    	pj_pool_release(pool);
	return PJ_RETURN_OS_ERROR(err);
    }

    /* Done */
    strm->base.op = &stream_op;
    *p_aud_strm = &strm->base;

    return PJ_SUCCESS;
}

/* API: Get stream info. */
static pj_status_t stream_get_param(pjmedia_aud_stream *s,
				    pjmedia_aud_param *pi)
{
    struct vas_stream *strm = (struct vas_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    /* Update the output volume setting */
    if (stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
		       &pi->output_vol) == PJ_SUCCESS)
    {
	pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
    }
    
    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t stream_get_cap(pjmedia_aud_stream *s,
				  pjmedia_aud_dev_cap cap,
				  void *pval)
{
    struct vas_stream *strm = (struct vas_stream*)s;
    pj_status_t status = PJ_ENOTSUP;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    switch (cap) {
    case PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE: 
	if (strm->param.dir & PJMEDIA_DIR_PLAYBACK) {
	    *(pjmedia_aud_dev_route*)pval = strm->param.output_route;
	    status = PJ_SUCCESS;
	}
	break;
    
    /* There is a case that GetMaxGain() stucks, e.g: in N95. */ 
    /*
    case PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING:
	if (strm->param.dir & PJMEDIA_DIR_CAPTURE) {
	    PJ_ASSERT_RETURN(strm->engine, PJ_EINVAL);
	    
	    TInt max_gain = strm->engine->GetMaxGain();
	    TInt gain = strm->engine->GetGain();
	    
	    if (max_gain > 0 && gain >= 0) {
		*(unsigned*)pval = gain * 100 / max_gain; 
		status = PJ_SUCCESS;
	    } else {
		status = PJMEDIA_EAUD_NOTREADY;
	    }
	}
	break;
    */

    case PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING:
	if (strm->param.dir & PJMEDIA_DIR_PLAYBACK) {
	    PJ_ASSERT_RETURN(strm->engine, PJ_EINVAL);
	    
	    TInt max_vol = strm->engine->GetMaxVolume();
	    TInt vol = strm->engine->GetVolume();
	    
	    if (max_vol > 0 && vol >= 0) {
		*(unsigned*)pval = vol * 100 / max_vol; 
		status = PJ_SUCCESS;
	    } else {
		status = PJMEDIA_EAUD_NOTREADY;
	    }
	}
	break;
    default:
	break;
    }
    
    return status;
}

/* API: set capability */
static pj_status_t stream_set_cap(pjmedia_aud_stream *s,
				  pjmedia_aud_dev_cap cap,
				  const void *pval)
{
    struct vas_stream *strm = (struct vas_stream*)s;
    pj_status_t status = PJ_ENOTSUP;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    switch (cap) {
    case PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE: 
	if (strm->param.dir & PJMEDIA_DIR_PLAYBACK) {
	    pjmedia_aud_dev_route r = *(const pjmedia_aud_dev_route*)pval;
	    TInt err;

	    PJ_ASSERT_RETURN(strm->engine, PJ_EINVAL);
	    
	    switch (r) {
	    case PJMEDIA_AUD_DEV_ROUTE_DEFAULT:
	    case PJMEDIA_AUD_DEV_ROUTE_EARPIECE:
		err = strm->engine->ActivateSpeaker(EFalse);
		status = (err==KErrNone)? PJ_SUCCESS:PJ_RETURN_OS_ERROR(err);
		break;
	    case PJMEDIA_AUD_DEV_ROUTE_LOUDSPEAKER:
		err = strm->engine->ActivateSpeaker(ETrue);
		status = (err==KErrNone)? PJ_SUCCESS:PJ_RETURN_OS_ERROR(err);
		break;
	    default:
		status = PJ_EINVAL;
		break;
	    }
	    if (status == PJ_SUCCESS)
		strm->param.output_route = r; 
	}
	break;

    /* There is a case that GetMaxGain() stucks, e.g: in N95. */ 
    /*
    case PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING:
	if (strm->param.dir & PJMEDIA_DIR_CAPTURE) {
	    PJ_ASSERT_RETURN(strm->engine, PJ_EINVAL);
	    
	    TInt max_gain = strm->engine->GetMaxGain();
	    if (max_gain > 0) {
		TInt gain, err;
		
		gain = *(unsigned*)pval * max_gain / 100;
		err = strm->engine->SetGain(gain);
		status = (err==KErrNone)? PJ_SUCCESS:PJ_RETURN_OS_ERROR(err);
	    } else {
		status = PJMEDIA_EAUD_NOTREADY;
	    }
	    if (status == PJ_SUCCESS)
		strm->param.input_vol = *(unsigned*)pval;
	}
	break;
    */

    case PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING:
	if (strm->param.dir & PJMEDIA_DIR_PLAYBACK) {
	    PJ_ASSERT_RETURN(strm->engine, PJ_EINVAL);
	    
	    TInt max_vol = strm->engine->GetMaxVolume();
	    if (max_vol > 0) {
		TInt vol, err;
		
		vol = *(unsigned*)pval * max_vol / 100;
		err = strm->engine->SetVolume(vol);
		status = (err==KErrNone)? PJ_SUCCESS:PJ_RETURN_OS_ERROR(err);
	    } else {
		status = PJMEDIA_EAUD_NOTREADY;
	    }
	    if (status == PJ_SUCCESS)
		strm->param.output_vol = *(unsigned*)pval;
	}
	break;
    default:
	break;
    }
    
    return status;
}

/* API: Start stream. */
static pj_status_t stream_start(pjmedia_aud_stream *strm)
{
    struct vas_stream *stream = (struct vas_stream*)strm;

    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if (stream->engine) {
	enum { VAS_WAIT_START = 2000 }; /* in msecs */
	TTime start, now;
	TInt err = stream->engine->Start();
	
    	if (err != KErrNone)
    	    return PJ_RETURN_OS_ERROR(err);

    	/* Perform synchronous start, timeout after VAS_WAIT_START ms */
	start.UniversalTime();
	do {
    	    pj_symbianos_poll(-1, 100);
    	    now.UniversalTime();
    	} while (!stream->engine->IsStarted() &&
		 (now.MicroSecondsFrom(start) < VAS_WAIT_START * 1000));
	
	if (stream->engine->IsStarted()) {
	    
	    /* Apply output volume setting if specified */
	    if (stream->param.flags & 
		PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) 
	    {
		stream_set_cap(strm,
			       PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, 
			       &stream->param.output_vol);
	    }

	    return PJ_SUCCESS;
	} else {
	    return PJ_ETIMEDOUT;
	}
    }    

    return PJ_EINVALIDOP;
}

/* API: Stop stream. */
static pj_status_t stream_stop(pjmedia_aud_stream *strm)
{
    struct vas_stream *stream = (struct vas_stream*)strm;

    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if (stream->engine) {
    	stream->engine->Stop();
    }

    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t stream_destroy(pjmedia_aud_stream *strm)
{
    struct vas_stream *stream = (struct vas_stream*)strm;

    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    stream_stop(strm);

    delete stream->engine;
    stream->engine = NULL;

    if (stream->param.ext_fmt.id == PJMEDIA_FORMAT_G729) {
	TBitStream *g729_bitstream = (TBitStream*)stream->strm_data;
	stream->strm_data = NULL;
	delete g729_bitstream;
    }

    pj_pool_t *pool;
    pool = stream->pool;
    if (pool) {
    	stream->pool = NULL;
    	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}

#endif // PJMEDIA_AUDIO_DEV_HAS_SYMB_VAS

