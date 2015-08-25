# $Id: 999_asterisk_err.py 2081 2008-06-27 21:59:15Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# http://lists.pjsip.org/pipermail/pjsip_lists.pjsip.org/2008-June/003426.html:
#
# Report in pjsip mailing list on 27/6/2008 that this message will
# cause pjsip to respond with 500 and then second request will cause
# segfault.
complete_msg = \
"""INVITE sip:5001@192.168.1.200:5060;transport=UDP SIP/2.0
Via: SIP/2.0/UDP 192.168.1.11:5060;branch=z9hG4bK74a60ee5;rport
From: \"A user\" <sip:66660000@192.168.1.11>;tag=as2858a32c
To: <sip:5001@192.168.1.200:5060;transport=UDP>
Contact: <sip:66660000@192.168.1.11>
Call-ID: 0bc7612c665e875a4a46411442b930a6@192.168.1.11
CSeq: 102 INVITE
User-Agent: Asterisk PBX
Max-Forwards: 70
Date: Fri, 27 Jun 2008 08:46:47 GMT
Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, SUBSCRIBE, NOTIFY
Supported: replaces
Content-Type: application/sdp
Content-Length: 285

v=0
o=root 4236 4236 IN IP4 192.168.1.11
s=session
c=IN IP4 192.168.1.11
t=0 0
m=audio 14390 RTP/AVP 0 3 8 101
a=rtpmap:0 PCMU/8000
a=rtpmap:3 GSM/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-16
a=silenceSupp:off - - - -
a=ptime:20
a=sendrecv
"""


sendto_cfg = sip.SendtoCfg( "Asterisk 500", "--null-audio --auto-answer 200", 
			    "", 200, complete_msg=complete_msg)

