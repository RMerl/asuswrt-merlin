# $Id: 140_sdp_with_direction_attr_in_session_2.py 3086 2010-02-03 14:43:25Z nanang $
import inc_sip as sip
import inc_sdp as sdp

# Offer contains "inactive" attribute in the session, however the media
# also has "sendonly" attribute. Answer should appropriately respond
# direction attribute in media, instead of the one in session.
sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
a=inactive
m=audio 5000 RTP/AVP 0
a=sendonly
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

