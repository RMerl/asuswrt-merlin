# $Id: 252_multipart_ok_clutter.py 3243 2010-08-01 09:48:51Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

body = \
"""
This is the preamble.  It is to be ignored, though it
is a handy place for composition agents to include an
explanatory note to non-MIME conformant readers.

--123:45
Content-Type: text/plain

The first part is definitely not SDP

--123:45

This is implicitly typed plain US-ASCII text.
It does NOT end with a linebreak.
--123:45
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

--123:45--
This is the epilogue.  It is also to be ignored.
"""

args = "--null-audio --auto-answer 200 --max-calls 1"
extra_headers = "Content-Type: multipart/mixed; boundary=\"123:45\""
include = ["v=0", "m=audio"]
exclude = []

sendto_cfg = sip.SendtoCfg( "Valid but cluttered multipart/mixed body containing SDP", 
			    pjsua_args=args, sdp="", resp_code=200, 
			    extra_headers=extra_headers, body=body,
			    resp_inc=include, resp_exc=exclude)

