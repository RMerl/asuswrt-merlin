# $Id: 159_no_rport_nit.py 2442 2009-02-06 08:44:23Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Ticket http://trac.pjsip.org/repos/ticket/718
# RTC doesn't put rport in Via, and it is reported to have caused segfault.
#
complete_msg = \
"""MESSAGE sip:localhost SIP/2.0
Via: SIP/2.0/UDP localhost:$LOCAL_PORT;branch=z9hG4bK$BRANCH
From: <sip:tester@localhost>;tag=as2858a32c
To: <sip:pjsua@localhost>
Call-ID: 123@localhost
CSeq: 1 MESSAGE
Max-Forwards: 70
Content-Length: 11
Content-Type: text/plain

Hello world
"""


sendto_cfg = sip.SendtoCfg( "RTC no rport", "--null-audio --auto-answer 200", 
			    "", 200, complete_msg=complete_msg)

