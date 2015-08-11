# $Id: 170_timer_required.py 3307 2010-09-08 05:38:49Z nanang $
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

pjsua_args = "--null-audio --auto-answer 200 --use-timer 2 --timer-min-se 90 --timer-se 1800"
extra_headers = "Require: timer\nSupported: timer\nSession-Expires: 1800\n"
include = ["Session-Expires: .*;refresher=.*"]
exclude = []
sendto_cfg = sip.SendtoCfg("Session Timer required", pjsua_args, sdp, 200, 
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 
			   

