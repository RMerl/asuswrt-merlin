# $Id: 251_multipart_ok_simple.py 3243 2010-08-01 09:48:51Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

body = \
"""
--12345
Content-Type: application/sdp

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

--12345
Content-Type: text/plain

Hi there this is definitely not SDP

--12345--
"""

args = "--null-audio --auto-answer 200 --max-calls 1"
extra_headers = "Content-Type: multipart/mixed; boundary=12345"
include = ["v=0", "m=audio"]
exclude = []

sendto_cfg = sip.SendtoCfg( "Valid multipart/mixed body containing SDP", 
			    pjsua_args=args, sdp="", resp_code=200, 
			    extra_headers=extra_headers, body=body,
			    resp_inc=include, resp_exc=exclude)

