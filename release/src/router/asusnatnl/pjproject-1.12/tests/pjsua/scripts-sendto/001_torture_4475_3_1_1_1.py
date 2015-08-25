# $Id: 001_torture_4475_3_1_1_1.py 2505 2009-03-12 11:25:11Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Torture message from RFC 4475
# 3.1.1.  Valid Messages
# 3.1.1.1.  A Short Tortuous INVITE
complete_msg = \
"""INVITE sip:vivekg@chair-dnrc.example.com;unknownparam SIP/2.0
TO :
 sip:vivekg@chair-dnrc.example.com ;   tag    = 1918181833n
from   : "J Rosenberg \\\\\\""       <sip:jdrosen@example.com>
  ;
  tag = 98asjd8
MaX-fOrWaRdS: 0068
Call-ID: wsinv.ndaksdj@192.0.2.1
Content-Length   : 150
cseq: 0009
  INVITE
Via  : SIP  /   2.0
 /UDP
    192.0.2.2;rport;branch=390skdjuw
s :
NewFangledHeader:   newfangled value
 continued newfangled value
UnknownHeaderWithUnusualValue: ;;,,;;,;
Content-Type: application/sdp
Route:
 <sip:services.example.com;lr;unknownwith=value;unknown-no-value>
v:  SIP  / 2.0  / TCP     spindle.example.com   ;
  branch  =   z9hG4bK9ikj8  ,
 SIP  /    2.0   / UDP  192.168.255.111   ; branch=
 z9hG4bK30239
m:"Quoted string \\"\\"" <sip:jdrosen@example.com> ; newparam =
      newvalue ;
  secondparam ; q = 0.33

v=0
o=mhandley 29739 7272939 IN IP4 192.0.2.3
s=-
c=IN IP4 192.0.2.4
t=0 0
m=audio 49217 RTP/AVP 0 12
m=video 3227 RTP/AVP 31
a=rtpmap:31 LPC
"""


sendto_cfg = sip.SendtoCfg( "RFC 4475 3.1.1.1", 
			    "--null-audio --auto-answer 200", 
			    "", 481, complete_msg=complete_msg)

