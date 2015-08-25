# $Id: 173_timer_offer_refresher_uac.py 2858 2009-08-11 12:42:38Z nanang $
import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=pjmedia
c=IN IP4 127.0.0.1
t=0 0
m=audio 4000 RTP/AVP 0 101
a=rtpmap:0 PCMU/8000
a=sendrecv
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
"""

pjsua_args = "--null-audio --auto-answer 200 --timer-min-se 90"
extra_headers = "Supported: timer\nSession-Expires: 1800;refresher=uac\n"
include = ["Session-Expires:.*;refresher=ua[cs]"]
exclude = []
sendto_cfg = sip.SendtoCfg("Session Timer offer refresher uac", pjsua_args, sdp, 200, 
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 
			   
