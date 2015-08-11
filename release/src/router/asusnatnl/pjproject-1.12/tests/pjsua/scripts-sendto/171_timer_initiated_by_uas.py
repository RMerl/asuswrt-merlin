# $Id: 171_timer_initiated_by_uas.py 3307 2010-09-08 05:38:49Z nanang $
import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=pjmedia
c=IN IP4 127.0.0.1
t=0 0
m=audio 4000 RTP/AVP 0 101
a=rtpmap:0 PCMU/8000
a=sendrecv
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
"""

# RFC 4028 Section 9:
#   If the incoming request contains a Supported header field with a
#   value 'timer' but does not contain a Session-Expires header, it means
#   that the UAS is indicating support for timers but is not requesting
#   one.  The UAS may request a session timer in the 2XX response by
#   including a Session-Expires header field.  The value MUST NOT be set
#   to a duration lower than the value in the Min-SE header field in the
#   request, if it is present.
   
pjsua_args = "--null-audio --auto-answer 200 --use-timer 2 --timer-min-se 90 --timer-se 1800"
extra_headers = "Supported: timer\n"
include = ["Session-Expires: .*;refresher=.*"]
exclude = []
sendto_cfg = sip.SendtoCfg("Session Timer initiated by UAS", pjsua_args, sdp, 200, 
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 
			   

