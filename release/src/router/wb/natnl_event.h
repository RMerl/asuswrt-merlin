
#ifndef __NATNL_EVENT_H__
#define __NATNL_EVENT_H__

/**
 * This enumeration describes invite session state.
 */
typedef enum natnl_event
{
    NATNL_INV_EVENT_NULL,			/**< Before INVITE is sent or received  */
    NATNL_INV_EVENT_CALLING,	    /**< After INVITE is sent		    */
    NATNL_INV_EVENT_INCOMING,	    /**< After INVITE is received.	    */
    NATNL_INV_EVENT_EARLY,			/**< After response with To tag.	    */
    NATNL_INV_EVENT_CONNECTING,	    /**< After 2xx is sent/received.	    */
    NATNL_INV_EVENT_CONFIRMED,	    /**< After ACK is sent/received.	    */
    NATNL_INV_EVENT_DISCONNECTED,   /**< Session is terminated.		    */

	NATNL_SIP_EVENT_REG = 60000,	/**< NATNL sip registration (login sip server) state*/
	NATNL_SIP_EVENT_UNREG,			/**< NATNL sip unregistration (logout sip server) state*/
	
	NATNL_TNL_EVENT_KA_TIMEOUT,     /**< NATNL tunnel or STUN keep-alvie timeout*/
	NATNL_TNL_EVENT_START,			/**< NATNL tunnel start*/
	NATNL_TNL_EVENT_STOP,           /**< NATNL tunnel stop*/

	NATNL_TNL_EVENT_NAT_TYPE_DETECTED, /**< since v1.1.0.6 NATNL NAT type detect completely*/
	
	NATNL_TNL_EVENT_INIT_OK			= 60100,	/**< since v1.1.0.11 NATNL init successfully */
	NATNL_TNL_EVENT_INIT_FAILED		= 60101,	/**< since v1.1.0.11 NATNL init failed */
	NATNL_TNL_EVENT_DEINIT_OK		= 60102,	/**< since v1.1.0.11 NATNL deinit successfully */
	NATNL_TNL_EVENT_DEINIT_FAILED	= 60103,	/**< since v1.1.0.11 NATNL deinit failed */
	NATNL_TNL_EVENT_MAKECALL_OK		= 60104,	/**< since v1.1.0.11 NATNL make call successfully */
	NATNL_TNL_EVENT_MAKECALL_FAILED	= 60105,	/**< since v1.1.0.11 NATNL make call failed */
	NATNL_TNL_EVENT_HANGUP_OK		= 60106,	/**< since v1.1.0.11 NATNL hangup ok. */
	NATNL_TNL_EVENT_HANGUP_FAILED	= 60107,	/**< since v1.1.0.11 NATNL hangup failed */
	NATNL_TNL_EVENT_IP_CHANGED		= 60108,	/**< since v1.2.8.0  NATNL IP changed */
	NATNL_TNL_EVENT_REG_OK			= 60109,	/**< since v1.3.2.0  NATNL register device successfully */
	NATNL_TNL_EVENT_REG_FAILED		= 60110,	/**< since v1.3.2.0  NATNL register device failed */
	NATNL_TNL_EVENT_UNREG_OK		= 60111,	/**< since v1.3.2.0  NATNL un-register device successfully */
	NATNL_TNL_EVENT_UNREG_FAILED	= 60112,	/**< since v1.3.2.0  NATNL un-register device failed */
	NATNL_TNL_EVENT_IDLE_TIMEOUT	= 60113,	/**< since v1.6.1.2  NATNL idle timeout */
	//NATNL_TNL_EVENT_CFG_UPDATE_OK	= 60113,	/**< since v1.3.3.0  NATNL un-register device successfully */
	//NATNL_TNL_EVENT_CFG_UPDATE_FAILED= 60114	/**< since v1.3.3.0  NATNL un-register device failed */

} natnl_event;


/**
 * This enumeration lists constst of standard SIP status codes according to RFC 3261 and 
 * asusnatnl status code.
 * In addition, it also declares new status class 7xx for errors generated
 * by the stack. This status class however should not get transmitted on the 
 * wire.
 */
typedef enum natnl_status_code
{
	NATNL_SC_TNL_OK = 0,  // asusnatnl status, everthing is OK


    PJ_SC_TRYING = 100,
    PJ_SC_RINGING = 180,
    PJ_SC_CALL_BEING_FORWARDED = 181,
    PJ_SC_QUEUED = 182,
    PJ_SC_PROGRESS = 183,

    PJ_SC_OK = 200,
    PJ_SC_ACCEPTED = 202,

    PJ_SC_MULTIPLE_CHOICES = 300,
    PJ_SC_MOVED_PERMANENTLY = 301,
    PJ_SC_MOVED_TEMPORARILY = 302,
    PJ_SC_USE_PROXY = 305,
    PJ_SC_ALTERNATIVE_SERVICE = 380,

    PJ_SC_BAD_REQUEST = 400,
    PJ_SC_UNAUTHORIZED = 401,
    PJ_SC_PAYMENT_REQUIRED = 402,
    PJ_SC_FORBIDDEN = 403,
    PJ_SC_NOT_FOUND = 404,
    PJ_SC_METHOD_NOT_ALLOWED = 405,
    PJ_SC_NOT_ACCEPTABLE = 406,
    PJ_SC_PROXY_AUTHENTICATION_REQUIRED = 407,
    PJ_SC_REQUEST_TIMEOUT = 408,
    PJ_SC_GONE = 410,
    PJ_SC_REQUEST_ENTITY_TOO_LARGE = 413,
    PJ_SC_REQUEST_URI_TOO_LONG = 414,
    PJ_SC_UNSUPPORTED_MEDIA_TYPE = 415,
    PJ_SC_UNSUPPORTED_URI_SCHEME = 416,
    PJ_SC_BAD_EXTENSION = 420,
    PJ_SC_EXTENSION_REQUIRED = 421,
    PJ_SC_SESSION_TIMER_TOO_SMALL = 422,
    PJ_SC_INTERVAL_TOO_BRIEF = 423,
    PJ_SC_TEMPORARILY_UNAVAILABLE = 480,
    PJ_SC_CALL_TSX_DOES_NOT_EXIST = 481,
    PJ_SC_LOOP_DETECTED = 482,
    PJ_SC_TOO_MANY_HOPS = 483,
    PJ_SC_ADDRESS_INCOMPLETE = 484,
    PJ_AC_AMBIGUOUS = 485,
    PJ_SC_BUSY_HERE = 486,
    PJ_SC_REQUEST_TERMINATED = 487,
    PJ_SC_NOT_ACCEPTABLE_HERE = 488,
    PJ_SC_BAD_EVENT = 489,
    PJ_SC_REQUEST_UPDATED = 490,
    PJ_SC_REQUEST_PENDING = 491,
    PJ_SC_UNDECIPHERABLE = 493,

    PJ_SC_INTERNAL_SERVER_ERROR = 500,
    PJ_SC_NOT_IMPLEMENTED = 501,
    PJ_SC_BAD_GATEWAY = 502,
    PJ_SC_SERVICE_UNAVAILABLE = 503,
    PJ_SC_SERVER_TIMEOUT = 504,
    PJ_SC_VERSION_NOT_SUPPORTED = 505,
    PJ_SC_MESSAGE_TOO_LARGE = 513,
    PJ_SC_PRECONDITION_FAILURE = 580,

    PJ_SC_BUSY_EVERYWHERE = 600,
    PJ_SC_DECLINE = 603,
    PJ_SC_DOES_NOT_EXIST_ANYWHERE = 604,
    PJ_SC_NOT_ACCEPTABLE_ANYWHERE = 606,

    PJ_SC_TSX_TIMEOUT = PJ_SC_REQUEST_TIMEOUT,
    /*PJ_SC_TSX_RESOLVE_ERROR = 702,*/
    PJ_SC_TSX_TRANSPORT_ERROR = PJ_SC_SERVICE_UNAVAILABLE,

#if defined(WIN32) && !defined(NATNL_LIB)
	PJ_EUNKNOWN	=	70001	,	//	"Unknown Error"				
	PJ_EPENDING  	=	70002	,	//	"Pending operation"				
	PJ_ETOOMANYCONN  	=	70003	,	//	"Too many connecting sockets"				
	PJ_EINVAL	=	70004	,	//	"Invalid value or argument"				
	PJ_ENAMETOOLONG  	=	70005	,	//	"Name too long"				
	PJ_ENOTFOUND 	=	70006	,	//	"Not found"				
	PJ_ENOMEM	=	70007	,	//	"Not enough memory"				
	PJ_EBUG  	=	70008	,	//	"BUG DETECTED!"				
	PJ_ETIMEDOUT 	=	70009	,	//	"Operation timed out"				
	PJ_ETOOMANY  	=	70010	,	//	"Too many objects of the specified type"				
	PJ_EBUSY 	=	70011	,	//	"Object is busy"				
	PJ_ENOTSUP	=	70012	,	//	"Option/operation is not supported"				
	PJ_EINVALIDOP	=	70013	,	//	"Invalid operation"				
	PJ_ECANCELLED	=	70014	,	//	"Operation cancelled"				
	PJ_EEXISTS  	=	70015	,	//	"Object already exists"				
	PJ_EEOF	=	70016	,	//	"End of file"				
	PJ_ETOOBIG	=	70017	,	//	"Size is too big"				
	PJ_ERESOLVE	=	70018	,	//	"gethostbyname( has returned error"				
	PJ_ETOOSMALL	=	70019	,	//	"Size is too short"				
	PJ_EIGNORED	=	70020	,	//	"Ignored"				
	PJ_EIPV6NOTSUP   	=	70021	,	//	"IPv6 is not supported"				
	PJ_EAFNOTSUP	=	70022	,	//	"Unsupported address family"				
		/*	Generic PJMEDIA errors shouldn't be used!	*/					
	PJMEDIA_ERROR	=	220001	,	//	"Unspecified PJMEDIA error"				
		/*	SDP error	*/						
	PJMEDIA_SDP_EINSDP	=	220020	,	//	"Invalid SDP descriptor"				
	PJMEDIA_SDP_EINVER	=	220021	,	//	"Invalid SDP version line"				
	PJMEDIA_SDP_EINORIGIN   	=	220022	,	//	"Invalid SDP origin line"				
	PJMEDIA_SDP_EINTIME	=	220023	,	//	"Invalid SDP time line"				
	PJMEDIA_SDP_EINNAME	=	220024	,	//	"SDP name/subject line is empty"				
	PJMEDIA_SDP_EINCONN	=	220025	,	//	"Invalid SDP connection line"				
	PJMEDIA_SDP_EMISSINGCONN 	=	220026	,	//	"Missing SDP connection info line"				
	PJMEDIA_SDP_EINATTR	=	220027	,	//	"Invalid SDP attributes"				
	PJMEDIA_SDP_EINRTPMAP	=	220028	,	//	"Invalid SDP rtpmap attribute"				
	PJMEDIA_SDP_ERTPMAPTOOLONG	=	220029	,	//	"SDP rtpmap attribute too long"				
	PJMEDIA_SDP_EMISSINGRTPMAP	=	220030	,	//	"Missing SDP rtpmap for dynamic payload type"				
	PJMEDIA_SDP_EINMEDIA	=	220031	,	//	"Invalid SDP media line"				
	PJMEDIA_SDP_ENOFMT	=	220032	,	//	"No SDP payload format in the media line"				
	PJMEDIA_SDP_EINPT	=	220033	,	//	"Invalid SDP payload type in media line"				
	PJMEDIA_SDP_EINFMTP	=	220034	,	//	"Invalid SDP fmtp attribute"				
	PJMEDIA_SDP_EINRTCP	=	220035	,	//	"Invalid SDP rtcp attribyte"				
	PJMEDIA_SDP_EINPROTO	=	220036	,	//	"Invalid SDP media transport protocol"				
		/*	SDP negotiator errors	*/						
	PJMEDIA_SDPNEG_EINSTATE	=	220040	,	//	Invalid SDP negotiator state for operation				
	PJMEDIA_SDPNEG_ENOINITIAL	=	220041	,	//	No initial local SDP in SDP negotiator				
	PJMEDIA_SDPNEG_ENOACTIVE	=	220042	,	//	No active SDP in SDP negotiator				
	PJMEDIA_SDPNEG_ENONEG	=	220043	,	//	No current local/remote offer/answer				
	PJMEDIA_SDPNEG_EMISMEDIA	=	220044	,	//	SDP media count mismatch in offer/answer				
	PJMEDIA_SDPNEG_EINVANSMEDIA	=	220045	,	//	SDP media type mismatch in offer/answer				
	PJMEDIA_SDPNEG_EINVANSTP	=	220046	,	//	SDP media transport type mismatch in offer/answer				
	PJMEDIA_SDPNEG_EANSNOMEDIA	=	220047	,	//	No common SDP media payload in answer				
	PJMEDIA_SDPNEG_ENOMEDIA	=	220048	,	//	No active media stream after negotiation				
	PJMEDIA_SDPNEG_NOANSCODEC	=	220049	,	//	No suitable codec for remote offer				
	PJMEDIA_SDPNEG_NOANSTELEVENT	=	220050	,	//	No suitable telephone-event for remote offer				
	PJMEDIA_SDPNEG_NOANSUNKNOWN	=	220051	,	//	No suitable answer for unknown remote offer				
		/*	SDP comparison results	*/						
	PJMEDIA_SDP_EMEDIANOTEQUAL  	=	220060	,	//	"SDP media descriptor not equal"				
	PJMEDIA_SDP_EPORTNOTEQUAL   	=	220061	,	//	"Port in SDP media descriptor not equal"				
	PJMEDIA_SDP_ETPORTNOTEQUAL   	=	220062	,	//	"Transport in SDP media descriptor not equal"				
	PJMEDIA_SDP_EFORMATNOTEQUAL  	=	220063	,	//	"Format in SDP media descriptor not equal"				
	PJMEDIA_SDP_ECONNNOTEQUAL   	=	220064	,	//	"SDP connection line not equal"				
	PJMEDIA_SDP_EATTRNOTEQUAL	=	220065	,	//	"SDP attributes not equal"				
	PJMEDIA_SDP_EDIRNOTEQUAL 	=	220066	,	//	"SDP media direction not equal"				
	PJMEDIA_SDP_EFMTPNOTEQUAL	=	220067	,	//	"SDP fmtp attribute not equal"				
	PJMEDIA_SDP_ERTPMAPNOTEQUAL  	=	220068	,	//	"SDP rtpmap attribute not equal"				
	PJMEDIA_SDP_ESESSNOTEQUAL   	=	220069	,	//	 "SDP session descriptor not equal"				
	PJMEDIA_SDP_EORIGINNOTEQUAL  	=	220070	,	//	"SDP origin line not equal"				
	PJMEDIA_SDP_ENAMENOTEQUAL	=	220071	,	//	"SDP name/subject line not equal"				
	PJMEDIA_SDP_ETIMENOTEQUAL	=	220072	,	//	"SDP time line not equal"				
		/*	Codec errors	*/						
	PJMEDIA_CODEC_EUNSUP	=	220080	,	//	Unsupported media codec				
	PJMEDIA_CODEC_EFAILED	=	220081	,	//	Codec internal creation error				
	PJMEDIA_CODEC_EFRMTOOSHORT   	=	220082	,	//	"Codec frame is too short"				
	PJMEDIA_CODEC_EPCMTOOSHORT   	=	220083	,	//	"PCM frame is too short"				
	PJMEDIA_CODEC_EFRMINLEN  	=	220084	,	//	"Invalid codec frame length"				
	PJMEDIA_CODEC_EPCMFRMINLEN   	=	220085	,	//	"Invalid PCM frame length"				
	PJMEDIA_CODEC_EINMODE	=	220086	,	//	Invalid codec mode (no fmtp)?				
		/*	Media errors	*/						
	PJMEDIA_EINVALIDIP	=	220100	,	//	"Invalid remote media (IP address"				
	PJMEDIA_EASYMCODEC	=	220101	,	//	"Asymetric media codec is not supported"				
	PJMEDIA_EINVALIDPT	=	220102	,	//	"Invalid media payload type"				
	PJMEDIA_EMISSINGRTPMAP   	=	220103	,	//	"Missing rtpmap in media description"				
	PJMEDIA_EINVALIMEDIATYPE 	=	220104	,	//	"Invalid media type"				
	PJMEDIA_EREMOTENODTMF	=	220105	,	//	"Remote does not support DTMF"				
	PJMEDIA_RTP_EINDTMF	=	220106	,	//	"Invalid DTMF digit"				
	PJMEDIA_RTP_EREMNORFC2833	=	220107	,	//	"Remote does not support RFC 2833"				
		/*	 RTP session errors	*/						
	PJMEDIA_RTP_EINPKT	=	220120	,	//	"Invalid RTP packet"				
	PJMEDIA_RTP_EINPACK	=	220121	,	//	"Invalid RTP packing (internal error)"				
	PJMEDIA_RTP_EINVER	=	220122	,	//	"Invalid RTP version"				
	PJMEDIA_RTP_EINSSRC	=	220123	,	//	"RTP packet SSRC id mismatch"				
	PJMEDIA_RTP_EINPT	=	220124	,	//	"RTP packet payload type mismatch"				
	PJMEDIA_RTP_EINLEN	=	220125	,	//	"Invalid RTP packet length"				
	PJMEDIA_RTP_ESESSRESTART	=	220130	,	//	"RTP session restarted"				
	PJMEDIA_RTP_ESESSPROBATION  	=	220131	,	//	"RTP session in probation"				
	PJMEDIA_RTP_EBADSEQ	=	220132	,	//	"Bad sequence number in RTP packet"				
	PJMEDIA_RTP_EBADDEST	=	220133	,	//	"RTP media port destination is not configured"				
	PJMEDIA_RTP_ENOCONFIG	=	220134	,	//	"RTP is not configured"				
		/*	Media port errors	*/						
	PJMEDIA_ENOTCOMPATIBLE  	=	220160	,	//	"Media ports are not compatible"				
	PJMEDIA_ENCCLOCKRATE	=	220161	,	//	"Media ports have incompatible clock rate"				
	PJMEDIA_ENCSAMPLESPFRAME	=	220162	,	//	"Media ports have incompatible samples per frame"				
	PJMEDIA_ENCTYPE	=	220163	,	//	"Media ports have incompatible media type"				
	PJMEDIA_ENCBITS	=	220164	,	//	"Media ports have incompatible bits per sample"				
	PJMEDIA_ENCBYTES	=	220165	,	//	"Media ports have incompatible bytes per frame"				
	PJMEDIA_ENCCHANNEL	=	220166	,	//	"Media ports have incompatible number of channels"				
		/*	Media file errors	*/						
	PJMEDIA_ENOTVALIDWAVE	=	220180	,	//	"Not a valid WAVE file"				
	PJMEDIA_EWAVEUNSUPP	=	220181	,	//	"Unsupported WAVE file format"				
	PJMEDIA_EWAVETOOSHORT	=	220182	,	//	"WAVE file too short"				
	PJMEDIA_EFRMFILETOOBIG   	=	220183	,	//	"Sound frame too large for file buffer"				
		/*	Sound device errors	*/						
	PJMEDIA_ENOSNDREC	=	220200	,	//	"No suitable sound capture device"				
	PJMEDIA_ENOSNDPLAY	=	220201	,	//	"No suitable sound playback device"				
	PJMEDIA_ESNDINDEVID	=	220202	,	//	"Invalid sound device ID"				
	PJMEDIA_ESNDINSAMPLEFMT  	=	220203	,	//	"Invalid sample format for sound device"				
	PJSIP_SIMPLE_ENOPKG	=	270001	,	//	"No SIP event package with the specified name"				
	PJSIP_SIMPLE_EPKGEXISTS	=	270002	,	//	"SIP event package already exist"				
		/*	Presence errors	*/						
	PJSIP_SIMPLE_ENOTSUBSCRIBE   	=	270020	,	//	"Expecting SUBSCRIBE request"				
	PJSIP_SIMPLE_ENOPRESENCE	=	270021	,	//	"No presence associated with the subscription"				
	PJSIP_SIMPLE_ENOPRESENCEINFO 	=	270022	,	//	"No presence info in the server subscription"				
	PJSIP_SIMPLE_EBADCONTENT	=	270023	,	//	"Bad Content-Type for presence"				
	PJSIP_SIMPLE_EBADPIDF	=	270024	,	//	"Bad PIDF content for presence"				
	PJSIP_SIMPLE_EBADXPIDF	=	270025	,	//	"Bad XPIDF content for presence"				
	PJSIP_SIMPLE_EBADRPID	=	270026	,	//	"Invalid or bad RPID document"				
		/*	 isComposing errors	*/						
	PJSIP_SIMPLE_EBADISCOMPOSE   	=	270040	,	//	"Bad isComposing indication/XML message"				
		/*	STUN errors	*/						
	PJLIB_UTIL_ESTUNRESOLVE	=	320001	,	//	Unable to resolve STUN server				
	PJLIB_UTIL_ESTUNINMSGTYPE	=	320002	,	//	Unknown STUN message type				
	PJLIB_UTIL_ESTUNINMSGLEN	=	320003	,	//	Invalid STUN message length				
	PJLIB_UTIL_ESTUNINATTRLEN	=	320004	,	//	STUN attribute length error				
	PJLIB_UTIL_ESTUNINATTRTYPE	=	320005	,	//	Invalid STUN attribute type				
	PJLIB_UTIL_ESTUNININDEX	=	320006	,	//	Invalid STUN server/socket index				
	PJLIB_UTIL_ESTUNNOBINDRES	=	320007	,	//	No STUN binding response in the message				
	PJLIB_UTIL_ESTUNRECVERRATTR	=	320008	,	//	Received STUN error attribute				
	PJLIB_UTIL_ESTUNNOMAP	=	320009	,	//	No STUN mapped address attribute				
	PJLIB_UTIL_ESTUNNOTRESPOND	=	320010	,	//	Received no response from STUN server				
	PJLIB_UTIL_ESTUNSYMMETRIC	=	320011	,	//	Symetric NAT detected by STUN				
		/*	XML errors	*/						
	PJLIB_UTIL_EINXML	=	320020	,	//	Invalid XML message				
		/*	DNS errors	*/						
	PJLIB_UTIL_EDNSQRYTOOSMALL	=	320040	,	//	DNS query packet buffer is too small				
	PJLIB_UTIL_EDNSINSIZE	=	320041	,	//	Invalid DNS packet length				
	PJLIB_UTIL_EDNSINCLASS	=	320042	,	//	Invalid DNS class				
	PJLIB_UTIL_EDNSINNAMEPTR	=	320043	,	//	Invalid DNS name pointer				
	PJLIB_UTIL_EDNSINNSADDR	=	320044	,	//	Invalid DNS nameserver address				
	PJLIB_UTIL_EDNSNONS	=	320045	,	//	No nameserver is in DNS resolver				
	PJLIB_UTIL_EDNSNOWORKINGNS	=	320046	,	//	No working DNS nameserver				
	PJLIB_UTIL_EDNSNOANSWERREC	=	320047	,	//	No answer record in the DNS response				
	PJLIB_UTIL_EDNSINANSWER	=	320048	,	//	Invalid DNS answer				
	PJLIB_UTIL_EDNS_FORMERR	=	320051	,	//	DNS \Format error\""				
	PJLIB_UTIL_EDNS_SERVFAIL	=	320052	,	//	DNS \Server failure\""				
	PJLIB_UTIL_EDNS_NXDOMAIN	=	320053	,	//	DNS \Name Error\""				
	PJLIB_UTIL_EDNS_NOTIMPL	=	320054	,	//	DNS \Not Implemented\""				
	PJLIB_UTIL_EDNS_REFUSED	=	320055	,	//	DNS \Refused\""				
	PJLIB_UTIL_EDNS_YXDOMAIN	=	320056	,	//	DNS \The name exists\""				
	PJLIB_UTIL_EDNS_YXRRSET	=	320057	,	//	DNS \The RRset (name type exists\""				
	PJLIB_UTIL_EDNS_NXRRSET	=	320058	,	//	DNS \The RRset (name type does not exist\""				
	PJLIB_UTIL_EDNS_NOTAUTH	=	320059	,	//	DNS \Not authorized\""				
	PJLIB_UTIL_EDNS_NOTZONE	=	320060	,	//	DNS \The zone specified is not a zone\""				
		/*	STUN errors	*/						
	PJLIB_UTIL_ESTUNTOOMANYATTR	=	320110	,	//	Too many STUN attributes				
	PJLIB_UTIL_ESTUNUNKNOWNATTR	=	320111	,	//	Unknown STUN attribute				
	PJLIB_UTIL_ESTUNINADDRLEN	=	320112	,	//	Invalid STUN socket address length				
	PJLIB_UTIL_ESTUNIPV6NOTSUPP	=	320113	,	//	STUN IPv6 attribute not supported				
	PJLIB_UTIL_ESTUNNOTRESPONSE	=	320114	,	//	Expecting STUN response message				
	PJLIB_UTIL_ESTUNINVALIDID	=	320115	,	//	STUN transaction ID mismatch				
	PJLIB_UTIL_ESTUNNOHANDLER	=	320116	,	//	Unable to find STUN handler for the request				
	PJLIB_UTIL_ESTUNMSGINTPOS	=	320118	,	//	Found non-FINGERPRINT attr. after MESSAGE-INTEGRITY				
	PJLIB_UTIL_ESTUNFINGERPOS	=	320119	,	//	Found STUN attribute after FINGERPRINT				
	PJLIB_UTIL_ESTUNNOUSERNAME	=	320120	,	//	Missing STUN USERNAME attribute				
	PJLIB_UTIL_ESTUNMSGINT	=	320122	,	//	Missing/invalid STUN MESSAGE-INTEGRITY attribute				
	PJLIB_UTIL_ESTUNDUPATTR	=	320123	,	//	Found duplicate STUN attribute				
	PJLIB_UTIL_ESTUNNOREALM	=	320124	,	//	Missing STUN REALM attribute				
	PJLIB_UTIL_ESTUNNONCE	=	320125	,	//	Missing/stale STUN NONCE attribute value				
	PJLIB_UTIL_ESTUNTSXFAILED	=	320126	,	//	STUN transaction terminates with failure				
		/*	HTTP Client	*/						
	PJLIB_UTIL_EHTTPINURL	=	320151	,	//	Invalid URL format				
	PJLIB_UTIL_EHTTPINPORT	=	320152	,	//	Invalid URL port number				
	PJLIB_UTIL_EHTTPINCHDR	=	320153	,	//	Incomplete response header received				
	PJLIB_UTIL_EHTTPINSBUF	=	320154	,	//	Insufficient buffer				
	PJLIB_UTIL_EHTTPLOST	=	320155	,	//	"Connection lost"				
		/*	STUN related error codes	*/						
	PJNATH_EINSTUNMSG	=	370001	,	//	"Invalid STUN message"				
	PJNATH_EINSTUNMSGLEN	=	370002	,	//	"Invalid STUN message length"				
	PJNATH_EINSTUNMSGTYPE	=	370003	,	//	"Invalid or unexpected STUN message type"				
	PJNATH_ESTUNTIMEDOUT	=	370004	,	//	"STUN transaction has timed out"				
	PJNATH_ESTUNTOOMANYATTR  	=	370021	,	//	"Too many STUN attributes"				
	PJNATH_ESTUNINATTRLEN	=	370022	,	//	"Invalid STUN attribute length"				
	PJNATH_ESTUNDUPATTR	=	370023	,	//	"Found duplicate STUN attribute"				
	PJNATH_ESTUNFINGERPRINT  	=	370030	,	//	"STUN FINGERPRINT verification failed"				
	PJNATH_ESTUNMSGINTPOS	=	370031	,	//	"Invalid STUN attribute after MESSAGE-INTEGRITY"				
	PJNATH_ESTUNFINGERPOS	=	370033	,	//	"Invalid STUN attribute after FINGERPRINT"				
	PJNATH_ESTUNNOMAPPEDADDR 	=	370040	,	//	"STUN (XOR-MAPPED-ADDRESS attribute not found"				
	PJNATH_ESTUNIPV6NOTSUPP  	=	370041	,	//	"STUN IPv6 attribute not supported"				
	PJNATH_EINVAF	=	370042	,	//	"Invalid STUN address family value"				
	PJNATH_ESTUNINSERVER	=	370050	,	//	"Invalid STUN server or server not configured"				
	PJNATH_ESTUNDESTROYED	=	370060	,	//	"STUN object has been destoyed"				
		/*	ICE related errors	*/						
	PJNATH_ENOICE	=	370080	,	//	"ICE session not available"				
	PJNATH_EICEINPROGRESS	=	370081	,	//	"ICE check is in progress"				
	PJNATH_EICEFAILED	=	370082	,	//	"All ICE checklists failed"				
	PJNATH_EICEMISMATCH	=	370083	,	//	"Default target doesn't match any ICE candidates"				
	PJNATH_EICEINCOMPID	=	370086	,	//	"Invalid ICE component ID"				
	PJNATH_EICEINCANDID	=	370087	,	//	"Invalid ICE candidate ID"				
	PJNATH_EICEINSRCADDR	=	370088	,	//	"Source address mismatch"				
	PJNATH_EICEMISSINGSDP	=	370090	,	//	"Missing ICE SDP attribute"				
	PJNATH_EICEINCANDSDP	=	370091	,	//	"Invalid SDP \"candidate\" attribute"				
	PJNATH_EICENOHOSTCAND	=	370092	,	//	"No host candidate associated with srflx"				
	PJNATH_EICENOMTIMEOUT	=	370093	,	//	"Controlled agent timed out waiting for nomination"				
		/*	TURN related errors	*/						
	PJNATH_ETURNINTP	=	370120	,	//	"Invalid/unsupported transport"				
		/*	Audio Device errors shouldn't be used	*/						
	PJMEDIA_EAUD_ERR	=	420001	,	//	"Unspecified audio device error"				
	PJMEDIA_EAUD_SYSERR	=	420002	,	//	"Unknown error from audio driver"				
	PJMEDIA_EAUD_INIT	=	420003	,	//	"Audio subsystem not initialized"				
	PJMEDIA_EAUD_INVDEV	=	420004	,	//	"Invalid audio device"				
	PJMEDIA_EAUD_NODEV	=	420005	,	//	"Found no audio devices"				
	PJMEDIA_EAUD_NODEFDEV    	=	420006	,	//	"Unable to find default audio device"				
	PJMEDIA_EAUD_NOTREADY   	=	420007	,	//	"Audio device not ready"				
	PJMEDIA_EAUD_INVCAP	=	420008	,	//	"Invalid or unsupported audio capability"				
	PJMEDIA_EAUD_INVOP	=	420009	,	//	"Invalid or unsupported audio device operation"				
	PJMEDIA_EAUD_BADFORMAT   	=	4200010	,	//	"Bad or invalid audio device format"				
	PJMEDIA_EAUD_SAMPFORMAT  	=	4200011	,	//	"Invalid audio device sample format"				
	PJMEDIA_EAUD_BADLATENCY  	=	4200012	,	//	"Bad audio latency setting"				
#endif	

	/* asusnatnl defined status, create list failed */
    NATNL_SC_TNL_CREATE_LIST_FAILED = 60000000, 
	/* asusnatnl status, create sock failed 
	 * the reason can be found in status_text of the natnl_tnl_state structure
	 */
    NATNL_SC_TNL_CREATE_SOCK_FAILED			= 60000001,	// lport of tunnel port may be already in use.
	NATNL_SC_TNL_ADD_OBJECT_FAILED			= 60000002,	// 
	NATNL_SC_TNL_ANOTHER_CALL_ALREADY_MADE	= 60000003,	// tunnel is already made, please do hangup first. 
	NATNL_SC_ALREADY_INITED					= 60000004,	// SDK is already inited, please do deinit first.
	NATNL_SC_NOT_INITED						= 60000005,	// SDK is not inited, please do init first.
	NATNL_SC_MAKE_CALL_TIMEOUT				= 60000006,	// SDK doesn't finish making call process in timeout(defined in natnl_make_call's timeout_sec) period.
	NATNL_SC_IP_CHANGED						= 60000007,	// ip has changed, must do deinit and init again.
	NATNL_SC_TOO_MANY_INSTANCES				= 60000008,	// too many instances.
	NATNL_SC_TNL_TIMEOUT                    = 60000009, // tunnel timeout.
	NATNL_SC_IDLE_TIMEOUT                   = 60000010, // idle timeout.
	NATNL_SC_DE_INITIALIZING				= 60000011,	// the SDK is de-initializing.
	NATNL_SC_INITIALIZING					= 60000012,	// the SDK is initializing.
	NATNL_SC_CONNECT_TO_SIP_TIMEOUT			= 60000013,	// SDK fails to connect to SIP server with connection timeout.
	NATNL_SC_INSTANT_MSG_TOO_LONG			= 60000014,	// the instant message is too long. The maximum length is 1024 bytes.
	NATNL_SC_UDT_CONNECT_FAILED				= 60000015,	// UDT connect failed.

} natnl_status_code;

#endif	/* __NATNL_EVENT_H__ */