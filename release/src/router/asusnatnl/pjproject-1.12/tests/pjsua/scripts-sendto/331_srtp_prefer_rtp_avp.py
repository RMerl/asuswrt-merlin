# $Id: 331_srtp_prefer_rtp_avp.py 2081 2008-06-27 21:59:15Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# When SRTP is NOT enabled in pjsua, it should prefer to use
# RTP/AVP media line if there are multiple m=audio lines
sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=audio 5000 RTP/SAVP 0
a=crypto:1 aes_cm_128_hmac_sha1_80 inline:WnD7c1ksDGs+dIefCEo8omPg4uO8DYIinNGL5yxQ
m=audio 4000 RTP/AVP 0
"""

pjsua_args = "--null-audio --auto-answer 200 --use-srtp 0"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=audio 0 RTP/SAVP[\\s\\S]+m=audio [1-9]+[0-9]* RTP/AVP"
	   ]
exclude = ["a=crypto"]

sendto_cfg = sip.SendtoCfg("Prefer RTP/SAVP", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

