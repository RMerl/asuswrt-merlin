# $Id: 126_sdp_with_port_0_and_no_rtpmap_for_dynamic_pt.py 3011 2009-11-10 10:58:20Z nanang $
import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=video 0 RTP/AVP 100
m=audio 5000 RTP/AVP 0
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=video 0 RTP/AVP[\\s\\S]+m=audio [1-9]+[0-9]* RTP/AVP"
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("SDP media with port 0 and no rtpmap for dynamic PT", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

