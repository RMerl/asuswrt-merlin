# $Id: 001_torture_4475_3_1_1_4.py 2505 2009-03-12 11:25:11Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Torture message from RFC 4475
# 3.1.1.  Valid Messages
# 3.1.1.4. Escaped Nulls in URIs
complete_msg = \
"""REGISTER sip:example.com SIP/2.0
To: sip:null-%00-null@example.com
From: sip:null-%00-null@example.com;tag=839923423
Max-Forwards: 70
Call-ID: escnull.39203ndfvkjdasfkq3w4otrq0adsfdfnavd
CSeq: 14398234 REGISTER
Via: SIP/2.0/UDP host5.example.com;rport;branch=z9hG4bKkdjuw
Contact: <sip:%00@host5.example.com>
Contact: <sip:%00%00@host5.example.com>
L:0
"""


sendto_cfg = sip.SendtoCfg( "RFC 4475 3.1.1.4", 
			    "--null-audio --auto-answer 200", 
			    "", 405, complete_msg=complete_msg)

