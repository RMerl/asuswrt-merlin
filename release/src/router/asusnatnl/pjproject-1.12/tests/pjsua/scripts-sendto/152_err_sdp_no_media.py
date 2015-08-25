# $Id: 152_err_sdp_no_media.py 2066 2008-06-26 19:51:01Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=pjmedia
c=IN IP4 127.0.0.1
t=0 0
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = []
exclude = []

sendto_cfg = sip.SendtoCfg("No media in SDP", pjsua_args, sdp, 400,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

