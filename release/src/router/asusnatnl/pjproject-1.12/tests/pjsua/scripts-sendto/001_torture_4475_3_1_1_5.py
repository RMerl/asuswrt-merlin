# $Id: 001_torture_4475_3_1_1_5.py 2505 2009-03-12 11:25:11Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Torture message from RFC 4475
# 3.1.1.  Valid Messages
# 3.1.1.5. Use of % When It Is Not an Escape 
complete_msg = \
"""RE%47IST%45R sip:registrar.example.com SIP/2.0
To: "%Z%45" <sip:resource@example.com>
From: "%Z%45" <sip:resource@example.com>;tag=f232jadfj23
Call-ID: esc02.asdfnqwo34rq23i34jrjasdcnl23nrlknsdf
Via: SIP/2.0/TCP host.example.com;rport;branch=z9hG4bK209%fzsnel234
CSeq: 29344 RE%47IST%45R
Max-Forwards: 70
Contact: <sip:alias1@host1.example.com>
C%6Fntact: <sip:alias2@host2.example.com>
Contact: <sip:alias3@host3.example.com>
l: 0
"""

sendto_cfg = sip.SendtoCfg( "RFC 4475 3.1.1.5", 
			    "--null-audio --auto-answer 200", 
			    "", 405, complete_msg=complete_msg)

