# $Id: 125_sdp_with_multi_audio_4.py 2081 2008-06-27 21:59:15Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Multiple m=audio, one of them has static PT codec that we don't support
sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=audio 5000 RTP/AVP 80
m=audio 4000 RTP/AVP 0
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=audio 0 RTP/AVP[\s\S]+m=audio [1-9]+[0-9]* RTP/AVP"
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("Multiple audio lines", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

