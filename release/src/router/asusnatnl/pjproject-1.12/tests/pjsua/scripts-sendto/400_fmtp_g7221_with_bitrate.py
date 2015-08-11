# $Id $
import inc_sip as sip
import inc_sdp as sdp

# Answer for codec G722.1 should contain fmtp bitrate

sdp = \
"""
v=0
o=- 3428650655 3428650655 IN IP4 192.168.1.9
s=pjmedia
c=IN IP4 192.168.1.9
t=0 0
a=X-nat:0
m=audio 4000 RTP/AVP 99 100 101
a=rtcp:4001 IN IP4 192.168.1.9
a=rtpmap:99 G7221/16000
a=fmtp:99 bitrate=24000
a=rtpmap:100 G7221/16000
a=fmtp:100 bitrate=32000
a=sendrecv
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
"""

pjsua_args = "--null-audio --auto-answer 200 --add-codec G7221"
extra_headers = ""
include = ["fmtp:[\d]+ bitrate="]	# response must include fmtp bitrate
exclude = []

sendto_cfg = sip.SendtoCfg("Answer should contain fmtp bitrate for codec G722.1", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

