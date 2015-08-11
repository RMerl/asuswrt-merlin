/*
 * This file contains several sample settings especially for Windows
 * Mobile and Symbian targets. You can include this file in your
 * <pj/config_site.h> file.
 *
 * The Windows Mobile and Symbian settings will be activated
 * automatically if you include this file.
 *
 * In addition, you may specify one of these macros (before including
 * this file) to activate additional settings:
 *
 * #define PJ_CONFIG_NOKIA_APS_DIRECT
 *   Use this macro to activate the APS-Direct feature. Please see
 *   http://trac.pjsip.org/repos/wiki/Nokia_APS_VAS_Direct for more 
 *   info.
 *
 * #define PJ_CONFIG_WIN32_WMME_DIRECT
 *   Configuration to activate "APS-Direct" media mode on Windows or
 *   Windows Mobile, useful for testing purposes only.
 */


/*
 * Typical configuration for WinCE target.
 */
#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0

    /*
     * PJLIB settings.
     */

    /* Disable floating point support */
    #define PJ_HAS_FLOATING_POINT		0

    /*
     * PJMEDIA settings
     */

    /* Select codecs to disable */
    #define PJMEDIA_HAS_L16_CODEC		0
    #define PJMEDIA_HAS_ILBC_CODEC		0

    /* We probably need more buffers on WM, so increase the limit */
    #define PJMEDIA_SOUND_BUFFER_COUNT		32

    /* Fine tune Speex's default settings for best performance/quality */
    #define PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY	5

    /* For CPU reason, disable speex AEC and use the echo suppressor. */
    #define PJMEDIA_HAS_SPEEX_AEC		0

    /* Previously, resampling is disabled due to performance reason and
     * this condition prevented some 'light' wideband codecs (e.g: G722.1)
     * to work along with narrowband codecs. Lately, some tests showed
     * that 16kHz <-> 8kHz resampling using libresample small filter was 
     * affordable on ARM9 260 MHz, so here we decided to enable resampling.
     * Note that it is important to make sure that libresample is created
     * using small filter. For example PJSUA_DEFAULT_CODEC_QUALITY must
     * be set to 3 or 4 so pjsua-lib will apply small filter resampling.
     */
    //#define PJMEDIA_RESAMPLE_IMP		PJMEDIA_RESAMPLE_NONE
    #define PJMEDIA_RESAMPLE_IMP		PJMEDIA_RESAMPLE_LIBRESAMPLE

    /* Use the lighter WSOLA implementation */
    #define PJMEDIA_WSOLA_IMP			PJMEDIA_WSOLA_IMP_WSOLA_LITE

    /*
     * PJSIP settings.
     */

    /* Set maximum number of dialog/transaction/calls to minimum to reduce
     * memory usage 
     */
    #define PJSIP_MAX_TSX_COUNT 		31
    #define PJSIP_MAX_DIALOG_COUNT 		31
    #define PJSUA_MAX_CALLS			15

    /*
     * PJSUA settings
     */

    /* Default codec quality, previously was set to 5, however it is now
     * set to 4 to make sure pjsua instantiates resampler with small filter.
     */
    #define PJSUA_DEFAULT_CODEC_QUALITY		4

    /* Set maximum number of objects to minimum to reduce memory usage */
    #define PJSUA_MAX_ACC			4
    #define PJSUA_MAX_PLAYERS			4
    #define PJSUA_MAX_RECORDERS			4
    #define PJSUA_MAX_CONF_PORTS		(PJSUA_MAX_CALLS+2*PJSUA_MAX_PLAYERS)
    #define PJSUA_MAX_BUDDIES			32

#endif	/* PJ_WIN32_WINCE */


/*
 * Typical configuration for Symbian OS target
 */
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN!=0

    /*
     * PJLIB settings.
     */

    /* Disable floating point support */
    #define PJ_HAS_FLOATING_POINT		0

    /* Misc PJLIB setting */
    #define PJ_MAXPATH				80

    /* This is important for Symbian. Symbian lacks vsnprintf(), so
     * if the log buffer is not long enough it's possible that
     * large incoming packet will corrupt memory when the log tries
     * to log the packet.
     */
    #define PJ_LOG_MAX_SIZE			(PJSIP_MAX_PKT_LEN+500)

    /* Since we don't have threads, log buffer can use static buffer
     * rather than stack
     */
    #define PJ_LOG_USE_STACK_BUFFER		0

    /* Disable check stack since it increases footprint */
    #define PJ_OS_HAS_CHECK_STACK		0


    /*
     * PJMEDIA settings
     */

    /* Disable non-Symbian audio devices */
    #define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO	0
    #define PJMEDIA_AUDIO_DEV_HAS_WMME		0

    /* Select codecs to disable */
    #define PJMEDIA_HAS_L16_CODEC		0
    #define PJMEDIA_HAS_ILBC_CODEC		0
    #define PJMEDIA_HAS_G722_CODEC		0

    /* Fine tune Speex's default settings for best performance/quality */
    #define PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY	5

    /* For CPU reason, disable speex AEC and use the echo suppressor. */
    #define PJMEDIA_HAS_SPEEX_AEC		0

    /* Previously, resampling is disabled due to performance reason and
     * this condition prevented some 'light' wideband codecs (e.g: G722.1)
     * to work along with narrowband codecs. Lately, some tests showed
     * that 16kHz <-> 8kHz resampling using libresample small filter was 
     * affordable on ARM9 222 MHz, so here we decided to enable resampling.
     * Note that it is important to make sure that libresample is created
     * using small filter. For example PJSUA_DEFAULT_CODEC_QUALITY must
     * be set to 3 or 4 so pjsua-lib will apply small filter resampling.
     */
    //#define PJMEDIA_RESAMPLE_IMP		PJMEDIA_RESAMPLE_NONE
    #define PJMEDIA_RESAMPLE_IMP		PJMEDIA_RESAMPLE_LIBRESAMPLE

    /* Use the lighter WSOLA implementation */
    #define PJMEDIA_WSOLA_IMP			PJMEDIA_WSOLA_IMP_WSOLA_LITE

    /* We probably need more buffers especially if MDA audio backend 
     * is used, so increase the limit 
     */
    #define PJMEDIA_SOUND_BUFFER_COUNT		32

    /*
     * PJSIP settings.
     */

    /* Disable safe module access, since we don't use multithreading */
    #define PJSIP_SAFE_MODULE			0

    /* Use large enough packet size  */
    #define PJSIP_MAX_PKT_LEN			2000

    /* Symbian has problem with too many large blocks */
    #define PJSIP_POOL_LEN_ENDPT		1000
    #define PJSIP_POOL_INC_ENDPT		1000
    #define PJSIP_POOL_RDATA_LEN		2000
    #define PJSIP_POOL_RDATA_INC		2000
    #define PJSIP_POOL_LEN_TDATA		2000
    #define PJSIP_POOL_INC_TDATA		512
    #define PJSIP_POOL_LEN_UA			2000
    #define PJSIP_POOL_INC_UA			1000
    #define PJSIP_POOL_TSX_LAYER_LEN		256
    #define PJSIP_POOL_TSX_LAYER_INC		256
    #define PJSIP_POOL_TSX_LEN			512
    #define PJSIP_POOL_TSX_INC			128

    /*
     * PJSUA settings.
     */

    /* Default codec quality, previously was set to 5, however it is now
     * set to 4 to make sure pjsua instantiates resampler with small filter.
     */
    #define PJSUA_DEFAULT_CODEC_QUALITY		4

    /* Set maximum number of dialog/transaction/calls to minimum */
    #define PJSIP_MAX_TSX_COUNT 		31
    #define PJSIP_MAX_DIALOG_COUNT 		31
    #define PJSUA_MAX_CALLS			15

    /* Other pjsua settings */
    #define PJSUA_MAX_ACC			4
    #define PJSUA_MAX_PLAYERS			4
    #define PJSUA_MAX_RECORDERS			4
    #define PJSUA_MAX_CONF_PORTS		(PJSUA_MAX_CALLS+2*PJSUA_MAX_PLAYERS)
    #define PJSUA_MAX_BUDDIES			32
#endif


/*
 * Additional configuration to activate APS-Direct feature for
 * Nokia S60 target
 *
 * Please see http://trac.pjsip.org/repos/wiki/Nokia_APS_VAS_Direct
 */
#ifdef PJ_CONFIG_NOKIA_APS_DIRECT

    /* MUST use switchboard rather than the conference bridge */
    #define PJMEDIA_CONF_USE_SWITCH_BOARD	1

    /* Enable APS sound device backend and disable MDA & VAS */
    #define PJMEDIA_AUDIO_DEV_HAS_SYMB_MDA	0
    #define PJMEDIA_AUDIO_DEV_HAS_SYMB_APS	1
    #define PJMEDIA_AUDIO_DEV_HAS_SYMB_VAS	0

    /* Enable passthrough codec framework */
    #define PJMEDIA_HAS_PASSTHROUGH_CODECS	1

    /* And selectively enable which codecs are supported by the handset */
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU	1
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA	1
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR	1
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_G729	1
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC	1

#endif


/*
 * Additional configuration to activate VAS-Direct feature for
 * Nokia S60 target
 *
 * Please see http://trac.pjsip.org/repos/wiki/Nokia_APS_VAS_Direct
 */
#ifdef PJ_CONFIG_NOKIA_VAS_DIRECT

    /* MUST use switchboard rather than the conference bridge */
    #define PJMEDIA_CONF_USE_SWITCH_BOARD	1

    /* Enable VAS sound device backend and disable MDA & APS */
    #define PJMEDIA_AUDIO_DEV_HAS_SYMB_MDA	0
    #define PJMEDIA_AUDIO_DEV_HAS_SYMB_APS	0
    #define PJMEDIA_AUDIO_DEV_HAS_SYMB_VAS	1

    /* Enable passthrough codec framework */
    #define PJMEDIA_HAS_PASSTHROUGH_CODECS	1

    /* And selectively enable which codecs are supported by the handset */
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU	1
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA	1
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR	1
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_G729	1
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC	1

#endif


/*
 * Configuration to activate "APS-Direct" media mode on Windows,
 * useful for testing purposes only.
 */
#ifdef PJ_CONFIG_WIN32_WMME_DIRECT

    /* MUST use switchboard rather than the conference bridge */
    #define PJMEDIA_CONF_USE_SWITCH_BOARD	1

    /* Only WMME supports the "direct" feature */
    #define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO	0
    #define PJMEDIA_AUDIO_DEV_HAS_WMME		1

    /* Enable passthrough codec framework */
    #define PJMEDIA_HAS_PASSTHROUGH_CODECS	1

    /* Only PCMA and PCMU are supported by WMME-direct */
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU	1
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA	1
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR	0
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_G729	0
    #define PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC	0

#endif

/*
 * iPhone sample settings.
 */
#if PJ_CONFIG_IPHONE
    /*
     * PJLIB settings.
     */

    /* Disable floating point support */
    #define PJ_HAS_FLOATING_POINT		0

    /*
     * PJMEDIA settings
     */

    /* We have our own native CoreAudio backend */
    #define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO	0
    #define PJMEDIA_AUDIO_DEV_HAS_WMME		0
    #define PJMEDIA_AUDIO_DEV_HAS_COREAUDIO	1

    /* The CoreAudio backend has built-in echo canceller! */
    #define PJMEDIA_HAS_SPEEX_AEC    0

    /* Disable some codecs */
    #define PJMEDIA_HAS_L16_CODEC		0
    #define PJMEDIA_HAS_G722_CODEC		0

    /* Use the built-in CoreAudio's iLBC codec (yay!) */
    #define PJMEDIA_HAS_ILBC_CODEC		1
    #define PJMEDIA_ILBC_CODEC_USE_COREAUDIO	1

    /* Fine tune Speex's default settings for best performance/quality */
    #define PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY	5

    /*
     * PJSIP settings.
     */

    /* Increase allowable packet size, just in case */
    //#define PJSIP_MAX_PKT_LEN			2000

    /*
     * PJSUA settings.
     */

    /* Default codec quality, previously was set to 5, however it is now
     * set to 4 to make sure pjsua instantiates resampler with small filter.
     */
    #define PJSUA_DEFAULT_CODEC_QUALITY		4

    /* Set maximum number of dialog/transaction/calls to minimum */
    #define PJSIP_MAX_TSX_COUNT 		31
    #define PJSIP_MAX_DIALOG_COUNT 		31
    #define PJSUA_MAX_CALLS			15

    /* Other pjsua settings */
    #define PJSUA_MAX_ACC			4
    #define PJSUA_MAX_PLAYERS			4
    #define PJSUA_MAX_RECORDERS			4
    #define PJSUA_MAX_CONF_PORTS		(PJSUA_MAX_CALLS+2*PJSUA_MAX_PLAYERS)
    #define PJSUA_MAX_BUDDIES			32

#endif

/*
 * Minimum size
 */
#ifdef PJ_CONFIG_MINIMAL_SIZE

#   undef PJ_OS_HAS_CHECK_STACK
#   define PJ_OS_HAS_CHECK_STACK	0
#   define PJ_LOG_MAX_LEVEL		0
#   define PJ_ENABLE_EXTRA_CHECK	0
#   define PJ_HAS_ERROR_STRING		0
#   undef PJ_IOQUEUE_MAX_HANDLES
/* Putting max handles to lower than 32 will make pj_fd_set_t size smaller
 * than native fdset_t and will trigger assertion on sock_select.c.
 */
#   define PJ_IOQUEUE_MAX_HANDLES	32
#   define PJ_CRC32_HAS_TABLES		0
#   define PJSIP_MAX_TSX_COUNT		15
#   define PJSIP_MAX_DIALOG_COUNT	15
#   define PJSIP_UDP_SO_SNDBUF_SIZE	4000
#   define PJSIP_UDP_SO_RCVBUF_SIZE	4000
#   define PJMEDIA_HAS_ALAW_ULAW_TABLE	0

#elif defined(PJ_CONFIG_MAXIMUM_SPEED)
#   define PJ_SCANNER_USE_BITWISE	0
#   undef PJ_OS_HAS_CHECK_STACK
#   define PJ_OS_HAS_CHECK_STACK	0
#   define PJ_LOG_MAX_LEVEL		3
#   define PJ_ENABLE_EXTRA_CHECK	0
#   define PJ_IOQUEUE_MAX_HANDLES	5000
#   define PJSIP_MAX_TSX_COUNT		((640*1024)-1)
#   define PJSIP_MAX_DIALOG_COUNT	((640*1024)-1)
#   define PJSIP_UDP_SO_SNDBUF_SIZE	(24*1024*1024)
#   define PJSIP_UDP_SO_RCVBUF_SIZE	(24*1024*1024)
#   define PJ_DEBUG			0
#   define PJSIP_SAFE_MODULE		0
#   define PJ_HAS_STRICMP_ALNUM		0
#   define PJ_HASH_USE_OWN_TOLOWER	1
#   define PJSIP_UNESCAPE_IN_PLACE	1

#   ifdef PJ_WIN32
#     define PJSIP_MAX_NET_EVENTS	10
#   endif

#   define PJSUA_MAX_CALLS		15

#endif

