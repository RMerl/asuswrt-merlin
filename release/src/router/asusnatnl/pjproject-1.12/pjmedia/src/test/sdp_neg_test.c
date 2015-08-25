/* $Id: sdp_neg_test.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms oa the GNU General Public License as published by
 * the Free Software Foundation; either version 2 oa the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty oa
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy oa the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>
#include "test.h"
//charles debug
//#include <errno.h>

//charles debug

#define THIS_FILE   "sdp_neg_test.c"
#define START_TEST  0

enum session_type
{
    REMOTE_OFFER,
    LOCAL_OFFER,
};

struct offer_answer
{			
    enum session_type type;	/*  LOCAL_OFFER:	REMOTE_OFFER:	*/
    char *sdp1;			/* local offer		remote offer	*/
    char *sdp2;			/* remote answer	initial local	*/
    char *sdp3;			/* local active media	local answer	*/
};

static struct test
{
    const char		*title;
    unsigned		 offer_answer_count;
    struct offer_answer	 offer_answer[4];
} test[] = 
{
    /* test 0: */
    {
	/*********************************************************************
	 * RFC 3264 examples, section 10.1 (Alice's view)
	 *
	 * Difference from the example:
	 *  - Bob's port number of the third media stream in the first answer
	 *    is changed (make it different than Alice's)
	 *  - in the second offer/answer exchange, Alice can't accept the
	 *    additional line since she didn't specify the capability
	 *    in the initial negotiator creation.
	 */

	"RFC 3264 example 10.1 (Alice's view)",
	2,
	{
	  {
	    LOCAL_OFFER,
	    /* Alice sends offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.anywhere.com\r\n"
	    "s= \r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 51372 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "m=video 53000 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n",
	    /* Received Bob's answer: */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844730 IN IP4 host.example.com\r\n"
	    "s= \r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49920 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "m=video 53002 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n",
	    /* Alice's SDP now: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.anywhere.com\r\n"
	    "s= \r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    //"a=rtpmap:31 H261/90000\r\n"	/* <-- this is not necessary (port 0) */
	    "m=video 53000 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n"
	  },
	  {
	    REMOTE_OFFER,
	    /* Bob wants to change his local SDP 
	     * (change local port for the first stream and add new stream)
	     * Received SDP from Bob:
	     */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844731 IN IP4 host.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 65422 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "m=video 53002 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n"
	    "m=audio 51434 RTP/AVP 110\r\n"
	    "a=rtpmap:110 telephone-events/8000\r\n"
	    "a=recvonly\r\n",
	    NULL,
	    /* Alice's SDP now */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.anywhere.com\r\n"
	    "s= \r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    //"a=rtpmap:31 H261/90000\r\n"	/* <-- this is not necessary (port 0) */
	    "m=video 53000 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n"
	    "m=audio 0 RTP/AVP 110\r\n"
	    /* <-- the following attributes are not necessary (port 0) */
	    //"a=rtpmap:110 telephone-events/8000\r\n"
	    //"a=sendonly\r\n"
	  }
	}
    },

    /* test 1: */
    {
	/*********************************************************************
	 * RFC 3264 examples, section 10.1. (Bob's view)
	 *
	 * Difference:
	 *  - the SDP version in Bob's capability is changed to ver-1.
	 */

	"RFC 3264 example 10.1 (Bob's view)",
	2,
	{
	  {
	    REMOTE_OFFER,
	    /* Remote offer from Alice: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.anywhere.com\r\n"
	    "s= \r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 51372 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "m=video 53000 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n",
	    /* Bob's capability: */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844729 IN IP4 host.example.com\r\n"
	    "s= \r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49920 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "m=video 53000 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n",
	    /* This's how Bob's answer should look like: */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844730 IN IP4 host.example.com\r\n"
	    "s= \r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49920 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "m=video 53000 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n"
	  },
	  {
	    LOCAL_OFFER,
	    /* Bob wants to change his local SDP 
	     * (change local port for the first stream and add new stream)
	     */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844731 IN IP4 host.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 65422 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "m=video 53000 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n"
	    "m=audio 51434 RTP/AVP 110\r\n"
	    "a=rtpmap:110 telephone-events/8000\r\n"
	    "a=recvonly\r\n",
	    /* Got answer from Alice */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.anywhere.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "m=video 53000 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n"
	    "m=audio 53122 RTP/AVP 110\r\n"
	    "a=rtpmap:110 telephone-events/8000\r\n"
	    "a=sendonly\r\n",
	    /* This is how Bob's SDP should look like after negotiation */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844731 IN IP4 host.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 65422 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "m=video 53000 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n"
	    "m=audio 51434 RTP/AVP 110\r\n"
	    "a=rtpmap:110 telephone-events/8000\r\n"
	    "a=recvonly\r\n"
	  }
	}
    },

    /* test 2: */
    {
	/*********************************************************************
	 * RFC 3264 examples, section 10.2.
	 * This is from Alice's point of view.
	 */

	"RFC 3264 example 10.2 (Alice's view)",
	2,
	{
	  {
	    LOCAL_OFFER,
	    /* The initial offer from Alice to Bob indicates a single audio 
	     * stream with the three audio codecs that are available in the 
	     * DSP. The stream is marked as inactive, 
	     */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.anywhere.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 62986 RTP/AVP 0 4 18\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=rtpmap:18 G729/8000\r\n"
	    "a=inactive\r\n",
	    /* Bob can support dynamic switching between PCMU and G.723. So, 
	     * he sends the following answer:
	     */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844731 IN IP4 host.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 54344 RTP/AVP 0 4\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=inactive\r\n",
	    /* This is how Alice's media should look like after negotiation */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.anywhere.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 62986 RTP/AVP 0 4\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=inactive\r\n",
	  },
	  {
	    LOCAL_OFFER,
	    /* Alice sends an updated offer with a sendrecv stream: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.anywhere.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 62986 RTP/AVP 4\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=sendrecv\r\n",
	    /* Bob accepts the single codec: */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844732 IN IP4 host.example.com\r\n"
	    "s= \r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 54344 RTP/AVP 4\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=sendrecv\r\n",
	    /* This is how Alice's media should look like after negotiation */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.anywhere.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 62986 RTP/AVP 4\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=sendrecv\r\n"
	  }
	}
    },

#if 0
    // this test is commented, this causes error: 
    // No suitable codec for remote offer (PJMEDIA_SDPNEG_NOANSCODEC),
    // since currently the negotiator always answer with one codec, 
    // PCMU in this case, while PCMU is not included in the second offer.

    /* test 3: */
    {
	/*********************************************************************
	 * RFC 3264 examples, section 10.2.
	 * This is from Bob's point of view.
	 *
	 * Difference:
	 *  - The SDP version number in Bob's initial capability is ver-1
	 */

	"RFC 3264 example 10.2 (Bob's view)",
	2,
	{
	  {
	    REMOTE_OFFER,
	    /* Bob received offer from Alice:
	     */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.anywhere.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 62986 RTP/AVP 0 4 18\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=rtpmap:18 G729/8000\r\n"
	    "a=inactive\r\n",
	    /* Bob's capability:
	     */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844730 IN IP4 host.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 54344 RTP/AVP 0 4\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=inactive\r\n",
	    /* This is how Bob's media should look like after negotiation */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844731 IN IP4 host.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 54344 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=inactive\r\n"
	  },
	  {
	    REMOTE_OFFER,
	    /* Received updated Alice's SDP: offer with a sendrecv stream: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.anywhere.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.anywhere.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 62986 RTP/AVP 4\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=sendrecv\r\n",
	    /* Bob accepts the single codec: */
	    NULL,
	    /* This is how Bob's media should look like after negotiation */
	    "v=0\r\n"
	    "o=bob 2890844730 2890844732 IN IP4 host.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 54344 RTP/AVP 4\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=sendrecv\r\n",
	  }
	}
    },
#endif

    /* test 4: */
    {
	/*********************************************************************
	 * RFC 4317 Sample 2.1: Audio and Video 1 (Alice's view)
	 *
	 * This common scenario shows a video and audio session in which
	 * multiple codecs are offered but only one is accepted.  As a result of
	 * the exchange shown below, Alice and Bob may send only PCMU audio and
	 * MPV video.  Note: Dynamic payload type 97 is used for iLBC codec
	 */
	"RFC 4317 section 2.1: Audio and Video 1 (Alice's view)",
	1,
	{
	  {
	    LOCAL_OFFER,
	    /* Alice's local offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0 8 97\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:8 PCMA/8000\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "m=video 51372 RTP/AVP 31 32\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "a=rtpmap:32 MPV/90000\r\n",
	    /* Received answer from Bob: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844564 IN IP4 host.biloxi.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49174 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 49170 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n",
	    /* This is how Alice's media should look like now: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 51372 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n"
	  }
	}
    },

    /* test 5: */
    {
	/*********************************************************************
	 * RFC 4317 Sample 2.1: Audio and Video 1 (Bob's view)
	 *
	 * This common scenario shows a video and audio session in which
	 * multiple codecs are offered but only one is accepted.  As a result of
	 * the exchange shown below, Alice and Bob may send only PCMU audio and
	 * MPV video.  Note: Dynamic payload type 97 is used for iLBC codec
	 *
	 * Difference:
	 *  - Bob's initial capability version number
	 */
	"RFC 4317 section 2.1: Audio and Video 1 (Bob's view)",
	1,
	{
	  {
	    REMOTE_OFFER,
	    /* Received Alice's local offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0 8 97\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:8 PCMA/8000\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "m=video 51372 RTP/AVP 31 32\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "a=rtpmap:32 MPV/90000\r\n",
	    /* Bob's capability: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49174 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 49170 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n",
	    /* This is how Bob's media should look like now: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844564 IN IP4 host.biloxi.example.com\r\n"
	    "s=-\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49174 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 49170 RTP/AVP 32\r\n"
	    "a=rtpmap:32 MPV/90000\r\n"
	  }
	}
    },

    /* test 6: */
    {
	/*********************************************************************
	 * RFC 4317 Sample 2.2: Audio and Video 2 (Alice's view)
	 *
	 * Difference:
	 *  - Bob's initial capability version number
	 */
	"RFC 4317 section 2.2: Audio and Video 2 (Alice's view)",
	2,
	{
	  {
	    LOCAL_OFFER,
	    /* Alice sends offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0 8 97\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:8 PCMA/8000\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "m=video 51372 RTP/AVP 31 32\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "a=rtpmap:32 MPV/90000\r\n",
	    /* Bob's answer: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844564 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49172 RTP/AVP 0 8\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:8 PCMA/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n",
	    /* This is how Alice's media should look like now: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0 8\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:8 PCMA/8000\r\n"
	    // By #1088, the formats won't be negotiated when the media has port 0.
	    //"m=video 0 RTP/AVP 31\r\n"
	    "m=video 0 RTP/AVP 31 32\r\n"
	    //"a=rtpmap:31 H261/90000\r\n"  /* <-- this is not necessary (port 0) */
	  },
	  {
	    LOCAL_OFFER,
	    /* Alice sends updated offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 51372 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n",
	    /* Bob's answer: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844565 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49172 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n",
	    /* This is how Alice's SDP should look like: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 51372 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    //"a=rtpmap:31 H261/90000\r\n"  /* <-- this is not necessary (port 0) */
	  }
	}
    },

    /* test 7: */
    {
	/*********************************************************************
	 * RFC 4317 Sample 2.2: Audio and Video 2 (Bob's view)
	 *
	 * Difference:
	 *  - Bob's initial capability version number
	 */
	"RFC 4317 section 2.2: Audio and Video 2 (Bob's view)",
	2,
	{
	  {
	    REMOTE_OFFER,
	    /* Received offer from alice: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0 8 97\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:8 PCMA/8000\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "m=video 51372 RTP/AVP 31 32\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "a=rtpmap:32 MPV/90000\r\n",
	    /* Bob's initial capability: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49172 RTP/AVP 0 8\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:8 PCMA/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n",
	    /* This is how Bob's answer should look like now: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844564 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49172 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	  },
	  {
	    REMOTE_OFFER,
	    /* Received updated offer from Alice: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 51372 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n",
	    /* Bob's answer: */
	    NULL,
	    /* This is how Bob's answer should look like: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844565 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49172 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    //"a=rtpmap:31 H261/90000\r\n"  /* <-- this is not necessary (port 0) */
	  }
	}
    },

    /* test 8: */
    {
	/*********************************************************************
	 * RFC 4317 Sample 2.4: Audio and Telephone-Events (Alice's view)
	 *
	 */

	"RFC 4317 section 2.4: Audio and Telephone-Events (Alice's view)",
	1,
	{
	  {
	    LOCAL_OFFER,
	    /* Alice sends offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0 97\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "m=audio 49172 RTP/AVP 98\r\n"
	    "a=rtpmap:98 telephone-event/8000\r\n"
	    "a=sendonly\r\n",
	    /* Received Bob's answer: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844564 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49172 RTP/AVP 97\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "m=audio 49174 RTP/AVP 98\r\n"
	    "a=rtpmap:98 telephone-event/8000\r\n"
	    "a=recvonly\r\n",
	    /* Alice's SDP now: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 97\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "m=audio 49172 RTP/AVP 98\r\n"
	    "a=rtpmap:98 telephone-event/8000\r\n"
	    "a=sendonly\r\n"
	  }
	}
    },


    /* test 9: */
    {
	/*********************************************************************
	 * RFC 4317 Sample 2.4: Audio and Telephone-Events (Bob's view)
	 *
	 * Difference:
	 *  - Bob's initial SDP version number
	 *  - Bob's capability are added with more formats, and the
	 *    stream order is interchanged to test the negotiator.
	 */

	"RFC 4317 section 2.4: Audio and Telephone-Events (Bob's view)",
	1,
	{
	  {
	    REMOTE_OFFER,
	    /* Received Alice's offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0 97\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "m=audio 49172 RTP/AVP 98\r\n"
	    "a=rtpmap:98 telephone-event/8000\r\n"
	    "a=sendonly\r\n",
	    /* Bob's initial capability: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49174 RTP/AVP 4 98\r\n"
	    "a=rtpmap:98 telephone-event/8000\r\n"
	    "m=audio 49172 RTP/AVP 97 8 99\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "a=rtpmap:99 telephone-event/8000\r\n",
	    /* Bob's answer should be: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844564 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49172 RTP/AVP 97\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "m=audio 49174 RTP/AVP 98\r\n"
	    "a=rtpmap:98 telephone-event/8000\r\n"
	    "a=recvonly\r\n"
	  }
	}
    },

    /* test 10: */
    {
	/*********************************************************************
	 * RFC 4317 Sample 2.6: Audio with Telephone-Events (Alice's view)
	 *
	 */

	"RFC 4317 section 2.6: Audio with Telephone-Events (Alice's view)",
	1,
	{
	  {
	    LOCAL_OFFER,
	    /* Alice sends offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 51372 RTP/AVP 97 101\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "a=rtpmap:101 telephone-event/8000\r\n",
	    /* Received bob's answer: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844564 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 0 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 49170 RTP/AVP 97 101\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "a=rtpmap:101 telephone-event/8000\r\n",
	    /* Alice's local SDP should be: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 0 RTP/AVP 0\r\n"
	    //"a=rtpmap:0 PCMU/8000\r\n"	  /* <-- this is not necessary (port 0) */
	    "m=audio 51372 RTP/AVP 97 101\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "a=rtpmap:101 telephone-event/8000\r\n"
	  }
	}
    },

    /* test 11: */
    {
	/*********************************************************************
	 * RFC 4317 Sample 2.6: Audio with Telephone-Events (Bob's view)
	 *
	 * Difference:
	 *  - Bob's SDP version number
	 *  - Bob's initial capability are expanded with multiple m lines
	 *    and more formats
	 */

	"RFC 4317 section 2.6: Audio with Telephone-Events (Bob's view)",
	1,
	{
	  {
	    REMOTE_OFFER,
	    /* Received Alice's offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 51372 RTP/AVP 97 101\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "a=rtpmap:101 telephone-event/8000\r\n",
	    /* Bob's initial capability also has video: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 4 97 101\r\n"
	    "a=rtpmap:4 G723/8000\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "a=rtpmap:101 telephone-event/8000\r\n"
	    "m=video 1000 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n",
	    /* Bob's answer should be: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844564 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 0 RTP/AVP 0\r\n"
	    //"a=rtpmap:0 PCMU/8000\r\n"      /* <-- this is not necessary (port 0) */
	    "m=audio 49170 RTP/AVP 97 101\r\n"
	    "a=rtpmap:97 iLBC/8000\r\n"
	    "a=rtpmap:101 telephone-event/8000\r\n",
	  }
	}
    },

    /* test 12: */
    {
	/*********************************************************************
	 * Ticket #527: More lenient SDP negotiator.
	 */

	"Ticket #527 scenario #1: Partial answer",
	1,
	{
	  {
	    LOCAL_OFFER,
	    /* Alice sends offer audio and video: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 4000 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n",
	    /* Receive Bob's answer only audio: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n",
	    /* Alice's local SDP should be: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    //"a=rtpmap:31 H261/90000\r\n"    /* <-- this is not necessary (port 0) */
	    "",
	  }
	}
    },

    /* test 13: */
    {
	/*********************************************************************
	 * Ticket #527: More lenient SDP negotiator.
	 */

	"Ticket #527 scenario #1: Media mismatch in answer",
	1,
	{
	  {
	    LOCAL_OFFER,
	    /* Alice sends offer audio and video: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 3000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 4000 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n",
	    /* Receive Bob's answer two audio: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 49172 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n",
	    /* Alice's local SDP should be: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 3000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    //"a=rtpmap:31 H261/90000\r\n"    /* <-- this is not necessary (port 0) */
	    "",
	  }
	}
    },

    /* test 14: */
    {
	/*********************************************************************
	 * Ticket #527: More lenient SDP negotiator.
	 */

	"Ticket #527 scenario #2: Modify offer - partial streams",
	2,
	{
	  {
	    LOCAL_OFFER,
	    /* Alice sends offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 3000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 3100 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 3200 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	    /* Receive Bob's answer: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 4000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 4100 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 4200 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	    /* Alice's local SDP should be: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 3000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 3100 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 3200 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	  },
	  {
	    LOCAL_OFFER,
	    /* Alice modifies offer with only specify one audio: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 5200 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "",
	    /* Receive Bob's answer: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 7000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 0 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 0 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	    /* Alice's local SDP should be: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 5200 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 0 RTP/AVP 0\r\n"
	    //"a=rtpmap:0 PCMU/8000\r\n"      /* <-- this is not necessary (port 0) */
	    "m=video 0 RTP/AVP 31\r\n"
	    //"a=rtpmap:31 H261/90000\r\n"    /* <-- this is not necessary (port 0) */
	    "",
	  }
	}
    },

    /* test 15: */
    {
	/*********************************************************************
	 * Ticket #527: More lenient SDP negotiator.
	 */

	"Ticket #527 scenario #2: Modify offer - unordered m= lines",
	2,
	{
	  {
	    LOCAL_OFFER,
	    /* Alice sends offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 3000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 3200 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	    /* Receive Bob's answer: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 4000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 4200 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	    /* Alice's local SDP should be: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 3000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 3200 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	  },
	  {
	    LOCAL_OFFER,
	    /* Alice modifies offer with unordered m= lines: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=video 5000 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "m=audio 5200 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "",
	    /* Receive Bob's answer: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 7000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 2000 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	    /* Alice's local SDP should be: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 5200 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 5000 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	  }
	}
    },

    /* test 16: */
    {
	/*********************************************************************
	 * Ticket #527: More lenient SDP negotiator.
	 */

	"Ticket #527 scenario #2: Modify offer - partial & unordered streams",
	2,
	{
	  {
	    LOCAL_OFFER,
	    /* Alice sends offer: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 3000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 3200 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 3400 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	    /* Receive Bob's answer: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 4000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 4200 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 4400 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	    /* Alice's local SDP should be: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 3000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 3200 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 3400 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	  },
	  {
	    LOCAL_OFFER,
	    /* Alice modifies offer by specifying partial and unordered media: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844526 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=video 5000 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "m=audio 7000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "",
	    /* Receive Bob's answer: */
	    "v=0\r\n"
	    "o=bob 2808844564 2808844563 IN IP4 host.biloxi.example.com\r\n"
	    "s=bob\r\n"
	    "c=IN IP4 host.biloxi.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 4000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 0 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=video 4400 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	    /* Alice's local SDP should be: */
	    "v=0\r\n"
	    "o=alice 2890844526 2890844527 IN IP4 host.atlanta.example.com\r\n"
	    "s=alice\r\n"
	    "c=IN IP4 host.atlanta.example.com\r\n"
	    "t=0 0\r\n"
	    "m=audio 7000 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n"
	    "m=audio 0 RTP/AVP 0\r\n"
	    //"a=rtpmap:0 PCMU/8000\r\n"      /* <-- this is not necessary (port 0) */
	    "m=video 5000 RTP/AVP 31\r\n"
	    "a=rtpmap:31 H261/90000\r\n"
	    "",
	  }
	}
    },

};

static const char *find_diff(const char *s1, const char *s2,
			     int *line)
{
    *line = 1;
    while (*s2 && *s1) {
	if (*s2 != *s1)
	    return s2;
	if (*s2 == '\n')
	    ++*line;
	++s2, ++s1;
    }

    return s2;
}

static int compare_sdp_string(const char *cmp_title,
			      const char *title1,
			      const pjmedia_sdp_session *sdp1,
			      const char *title2,
			      const pjmedia_sdp_session *sdp2,
			      pj_status_t logical_cmp)
{
    char sdpbuf1[1024], sdpbuf2[1024];
    pj_ssize_t len1, len2;

    len1 = pjmedia_sdp_print(sdp1, sdpbuf1, sizeof(sdpbuf1));
    if (len1 < 1) {
	PJ_LOG(3,(THIS_FILE,"   error: printing sdp1"));
	return -1;
    }
    sdpbuf1[len1] = '\0';

    len2 = pjmedia_sdp_print(sdp2, sdpbuf2, sizeof(sdpbuf2));
    if (len2 < 1) {
	PJ_LOG(3,(THIS_FILE,"   error: printing sdp2"));
	return -1;
    }
    sdpbuf2[len2] = '\0';

    if (logical_cmp != PJ_SUCCESS) {
	char errbuf[80];

	pjmedia_strerror(logical_cmp, errbuf, sizeof(errbuf));

	PJ_LOG(3,(THIS_FILE,"%s mismatch: %s\n"
			    "%s:\n"
			    "%s\n"
			    "%s:\n"
			    "%s\n",
			    cmp_title,
			    errbuf,
			    title1, sdpbuf1, 
			    title2, sdpbuf2));

	return -1;

    } else if (strcmp(sdpbuf1, sdpbuf2) != 0) {
	int line;
	const char *diff;

	PJ_LOG(3,(THIS_FILE,"%s mismatch:\n"
			    "%s:\n"
			    "%s\n"
			    "%s:\n"
			    "%s\n",
			    cmp_title,
			    title1, sdpbuf1, 
			    title2, sdpbuf2));

	diff = find_diff(sdpbuf1, sdpbuf2, &line);
	PJ_LOG(3,(THIS_FILE,"Difference: line %d:\n"
			    "%s",
			    line, diff));
	return -1;

    } else {
	return 0;
    }

}

static int offer_answer_test(pj_pool_t *pool, pjmedia_sdp_neg **p_neg,
			     struct offer_answer *oa)
{
    pjmedia_sdp_session *sdp1;
    pjmedia_sdp_neg *neg;
    pj_status_t status;

    pj_size_t str_len = pj_ansi_strlen(oa->sdp1);

    status = pjmedia_sdp_parse(0, pool, &oa->sdp1, &str_len,
				&sdp1);
    if (status != PJ_SUCCESS) {
	app_perror(status, "   error: unexpected parse status for sdp1");
	return -10;
    }

    status = pjmedia_sdp_validate(sdp1);
    if (status != PJ_SUCCESS) {
	app_perror(status, "   error: sdp1 validation failed");
	return -15;
    }

    neg = *p_neg;

    if (oa->type == LOCAL_OFFER) {
	
	/* 
	 * Local creates offer first. 
	 */
	pjmedia_sdp_session *sdp2, *sdp3;
	const pjmedia_sdp_session *active;

	if (neg == NULL) {
	    /* Create negotiator with local offer. */
	    status = pjmedia_sdp_neg_create_w_local_offer(pool, sdp1, &neg);
	    if (status != PJ_SUCCESS) {
		app_perror(status, "   error: pjmedia_sdp_neg_create_w_local_offer");
		return -20;
	    }
	    *p_neg = neg;

	} else {
	    /* Modify local offer */
	    status = pjmedia_sdp_neg_modify_local_offer(pool, neg, sdp1);
	    if (status != PJ_SUCCESS) {
		app_perror(status, "   error: pjmedia_sdp_neg_modify_local_offer");
		return -30;
	    }
	}

	str_len = pj_ansi_strlen(oa->sdp2);
	/* Parse and validate remote answer */
	status = pjmedia_sdp_parse(0, pool, &oa->sdp2, &str_len,
				   &sdp2);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: parsing sdp2");
	    return -40;
	}

	status = pjmedia_sdp_validate(sdp2);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: sdp2 validation failed");
	    return -50;
	}

	/* Give the answer to negotiator. */
	status = pjmedia_sdp_neg_set_remote_answer(pool, neg, sdp2);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: pjmedia_sdp_neg_rx_remote_answer");
	    return -60;
	}

	/* Negotiate remote answer with local answer */
	status = pjmedia_sdp_neg_negotiate(0, pool, neg, 0);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: pjmedia_sdp_neg_negotiate");
	    return -70;
	}

	/* Get the local active media. */
	status = pjmedia_sdp_neg_get_active_local(neg, &active);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: pjmedia_sdp_neg_get_local");
	    return -80;
	}

	str_len = pj_ansi_strlen(oa->sdp3);
	/* Parse and validate the correct active media. */
	status = pjmedia_sdp_parse(0, pool, &oa->sdp3, &str_len,
				   &sdp3);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: parsing sdp3");
	    return -90;
	}

	status = pjmedia_sdp_validate(sdp3);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: sdp3 validation failed");
	    return -100;
	}

	/* Compare active with sdp3 */
	status = pjmedia_sdp_session_cmp(0, active, sdp3, 0);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: active local comparison mismatch");
	    compare_sdp_string("Logical cmp after negotiatin remote answer",
			       "Active local sdp from negotiator", active,
			       "The correct active local sdp", sdp3,
			       status);
	    return -110;
	}

	/* Compare the string representation oa both sdps */
	status = compare_sdp_string("String cmp after negotiatin remote answer",
				    "Active local sdp from negotiator", active,
				    "The correct active local sdp", sdp3,
				    PJ_SUCCESS);
	if (status != 0)
	    return -120;

    } else {
	/* 
	 * Remote creates offer first. 
	 */

	pjmedia_sdp_session *sdp2 = NULL, *sdp3;
	const pjmedia_sdp_session *answer;

	if (oa->sdp2) {

		str_len = pj_ansi_strlen(oa->sdp2);
	    /* Parse and validate initial local capability */
	    status = pjmedia_sdp_parse(0, pool, &oa->sdp2, &str_len,
				       &sdp2);
	    if (status != PJ_SUCCESS) {
		app_perror(status, "   error: parsing sdp2");
		return -200;
	    }

	    status = pjmedia_sdp_validate(sdp2);
	    if (status != PJ_SUCCESS) {
		app_perror(status, "   error: sdp2 validation failed");
		return -210;
	    }
	} else if (neg) {
	    const pjmedia_sdp_session *lsdp;
	    status = pjmedia_sdp_neg_get_active_local(neg, &lsdp);
	    if (status != PJ_SUCCESS) {
		app_perror(status, 
			   "   error: pjmedia_sdp_neg_get_active_local");
		return -215;
	    }
	    sdp2 = (pjmedia_sdp_session*)lsdp;
	}

	if (neg == NULL) {
	    /* Create negotiator with remote offer. */
	    status = pjmedia_sdp_neg_create_w_remote_offer(pool, sdp2, sdp1, &neg);
	    if (status != PJ_SUCCESS) {
		app_perror(status, "   error: pjmedia_sdp_neg_create_w_remote_offer");
		return -220;
	    }
	    *p_neg = neg;

	} else {
	    /* Received subsequent offer from remote. */
	    status = pjmedia_sdp_neg_set_remote_offer(pool, neg, sdp1);
	    if (status != PJ_SUCCESS) {
		app_perror(status, "   error: pjmedia_sdp_neg_rx_remote_offer");
		return -230;
	    }

	    status = pjmedia_sdp_neg_set_local_answer(pool, neg, sdp2);
	    if (status != PJ_SUCCESS) {
		app_perror(status, "   error: pjmedia_sdp_neg_set_local_answer");
		return -235;
	    }
	}

	/* Negotiate. */
	status = pjmedia_sdp_neg_negotiate(0, pool, neg, 0);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: pjmedia_sdp_neg_negotiate");
	    return -240;
	}
	
	/* Get our answer. */
	status = pjmedia_sdp_neg_get_active_local(neg, &answer);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: pjmedia_sdp_neg_get_local");
	    return -250;
	}

	str_len = pj_ansi_strlen(oa->sdp3);
	/* Parse the correct answer. */
	status = pjmedia_sdp_parse(0, pool, &oa->sdp3, &str_len,
				   &sdp3);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: parsing sdp3");
	    return -260;
	}

	/* Validate the correct answer. */
	status = pjmedia_sdp_validate(sdp3);
	if (status != PJ_SUCCESS) {
	    app_perror(status, "   error: sdp3 validation failed");
	    return -270;
	}

	/* Compare answer from negotiator and the correct answer */
	status = pjmedia_sdp_session_cmp(0, sdp3, answer, 0);
	if (status != PJ_SUCCESS) {
	    compare_sdp_string("Logical cmp after negotiating remote offer",
			       "Local answer from negotiator", answer,
			       "The correct local answer", sdp3,
			       status);

	    return -280;
	}

	/* Compare the string representation oa both answers */
	status = compare_sdp_string("String cmp after negotiating remote offer",
				    "Local answer from negotiator", answer,
				    "The correct local answer", sdp3,
				    PJ_SUCCESS);
	if (status != 0)
	    return -290;

    }

    return 0;
}

static int perform_test(pj_pool_t *pool, int test_index)
{
    pjmedia_sdp_neg *neg = NULL;
    unsigned i;
    int rc;

    for (i=0; i<test[test_index].offer_answer_count; ++i) {

	rc = offer_answer_test(pool, &neg, &test[test_index].offer_answer[i]);
	if (rc != 0) {
	    PJ_LOG(3,(THIS_FILE, "  test %d offer answer %d failed with status %d",
		      test_index, i, rc));
	    return rc;
	}
    }

    return 0;
}

int sdp_neg_test()
{
    unsigned i;
    int status;

    for (i=START_TEST; i<PJ_ARRAY_SIZE(test); ++i) {
	pj_pool_t *pool;

	pool = pj_pool_create(mem, "sdp_neg_test", 4000, 4000, NULL);
	if (!pool)
	    return PJ_ENOMEM;

	PJ_LOG(3,(THIS_FILE,"  test %d: %s", i, test[i].title));
	status = perform_test(pool, i);

	pj_pool_release(pool);

	if (status != 0) {
	    return status;
	}
    }

    return 0;
}

