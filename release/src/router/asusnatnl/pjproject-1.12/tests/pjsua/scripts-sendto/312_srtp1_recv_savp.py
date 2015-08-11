# $Id: 312_srtp1_recv_savp.py 2036 2008-06-20 17:43:55Z nanang $
import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=tester
c=IN IP4 127.0.0.1
t=0 0
m=audio 4000 RTP/SAVP 0 101
a=rtpmap:0 PCMU/8000
a=sendrecv
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:WnD7c1ksDGs+dIefCEo8omPg4uO8DYIinNGL5yxQ
a=crypto:2 AES_CM_128_HMAC_SHA1_32 inline:t0r0/apkukU7JjjfR0mY8GEimBq4OiPEm9eKSFOx
"""

args = "--null-audio --auto-answer 200 --max-calls 1 --use-srtp 1 --srtp-secure 0"
include = ["m=audio \d+ RTP/SAVP", "a=crypto"]
exclude = []

sendto_cfg = sip.SendtoCfg( "Callee has SRTP optional receive RTP/SAVP, should answer RTP/SAVP too", 
			    pjsua_args=args, sdp=sdp, resp_code=200, 
			    resp_inc=include, resp_exc=exclude)

