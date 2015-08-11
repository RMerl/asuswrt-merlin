# $Id: 320_srtp_with_unknown_transport_2.py 2081 2008-06-27 21:59:15Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=audio 4000 UNKNOWN 0
m=audio 5000 RTP/AVP 0
a=crypto:1 aes_cm_128_hmac_sha1_80 inline:WnD7c1ksDGs+dIefCEo8omPg4uO8DYIinNGL5yxQ
"""

pjsua_args = "--null-audio --auto-answer 200 --use-srtp 1 --srtp-secure 0"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=audio 0 UNKNOWN[\\s\\S]+m=audio [1-9]+[0-9]* RTP/AVP[\\s\\S]+a=crypto"
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("SRTP audio and unknown media", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

