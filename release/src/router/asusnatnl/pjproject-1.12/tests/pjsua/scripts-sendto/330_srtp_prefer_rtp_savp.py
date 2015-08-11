# $Id: 330_srtp_prefer_rtp_savp.py 3251 2010-08-06 01:03:33Z nanang $
import inc_sip as sip
import inc_sdp as sdp

# When SRTP is enabled in pjsua, it should prefer to use
# RTP/SAVP media line if there are multiple m=audio lines
sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=audio 4000 RTP/AVP 0
a=rtpmap:0 pcmu/8000
m=audio 4000 RTP/SAVP 0
a=crypto:1 aes_cm_128_hmac_sha1_80 inline:WnD7c1ksDGs+dIefCEo8omPg4uO8DYIinNGL5yxQ
"""

pjsua_args = "--null-audio --auto-answer 200 --use-srtp 1 --srtp-secure 0"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=audio 0 RTP/AVP[\\s\\S]+m=audio [1-9]+[0-9]* RTP/SAVP[\\s\\S]+a=crypto"
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("Prefer RTP/SAVP", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

