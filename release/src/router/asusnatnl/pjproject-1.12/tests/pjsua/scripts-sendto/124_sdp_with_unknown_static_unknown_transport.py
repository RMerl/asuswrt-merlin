# $Id: 124_sdp_with_unknown_static_unknown_transport.py 2081 2008-06-27 21:59:15Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=audio 5000 RTP/AVP 0
m=xapplicationx 4000 XRTPX/XAVPX 54
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=audio [1-9]+[0-9]* RTP/AVP[\\s\\S]+m=xapplicationx 0 XRTPX/XAVPX "
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("Mixed audio and unknown and with unknown transport", 
			   pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

