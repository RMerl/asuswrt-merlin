# $Id: 323_srtp2_receive_too_long_key.py 3193 2010-06-03 02:27:41Z nanang $
import inc_sip as sip
import inc_sdp as sdp

# Too long key should be rejected
sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=audio 5000 RTP/SAVP 0
a=crypto:1 aes_cm_128_hmac_sha1_80 inline:WnD7c1ksDGs+dIefCEo8omPg4uO8DYIinNGL5yxQWnD7c1ksDGs+dIefCEo8omPg4uO8DYIinNGL5yxQ
"""

pjsua_args = "--null-audio --auto-answer 200 --use-srtp 2 --srtp-secure 0"
extra_headers = ""
include = []
exclude = []

sendto_cfg = sip.SendtoCfg("SRTP receive too long key", pjsua_args, sdp, 406,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

