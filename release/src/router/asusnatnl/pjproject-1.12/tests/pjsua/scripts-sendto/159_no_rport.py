# $Id: 159_no_rport.py 2442 2009-02-06 08:44:23Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Ticket http://trac.pjsip.org/repos/ticket/718
# RTC doesn't put rport in Via, and it is report to have caused segfault.
complete_msg = \
"""INVITE sip:localhost SIP/2.0
Via: SIP/2.0/UDP $LOCAL_IP:$LOCAL_PORT;branch=z9hG4bK74a60ee5
From: <sip:tester@localhost>;tag=as2858a32c
To: <sip:pjsua@localhost>
Contact: <sip:tester@$LOCAL_IP:$LOCAL_PORT>
Call-ID: 123@localhost
CSeq: 1 INVITE
Max-Forwards: 70
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


sendto_cfg = sip.SendtoCfg( "RTC no rport", "--null-audio --auto-answer 200", 
			    "", 200, complete_msg=complete_msg)

