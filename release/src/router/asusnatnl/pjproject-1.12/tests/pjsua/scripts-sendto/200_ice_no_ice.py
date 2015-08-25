# $Id: 200_ice_no_ice.py 2392 2008-12-22 18:54:58Z bennylp $
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

args = "--null-audio --use-ice --auto-answer 200 --max-calls 1"
include = []
exclude = ["a=ice", "a=candidate"]

sendto_cfg = sip.SendtoCfg( "caller has no ice, answer must not have ICE", 
			    pjsua_args=args, sdp=sdp, resp_code=200, 
			    resp_inc=include, resp_exc=exclude)

