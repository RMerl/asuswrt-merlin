# $Id: 500_pres_subscribe_with_bad_event.py 2273 2008-09-11 10:25:51Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Ticket http://trac.pjsip.org/repos/ticket/623, based on
# http://lists.pjsip.org/pipermail/pjsip_lists.pjsip.org/2008-September/004709.html:
#
# Assertion when receiving SUBSCRIBE with non-presence Event.
complete_msg = \
"""SUBSCRIBE sip:localhost;transport=UDP SIP/2.0
Call-ID: f20e8783e764cae325dba17be4b8fe19@10.0.2.15
CSeq: 1 SUBSCRIBE
From: <sip:localhost>;tag=1710895
To: <sip:localhost>
Via: SIP/2.0/UDP localhost;rport;branch=z9hG4bKd88a.18c427d2.0
Max-Forwards: 69
Event:  message-summary
Contact: <sip:localhost>
Allow: NOTIFY, SUBSCRIBE
Content-Length: 0

"""


sendto_cfg = sip.SendtoCfg( "Incoming SUBSCRIBE with non presence", 
			    "--null-audio", 
			    "", 489, complete_msg=complete_msg)

