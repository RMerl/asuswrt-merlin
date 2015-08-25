# $Id: 001_torture_4475_3_1_1_3.py 2505 2009-03-12 11:25:11Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Torture message from RFC 4475
# 3.1.1.  Valid Messages
# 3.1.1.3. Valid Use of the % Escaping Mechanism
complete_msg = \
"""INVITE sip:sips%3Auser%40example.com@example.net SIP/2.0
To: sip:%75se%72@example.com
From: <sip:I%20have%20spaces@example.net>;tag=$FROM_TAG
Max-Forwards: 87
i: esc01.239409asdfakjkn23onasd0-3234
CSeq: 234234 INVITE
Via: SIP/2.0/UDP host5.example.net;rport;branch=z9hG4bKkdjuw
C: application/sdp
Contact:
  <sip:cal%6Cer@$LOCAL_IP:$LOCAL_PORT;%6C%72;n%61me=v%61lue%25%34%31>
Content-Length: 150

v=0
o=mhandley 29739 7272939 IN IP4 192.0.2.1
s=-
c=IN IP4 192.0.2.1
t=0 0
m=audio 49217 RTP/AVP 0 12
m=video 3227 RTP/AVP 31
a=rtpmap:31 LPC
"""


sendto_cfg = sip.SendtoCfg( "RFC 4475 3.1.1.3", 
			    "--null-audio --auto-answer 200", 
			    "", 200, complete_msg=complete_msg)

