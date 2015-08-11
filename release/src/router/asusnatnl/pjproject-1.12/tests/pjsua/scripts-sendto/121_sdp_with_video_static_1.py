# $Id: 121_sdp_with_video_static_1.py 2081 2008-06-27 21:59:15Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Video uses static payload type which will cause failure
# when session.c looks-up the codec in codec manager
sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=audio 5000 RTP/AVP 0
m=video 4000 RTP/AVP 54
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=audio [1-9]+[0-9]* RTP/AVP[\\s\\S]+m=video 0 RTP"
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("Mixed audio and video", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

