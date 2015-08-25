/* $Id: symb_mda_dev.cpp 3748 2011-09-09 09:51:10Z nanang $ */
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
#include <pjmedia-audiodev/audiodev_imp.h>
#include <pjmedia-audiodev/errno.h>
#include <pjmedia/alaw_ulaw.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/os.h>
#include <pj/string.h>

#if PJMEDIA_AUDIO_DEV_HAS_SYMB_MDA

/*
 * This file provides sound implementation for Symbian Audio Streaming
 * device. Application using this sound abstraction must link with:
 *  - mediaclientaudiostream.lib, and
 *  - mediaclientaudioinputstream.lib 
 */
#include <mda/common/audio.h>
#include <mdaaudiooutputstream.h>
#include <mdaaudioinputstream.h>


#define THIS_FILE			"symb_mda_dev.c"
#define BITS_PER_SAMPLE			16
#define BYTES_PER_SAMPLE		(BITS_PER_SAMPLE/8)


#if 1
#   define TRACE_(st) PJ_LOG(3, st)
#else
#   define TRACE_(st)
#endif


/* MDA factory */
struct mda_factory
{
    pjmedia_aud_dev_factory	 base;
    pj_pool_t			*pool;
    pj_pool_factory		*pf;
    pjmedia_aud_dev_info	 dev_info;
};

/* Forward declaration of internal engine. */
class CPjAudioInputEngine;
class CPjAudioOutputEngine;

/* MDA stream. */
struct mda_stream
{
    // Base
    pjmedia_aud_stream	 base;			/**< Base class.	*/
    
    // Pool
    pj_pool_t		*pool;			/**< Memory pool.       */

    // Common settings.
    pjmedia_aud_param param;		/**< Stream param.	*/

    // Audio engine
    CPjAudioInputEngine	*in_engine;		/**< Record engine.	*/
    CPjAudioOutputEngine *out_engine;		/**< Playback engine.	*/
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


/*
 * Convert clock rate to Symbian's TMdaAudioDataSettings capability.
 */
static TInt get_clock_rate_cap(unsigned clock_rate)
{
    switch (clock_rate) {
    case 8000:  return TMdaAudioDataSettings::ESampleRate8000Hz;
    case 11025: return TMdaAudioDataSettings::ESampleRate11025Hz;
    case 12000: return TMdaAudioDataSettings::ESampleRate12000Hz;
    case 16000: return TMdaAudioDataSettings::ESampleRate16000Hz;
    case 22050: return TMdaAudioDataSettings::ESampleRate22050Hz;
    case 24000: return TMdaAudioDataSettings::ESampleRate24000Hz;
    case 32000: return TMdaAudioDataSettings::ESampleRate32000Hz;
    case 44100: return TMdaAudioDataSettings::ESampleRate44100Hz;
    case 48000: return TMdaAudioDataSettings::ESampleRate48000Hz;
    case 64000: return TMdaAudioDataSettings::ESampleRate64000Hz;
    case 96000: return TMdaAudioDataSettings::ESampleRate96000Hz;
    default:
	return 0;
    }
}

/*
 * Convert number of channels into Symbian's TMdaAudioDataSettings capability.
 */
static TInt get_channel_cap(unsigned channel_count)
{
    switch (channel_count) {
    case 1: return TMdaAudioDataSettings::EChannelsMono;
    case 2: return TMdaAudioDataSettings::EChannelsStereo;
    default:
	return 0;
    }
}

/*
 * Utility: print sound device error
 */
static void snd_perror(const char *title, TInt rc) 
{
    PJ_LOG(1,(THIS_FILE, "%s: error code %d", title, rc));
}
 
//////////////////////////////////////////////////////////////////////////////
//

/*
 * Implementation: Symbian Input Stream.
 */
class CPjAudioInputEngine : public CBase, MMdaAudioInputStreamCallback
{
public:
    enum State
    {
	STATE_INACTIVE,
	STATE_ACTIVE,
    };

    ~CPjAudioInputEngine();

    static CPjAudioInputEngine *NewL(struct mda_stream *parent_strm,
				     pjmedia_aud_rec_cb rec_cb,
				     void *user_data);

    static CPjAudioInputEngine *NewLC(struct mda_stream *parent_strm,
				      pjmedia_aud_rec_cb rec_cb,
				      void *user_data);

    pj_status_t StartRecord();
    void Stop();

    pj_status_t SetGain(TInt gain) { 
	if (iInputStream_) { 
	    iInputStream_->SetGain(gain);
	    return PJ_SUCCESS;
	} else
	    return PJ_EINVALIDOP;
    }
    
    TInt GetGain() { 
	if (iInputStream_) { 
	    return iInputStream_->Gain();
	} else
	    return PJ_EINVALIDOP;
    }

    TInt GetMaxGain() { 
	if (iInputStream_) { 
	    return iInputStream_->MaxGain();
	} else
	    return PJ_EINVALIDOP;
    }
    
private:
    State		     state_;
    struct mda_stream	    *parentStrm_;
    pjmedia_aud_rec_cb	     recCb_;
    void		    *userData_;
    CMdaAudioInputStream    *iInputStream_;
    HBufC8		    *iStreamBuffer_;
    TPtr8		     iFramePtr_;
    TInt		     lastError_;
    pj_uint32_t		     timeStamp_;
    CActiveSchedulerWait     startAsw_;

    // cache variable
    // to avoid calculating frame length repeatedly
    TInt		     frameLen_;
    
    // sometimes recorded size != requested framesize, so let's
    // provide a buffer to make sure the rec callback returning
    // framesize as requested.
    TUint8		    *frameRecBuf_;
    TInt		     frameRecBufLen_;

    CPjAudioInputEngine(struct mda_stream *parent_strm,
	    pjmedia_aud_rec_cb rec_cb,
			void *user_data);
    void ConstructL();
    TPtr8 & GetFrame();
    
public:
    virtual void MaiscOpenComplete(TInt aError);
    virtual void MaiscBufferCopied(TInt aError, const TDesC8 &aBuffer);
    virtual void MaiscRecordComplete(TInt aError);

};


CPjAudioInputEngine::CPjAudioInputEngine(struct mda_stream *parent_strm,
					 pjmedia_aud_rec_cb rec_cb,
					 void *user_data)
    : state_(STATE_INACTIVE), parentStrm_(parent_strm), 
      recCb_(rec_cb), userData_(user_data), 
      iInputStream_(NULL), iStreamBuffer_(NULL), iFramePtr_(0, 0),
      lastError_(KErrNone), timeStamp_(0),
      frameLen_(parent_strm->param.samples_per_frame * 
	        BYTES_PER_SAMPLE),
      frameRecBuf_(NULL), frameRecBufLen_(0)
{
}

CPjAudioInputEngine::~CPjAudioInputEngine()
{
    Stop();

    delete iStreamBuffer_;
    iStreamBuffer_ = NULL;
    
    delete [] frameRecBuf_;
    frameRecBuf_ = NULL;
    frameRecBufLen_ = 0;
}

void CPjAudioInputEngine::ConstructL()
{
    iStreamBuffer_ = HBufC8::NewL(frameLen_);
    CleanupStack::PushL(iStreamBuffer_);

    frameRecBuf_ = new TUint8[frameLen_*2];
    CleanupStack::PushL(frameRecBuf_);
}

CPjAudioInputEngine *CPjAudioInputEngine::NewLC(struct mda_stream *parent,
						pjmedia_aud_rec_cb rec_cb,
					        void *user_data)
{
    CPjAudioInputEngine* self = new (ELeave) CPjAudioInputEngine(parent,
								 rec_cb, 
								 user_data);
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

CPjAudioInputEngine *CPjAudioInputEngine::NewL(struct mda_stream *parent,
					       pjmedia_aud_rec_cb rec_cb,
					       void *user_data)
{
    CPjAudioInputEngine *self = NewLC(parent, rec_cb, user_data);
    CleanupStack::Pop(self->frameRecBuf_);
    CleanupStack::Pop(self->iStreamBuffer_);
    CleanupStack::Pop(self);
    return self;
}


pj_status_t CPjAudioInputEngine::StartRecord()
{

    // Ignore command if recording is in progress.
    if (state_ == STATE_ACTIVE)
	return PJ_SUCCESS;

    // According to Nokia's AudioStream example, some 2nd Edition, FP2 devices
    // (such as Nokia 6630) require the stream to be reconstructed each time 
    // before calling Open() - otherwise the callback never gets called.
    // For uniform behavior, lets just delete/re-create the stream for all
    // devices.

    // Destroy existing stream.
    if (iInputStream_) delete iInputStream_;
    iInputStream_ = NULL;

    // Create the stream.
    TRAPD(err, iInputStream_ = CMdaAudioInputStream::NewL(*this));
    if (err != KErrNone)
	return PJ_RETURN_OS_ERROR(err);

    // Initialize settings.
    TMdaAudioDataSettings iStreamSettings;
    iStreamSettings.iChannels = 
			    get_channel_cap(parentStrm_->param.channel_count);
    iStreamSettings.iSampleRate = 
			    get_clock_rate_cap(parentStrm_->param.clock_rate);

    pj_assert(iStreamSettings.iChannels != 0 && 
	      iStreamSettings.iSampleRate != 0);

    PJ_LOG(4,(THIS_FILE, "Opening sound device for capture, "
    		         "clock rate=%d, channel count=%d..",
    		         parentStrm_->param.clock_rate, 
    		         parentStrm_->param.channel_count));
    
    // Open stream.
    lastError_ = KRequestPending;
    iInputStream_->Open(&iStreamSettings);
    
#if defined(PJMEDIA_AUDIO_DEV_MDA_USE_SYNC_START) && \
    PJMEDIA_AUDIO_DEV_MDA_USE_SYNC_START != 0
    
    startAsw_.Start();
    
#endif
    
    // Success
    PJ_LOG(4,(THIS_FILE, "Sound capture started."));
    return PJ_SUCCESS;
}


void CPjAudioInputEngine::Stop()
{
    // If capture is in progress, stop it.
    if (iInputStream_ && state_ == STATE_ACTIVE) {
    	lastError_ = KRequestPending;
    	iInputStream_->Stop();

	// Wait until it's actually stopped
    	while (lastError_ == KRequestPending)
	    pj_symbianos_poll(-1, 100);
    }

    if (iInputStream_) {
	delete iInputStream_;
	iInputStream_ = NULL;
    }
    
    if (startAsw_.IsStarted()) {
	startAsw_.AsyncStop();
    }
    
    state_ = STATE_INACTIVE;
}


TPtr8 & CPjAudioInputEngine::GetFrame() 
{
    //iStreamBuffer_->Des().FillZ(frameLen_);
    iFramePtr_.Set((TUint8*)(iStreamBuffer_->Ptr()), frameLen_, frameLen_);
    return iFramePtr_;
}

void CPjAudioInputEngine::MaiscOpenComplete(TInt aError)
{
    if (startAsw_.IsStarted()) {
	startAsw_.AsyncStop();
    }
    
    lastError_ = aError;
    if (aError != KErrNone) {
        snd_perror("Error in MaiscOpenComplete()", aError);
    	return;
    }

    /* Apply input volume setting if specified */
    if (parentStrm_->param.flags & 
        PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING) 
    {
        stream_set_cap(&parentStrm_->base,
                       PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING, 
                       &parentStrm_->param.input_vol);
    }

    // set stream priority to normal and time sensitive
    iInputStream_->SetPriority(EPriorityNormal, 
    			       EMdaPriorityPreferenceTime);				

    // Read the first frame.
    TPtr8 & frm = GetFrame();
    TRAPD(err2, iInputStream_->ReadL(frm));
    if (err2) {
    	PJ_LOG(4,(THIS_FILE, "Exception in iInputStream_->ReadL()"));
	lastError_ = err2;
	return;
    }

    // input stream opened succesfully, set status to Active
    state_ = STATE_ACTIVE;
}

void CPjAudioInputEngine::MaiscBufferCopied(TInt aError, 
					    const TDesC8 &aBuffer)
{
    lastError_ = aError;
    if (aError != KErrNone) {
    	snd_perror("Error in MaiscBufferCopied()", aError);
	return;
    }

    if (frameRecBufLen_ || aBuffer.Length() < frameLen_) {
	pj_memcpy(frameRecBuf_ + frameRecBufLen_, (void*) aBuffer.Ptr(), aBuffer.Length());
	frameRecBufLen_ += aBuffer.Length();
    }

    if (frameRecBufLen_) {
    	while (frameRecBufLen_ >= frameLen_) {
    	    pjmedia_frame f;
    	    
    	    f.type = PJMEDIA_FRAME_TYPE_AUDIO;
    	    f.buf = frameRecBuf_;
    	    f.size = frameLen_;
    	    f.timestamp.u32.lo = timeStamp_;
    	    f.bit_info = 0;
    	    
    	    // Call the callback.
	    recCb_(userData_, &f);
	    // Increment timestamp.
	    timeStamp_ += parentStrm_->param.samples_per_frame;

	    frameRecBufLen_ -= frameLen_;
	    pj_memmove(frameRecBuf_, frameRecBuf_+frameLen_, frameRecBufLen_);
    	}
    } else {
	pjmedia_frame f;
	
	f.type = PJMEDIA_FRAME_TYPE_AUDIO;
	f.buf = (void*)aBuffer.Ptr();
	f.size = aBuffer.Length();
	f.timestamp.u32.lo = timeStamp_;
	f.bit_info = 0;
	
	// Call the callback.
	recCb_(userData_, &f);
	
	// Increment timestamp.
	timeStamp_ += parentStrm_->param.samples_per_frame;
    }

    // Record next frame
    TPtr8 & frm = GetFrame();
    TRAPD(err2, iInputStream_->ReadL(frm));
    if (err2) {
    	PJ_LOG(4,(THIS_FILE, "Exception in iInputStream_->ReadL()"));
    }
}


void CPjAudioInputEngine::MaiscRecordComplete(TInt aError)
{
    lastError_ = aError;
    state_ = STATE_INACTIVE;
    if (aError != KErrNone && aError != KErrCancel) {
    	snd_perror("Error in MaiscRecordComplete()", aError);
    }
}



//////////////////////////////////////////////////////////////////////////////
//

/*
 * Implementation: Symbian Output Stream.
 */

class CPjAudioOutputEngine : public CBase, MMdaAudioOutputStreamCallback
{
public:
    enum State
    {
	STATE_INACTIVE,
	STATE_ACTIVE,
    };

    ~CPjAudioOutputEngine();

    static CPjAudioOutputEngine *NewL(struct mda_stream *parent_strm,
				      pjmedia_aud_play_cb play_cb,
				      void *user_data);

    static CPjAudioOutputEngine *NewLC(struct mda_stream *parent_strm,
				       pjmedia_aud_play_cb rec_cb,
				       void *user_data);

    pj_status_t StartPlay();
    void Stop();

    pj_status_t SetVolume(TInt vol) { 
	if (iOutputStream_) { 
	    iOutputStream_->SetVolume(vol);
	    return PJ_SUCCESS;
	} else
	    return PJ_EINVALIDOP;
    }
    
    TInt GetVolume() { 
	if (iOutputStream_) { 
	    return iOutputStream_->Volume();
	} else
	    return PJ_EINVALIDOP;
    }

    TInt GetMaxVolume() { 
	if (iOutputStream_) { 
	    return iOutputStream_->MaxVolume();
	} else
	    return PJ_EINVALIDOP;
    }

private:
    State		     state_;
    struct mda_stream	    *parentStrm_;
    pjmedia_aud_play_cb	     playCb_;
    void		    *userData_;
    CMdaAudioOutputStream   *iOutputStream_;
    TUint8		    *frameBuf_;
    unsigned		     frameBufSize_;
    TPtrC8		     frame_;
    TInt		     lastError_;
    unsigned		     timestamp_;
    CActiveSchedulerWait     startAsw_;

    CPjAudioOutputEngine(struct mda_stream *parent_strm,
			 pjmedia_aud_play_cb play_cb,
			 void *user_data);
    void ConstructL();

    virtual void MaoscOpenComplete(TInt aError);
    virtual void MaoscBufferCopied(TInt aError, const TDesC8& aBuffer);
    virtual void MaoscPlayComplete(TInt aError);
};


CPjAudioOutputEngine::CPjAudioOutputEngine(struct mda_stream *parent_strm,
					   pjmedia_aud_play_cb play_cb,
					   void *user_data) 
: state_(STATE_INACTIVE), parentStrm_(parent_strm), playCb_(play_cb), 
  userData_(user_data), iOutputStream_(NULL), frameBuf_(NULL),
  lastError_(KErrNone), timestamp_(0)
{
}


void CPjAudioOutputEngine::ConstructL()
{
    frameBufSize_ = parentStrm_->param.samples_per_frame *
		    BYTES_PER_SAMPLE;
    frameBuf_ = new TUint8[frameBufSize_];
}

CPjAudioOutputEngine::~CPjAudioOutputEngine()
{
    Stop();
    delete [] frameBuf_;	
}

CPjAudioOutputEngine *
CPjAudioOutputEngine::NewLC(struct mda_stream *parent_strm,
			    pjmedia_aud_play_cb play_cb,
			    void *user_data)
{
    CPjAudioOutputEngine* self = new (ELeave) CPjAudioOutputEngine(parent_strm,
								   play_cb, 
								   user_data);
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

CPjAudioOutputEngine *
CPjAudioOutputEngine::NewL(struct mda_stream *parent_strm,
			   pjmedia_aud_play_cb play_cb,
			   void *user_data)
{
    CPjAudioOutputEngine *self = NewLC(parent_strm, play_cb, user_data);
    CleanupStack::Pop(self);
    return self;
}

pj_status_t CPjAudioOutputEngine::StartPlay()
{
    // Ignore command if playing is in progress.
    if (state_ == STATE_ACTIVE)
	return PJ_SUCCESS;
    
    // Destroy existing stream.
    if (iOutputStream_) delete iOutputStream_;
    iOutputStream_ = NULL;
    
    // Create the stream
    TRAPD(err, iOutputStream_ = CMdaAudioOutputStream::NewL(*this));
    if (err != KErrNone)
	return PJ_RETURN_OS_ERROR(err);
    
    // Initialize settings.
    TMdaAudioDataSettings iStreamSettings;
    iStreamSettings.iChannels = 
			    get_channel_cap(parentStrm_->param.channel_count);
    iStreamSettings.iSampleRate = 
			    get_clock_rate_cap(parentStrm_->param.clock_rate);

    pj_assert(iStreamSettings.iChannels != 0 && 
	      iStreamSettings.iSampleRate != 0);
    
    PJ_LOG(4,(THIS_FILE, "Opening sound device for playback, "
    		         "clock rate=%d, channel count=%d..",
    		         parentStrm_->param.clock_rate, 
    		         parentStrm_->param.channel_count));

    // Open stream.
    lastError_ = KRequestPending;
    iOutputStream_->Open(&iStreamSettings);
    
#if defined(PJMEDIA_AUDIO_DEV_MDA_USE_SYNC_START) && \
    PJMEDIA_AUDIO_DEV_MDA_USE_SYNC_START != 0
    
    startAsw_.Start();
    
#endif

    // Success
    PJ_LOG(4,(THIS_FILE, "Sound playback started"));
    return PJ_SUCCESS;

}

void CPjAudioOutputEngine::Stop()
{
    // Stop stream if it's playing
    if (iOutputStream_ && state_ != STATE_INACTIVE) {
    	lastError_ = KRequestPending;
    	iOutputStream_->Stop();

	// Wait until it's actually stopped
    	while (lastError_ == KRequestPending)
	    pj_symbianos_poll(-1, 100);
    }
    
    if (iOutputStream_) {	
	delete iOutputStream_;
	iOutputStream_ = NULL;
    }
    
    if (startAsw_.IsStarted()) {
	startAsw_.AsyncStop();
    }
    
    state_ = STATE_INACTIVE;
}

void CPjAudioOutputEngine::MaoscOpenComplete(TInt aError)
{
    if (startAsw_.IsStarted()) {
	startAsw_.AsyncStop();
    }

    lastError_ = aError;
    
    if (aError==KErrNone) {
	// set stream properties, 16bit 8KHz mono
	TMdaAudioDataSettings iSettings;
	iSettings.iChannels = 
			get_channel_cap(parentStrm_->param.channel_count);
	iSettings.iSampleRate = 
			get_clock_rate_cap(parentStrm_->param.clock_rate);

	iOutputStream_->SetAudioPropertiesL(iSettings.iSampleRate, 
					    iSettings.iChannels);

        /* Apply output volume setting if specified */
        if (parentStrm_->param.flags & 
            PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) 
        {
            stream_set_cap(&parentStrm_->base,
                           PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, 
                           &parentStrm_->param.output_vol);
        } else {
            // set volume to 1/2th of stream max volume
            iOutputStream_->SetVolume(iOutputStream_->MaxVolume()/2);
        }
	
	// set stream priority to normal and time sensitive
	iOutputStream_->SetPriority(EPriorityNormal, 
				    EMdaPriorityPreferenceTime);				

	// Call callback to retrieve frame from upstream.
	pjmedia_frame f;
	pj_status_t status;
	
	f.type = PJMEDIA_FRAME_TYPE_AUDIO;
	f.buf = frameBuf_;
	f.size = frameBufSize_;
	f.timestamp.u32.lo = timestamp_;
	f.bit_info = 0;

	status = playCb_(this->userData_, &f);
	if (status != PJ_SUCCESS) {
	    this->Stop();
	    return;
	}

	if (f.type != PJMEDIA_FRAME_TYPE_AUDIO)
	    pj_bzero(frameBuf_, frameBufSize_);
	
	// Increment timestamp.
	timestamp_ += (frameBufSize_ / BYTES_PER_SAMPLE);

	// issue WriteL() to write the first audio data block, 
	// subsequent calls to WriteL() will be issued in 
	// MMdaAudioOutputStreamCallback::MaoscBufferCopied() 
	// until whole data buffer is written.
	frame_.Set(frameBuf_, frameBufSize_);
	iOutputStream_->WriteL(frame_);

	// output stream opened succesfully, set status to Active
	state_ = STATE_ACTIVE;
    } else {
    	snd_perror("Error in MaoscOpenComplete()", aError);
    }
}

void CPjAudioOutputEngine::MaoscBufferCopied(TInt aError, 
					     const TDesC8& aBuffer)
{
    PJ_UNUSED_ARG(aBuffer);

    if (aError==KErrNone) {
    	// Buffer successfully written, feed another one.

	// Call callback to retrieve frame from upstream.
	pjmedia_frame f;
	pj_status_t status;
	
	f.type = PJMEDIA_FRAME_TYPE_AUDIO;
	f.buf = frameBuf_;
	f.size = frameBufSize_;
	f.timestamp.u32.lo = timestamp_;
	f.bit_info = 0;

	status = playCb_(this->userData_, &f);
	if (status != PJ_SUCCESS) {
	    this->Stop();
	    return;
	}

	if (f.type != PJMEDIA_FRAME_TYPE_AUDIO)
	    pj_bzero(frameBuf_, frameBufSize_);
	
	// Increment timestamp.
	timestamp_ += (frameBufSize_ / BYTES_PER_SAMPLE);

	// Write to playback stream.
	frame_.Set(frameBuf_, frameBufSize_);
	iOutputStream_->WriteL(frame_);

    } else if (aError==KErrAbort) {
	// playing was aborted, due to call to CMdaAudioOutputStream::Stop()
	state_ = STATE_INACTIVE;
    } else  {
	// error writing data to output
	lastError_ = aError;
	state_ = STATE_INACTIVE;
	snd_perror("Error in MaoscBufferCopied()", aError);
    }
}

void CPjAudioOutputEngine::MaoscPlayComplete(TInt aError)
{
    lastError_ = aError;
    state_ = STATE_INACTIVE;
    if (aError != KErrNone && aError != KErrCancel) {
    	snd_perror("Error in MaoscPlayComplete()", aError);
    }
}

/****************************************************************************
 * Factory operations
 */

/*
 * C compatible declaration of MDA factory.
 */
PJ_BEGIN_DECL
PJ_DECL(pjmedia_aud_dev_factory*) pjmedia_symb_mda_factory(pj_pool_factory *pf);
PJ_END_DECL

/*
 * Init Symbian audio driver.
 */
pjmedia_aud_dev_factory* pjmedia_symb_mda_factory(pj_pool_factory *pf)
{
    struct mda_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "symb_aud", 1000, 1000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct mda_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;

    return &f->base;
}

/* API: init factory */
static pj_status_t factory_init(pjmedia_aud_dev_factory *f)
{
    struct mda_factory *af = (struct mda_factory*)f;

    pj_ansi_strcpy(af->dev_info.name, "Symbian Audio");
    af->dev_info.default_samples_per_sec = 8000;
    af->dev_info.caps = PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING |
			PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
    af->dev_info.input_count = 1;
    af->dev_info.output_count = 1;

    PJ_LOG(4, (THIS_FILE, "Symb Mda initialized"));

    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct mda_factory *af = (struct mda_factory*)f;
    pj_pool_t *pool = af->pool;

    af->pool = NULL;
    pj_pool_release(pool);

    PJ_LOG(4, (THIS_FILE, "Symbian Mda destroyed"));
    
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
    struct mda_factory *af = (struct mda_factory*)f;

    PJ_ASSERT_RETURN(index == 0, PJMEDIA_EAUD_INVDEV);

    pj_memcpy(info, &af->dev_info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t factory_default_param(pjmedia_aud_dev_factory *f,
					 unsigned index,
					 pjmedia_aud_param *param)
{
    struct mda_factory *af = (struct mda_factory*)f;

    PJ_ASSERT_RETURN(index == 0, PJMEDIA_EAUD_INVDEV);

    pj_bzero(param, sizeof(*param));
    param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    param->rec_id = index;
    param->play_id = index;
    param->clock_rate = af->dev_info.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = af->dev_info.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = BITS_PER_SAMPLE;
    // Don't set the flags without specifying the flags value.
    //param->flags = af->dev_info.caps;

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
    struct mda_factory *mf = (struct mda_factory*)f;
    pj_pool_t *pool;
    struct mda_stream *strm;

    /* Can only support 16bits per sample raw PCM format. */
    PJ_ASSERT_RETURN(param->bits_per_sample == BITS_PER_SAMPLE, PJ_EINVAL);
    PJ_ASSERT_RETURN((param->flags & PJMEDIA_AUD_DEV_CAP_EXT_FORMAT)==0 ||
		     param->ext_fmt.id == PJMEDIA_FORMAT_L16,
		     PJ_ENOTSUP);
    
    /* It seems that MDA recorder only supports for mono channel. */
    PJ_ASSERT_RETURN(param->channel_count == 1, PJ_EINVAL);

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(mf->pf, "symb_aud_dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct mda_stream);
    strm->pool = pool;
    strm->param = *param;

    // Create the output stream.
    if (strm->param.dir & PJMEDIA_DIR_PLAYBACK) {
	TRAPD(err, strm->out_engine = CPjAudioOutputEngine::NewL(strm, play_cb,
								 user_data));
	if (err != KErrNone) {
	    pj_pool_release(pool);	
	    return PJ_RETURN_OS_ERROR(err);
	}
    }

    // Create the input stream.
    if (strm->param.dir & PJMEDIA_DIR_CAPTURE) {
	TRAPD(err, strm->in_engine = CPjAudioInputEngine::NewL(strm, rec_cb, 
							       user_data));
	if (err != KErrNone) {
	    strm->in_engine = NULL;
	    delete strm->out_engine;
	    strm->out_engine = NULL;
	    pj_pool_release(pool);	
	    return PJ_RETURN_OS_ERROR(err);
	}
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
    struct mda_stream *strm = (struct mda_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));
    
    /* Update the output volume setting */
    if (stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
		       &pi->output_vol) == PJ_SUCCESS)
    {
	pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
    }
    
    /* Update the input volume setting */
    if (stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING,
		       &pi->input_vol) == PJ_SUCCESS)
    {
	pi->flags |= PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING;
    }
    
    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t stream_get_cap(pjmedia_aud_stream *s,
				  pjmedia_aud_dev_cap cap,
				  void *pval)
{
    struct mda_stream *strm = (struct mda_stream*)s;
    pj_status_t status = PJ_ENOTSUP;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    switch (cap) {
    case PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING:
	if (strm->param.dir & PJMEDIA_DIR_CAPTURE) {
	    PJ_ASSERT_RETURN(strm->in_engine, PJ_EINVAL);
	    
	    TInt max_gain = strm->in_engine->GetMaxGain();
	    TInt gain = strm->in_engine->GetGain();
	    
	    if (max_gain > 0 && gain >= 0) {
		*(unsigned*)pval = gain * 100 / max_gain; 
		status = PJ_SUCCESS;
	    } else {
		status = PJMEDIA_EAUD_NOTREADY;
	    }
	}
	break;
    case PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING:
	if (strm->param.dir & PJMEDIA_DIR_PLAYBACK) {
	    PJ_ASSERT_RETURN(strm->out_engine, PJ_EINVAL);
	    
	    TInt max_vol = strm->out_engine->GetMaxVolume();
	    TInt vol = strm->out_engine->GetVolume();
	    
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
    struct mda_stream *strm = (struct mda_stream*)s;
    pj_status_t status = PJ_ENOTSUP;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    switch (cap) {
    case PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING:
	if (strm->param.dir & PJMEDIA_DIR_CAPTURE) {
	    PJ_ASSERT_RETURN(strm->in_engine, PJ_EINVAL);
	    
	    TInt max_gain = strm->in_engine->GetMaxGain();
	    if (max_gain > 0) {
		TInt gain;
		
		gain = *(unsigned*)pval * max_gain / 100;
		status = strm->in_engine->SetGain(gain);
	    } else {
		status = PJMEDIA_EAUD_NOTREADY;
	    }
	}
	break;
    case PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING:
	if (strm->param.dir & PJMEDIA_DIR_PLAYBACK) {
	    PJ_ASSERT_RETURN(strm->out_engine, PJ_EINVAL);
	    
	    TInt max_vol = strm->out_engine->GetMaxVolume();
	    if (max_vol > 0) {
		TInt vol;
		
		vol = *(unsigned*)pval * max_vol / 100;
		status = strm->out_engine->SetVolume(vol);
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

/* API: Start stream. */
static pj_status_t stream_start(pjmedia_aud_stream *strm)
{
    struct mda_stream *stream = (struct mda_stream*)strm;

    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if (stream->out_engine) {
	pj_status_t status;
    	status = stream->out_engine->StartPlay();
    	if (status != PJ_SUCCESS)
    	    return status;
    }
    
    if (stream->in_engine) {
	pj_status_t status;
    	status = stream->in_engine->StartRecord();
    	if (status != PJ_SUCCESS)
    	    return status;
    }

    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t stream_stop(pjmedia_aud_stream *strm)
{
    struct mda_stream *stream = (struct mda_stream*)strm;

    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if (stream->in_engine) {
    	stream->in_engine->Stop();
    }
    	
    if (stream->out_engine) {
    	stream->out_engine->Stop();
    }

    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t stream_destroy(pjmedia_aud_stream *strm)
{
    struct mda_stream *stream = (struct mda_stream*)strm;

    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    stream_stop(strm);

    delete stream->in_engine;
    stream->in_engine = NULL;

    delete stream->out_engine;
    stream->out_engine = NULL;

    pj_pool_t *pool;
    pool = stream->pool;
    if (pool) {
    	stream->pool = NULL;
    	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}

#endif /* PJMEDIA_AUDIO_DEV_HAS_SYMB_MDA */
