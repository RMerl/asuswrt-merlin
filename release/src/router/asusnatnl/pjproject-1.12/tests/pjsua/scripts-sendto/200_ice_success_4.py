# $Id: 200_ice_success_4.py 2376 2008-12-11 17:25:50Z nanang $
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
a=rtcp:4382 IN IP4 192.168.0.4
a=ice-ufrag:1234
a=ice-pwd:5678
a=rtpmap:0 PCMU/8000
a=sendrecv
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
a=candidate:XX 1 UDP 1234 127.0.0.1 4000 typ host
a=candidate:YY 2 UDP 1234 127.0.0.2 4002 typ host
"""

args = "--null-audio --use-ice --auto-answer 200 --max-calls 1 --ice-no-rtcp"
include = ["a=ice-ufrag"]			# must have ICE
exclude = [
	   "ice-mismatch",		     	# must not mismatch
	   "a=candidate:[0-9a-zA-Z]+ 2 UDP"	# must not have RTCP component
	  ]

sendto_cfg = sip.SendtoCfg( "pjsua with --ice-no-rtcp ignores RTCP things in the SDP", 
			    pjsua_args=args, sdp=sdp, resp_code=200, 
			    resp_inc=include, resp_exc=exclude,
			    enable_buffer = True)

