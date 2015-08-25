# $Id: 158_err_sdp_bad_transport_type.py 2081 2008-06-27 21:59:15Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=pjmedia
c=IN IP4 127.0.0.1
t=0 0
m=audio 4000 RTP/XAVP 0 101
a=rtpmap:0 PCMU/8000
a=sendrecv
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = []
exclude = []
sendto_cfg = sip.SendtoCfg("Unsupported transport type", pjsua_args, sdp, 488, 
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 
			   

