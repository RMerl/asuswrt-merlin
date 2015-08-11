# $Id $
import inc_sip as sip
import inc_sdp as sdp

# Answer for codec AMR should not contain fmtp octet-align=1

sdp = \
"""
v=0
o=- 3428650655 3428650655 IN IP4 192.168.1.9
s=pjmedia
c=IN IP4 192.168.1.9
t=0 0
a=X-nat:0
m=audio 4000 RTP/AVP 99 101
a=rtcp:4001 IN IP4 192.168.1.9
a=rtpmap:99 AMR/8000
a=sendrecv
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
"""

pjsua_args = "--null-audio --auto-answer 200 --add-codec AMR"
extra_headers = ""
include = [""]
exclude = ["octet-align=1"]	# response must not include fmtp 'octet-align=1'

sendto_cfg = sip.SendtoCfg("AMR negotiation should not contain 'octet-align=1'", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

