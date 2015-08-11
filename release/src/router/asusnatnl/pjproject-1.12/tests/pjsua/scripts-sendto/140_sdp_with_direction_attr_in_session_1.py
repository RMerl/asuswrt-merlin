# $Id: 140_sdp_with_direction_attr_in_session_1.py 3086 2010-02-03 14:43:25Z nanang $
import inc_sip as sip
import inc_sdp as sdp

# Offer contains "sendonly" attribute in the session. Answer should
# respond with appropriate direction in session or media
sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
a=sendonly
m=audio 5000 RTP/AVP 0
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	       "a=recvonly"
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("SDP direction in session", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

