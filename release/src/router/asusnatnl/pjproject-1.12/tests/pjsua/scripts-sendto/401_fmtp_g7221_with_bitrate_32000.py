# $Id $
import inc_sip as sip
import inc_sdp as sdp

# Answer with codec G722.1 should choose the same bitrate
# which in this test is 32000

sdp = \
"""
v=0
o=- 3428650655 3428650655 IN IP4 192.168.1.9
s=pjmedia
c=IN IP4 192.168.1.9
t=0 0
a=X-nat:0
m=audio 4000 RTP/AVP 100 101
a=rtcp:4001 IN IP4 192.168.1.9
a=rtpmap:100 G7221/16000
a=fmtp:100 bitrate=32000
a=sendrecv
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
"""

pjsua_args = "--null-audio --auto-answer 200 --add-codec G7221"
extra_headers = ""
include = ["a=rtpmap:[\d]+ G7221/16000",  # response must choose G722.1
	   "fmtp:[\d]+ bitrate=32000"	  # response must choose the same bitrate
	  ]
exclude = []

sendto_cfg = sip.SendtoCfg("Answer with G722.1 should choose bitrate 32000", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

