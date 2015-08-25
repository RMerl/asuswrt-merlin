# $Id: 360_non_sip_uri.py 2392 2008-12-22 18:54:58Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Some non-SIP URI's in Contact header
#
complete_msg = \
"""INVITE sip:localhost SIP/2.0
Via: SIP/2.0/UDP 192.168.0.14:5060;rport;branch=z9hG4bKPj9db9
Max-Forwards: 70
From: <sip:192.168.0.14>;tag=08cd5bfc2d8a4fddb1f5e59c6961d298
To: <sip:localhost>
Call-ID: 3373d9eb32aa458db7e69c7ea51e0bd7
CSeq: 0 INVITE
Contact: mailto:dontspam@pjsip.org
Contact: <mailto:dontspam@pjsip.org>
Contact: http://www.pjsip.org/the%20path.cgi?pname=pvalue
Contact: <sip:localhost>
User-Agent: PJSUA v0.9.0-trunk/win32
Content-Length: 0
"""


sendto_cfg = sip.SendtoCfg( "Non SIP URI in Contact", 
			    "--null-audio --auto-answer 200", 
			    "", 200, complete_msg=complete_msg)

