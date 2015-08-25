# $Id: 155_err_sdp_bad_syntax.py 2066 2008-06-26 19:51:01Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=
o=
s=
c=
t=
a=
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = [ "Warning: " ]	# better have Warning header
exclude = []

sendto_cfg = sip.SendtoCfg("Bad SDP syntax", pjsua_args, sdp, 400,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

