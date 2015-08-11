# $Id: 110_tel_uri.py 2451 2009-02-13 10:13:08Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Handling of incoming tel: URI.
complete_msg = \
"""INVITE tel:+2065551212 SIP/2.0
Via: SIP/2.0/UDP $LOCAL_IP:$LOCAL_PORT;rport;x-route-tag="tgrp:cococisco1";branch=z9hG4bK61E05
From: <tel:12345>$FROM_TAG
To: <tel:+2065551212>
Date: Thu, 12 Feb 2009 18:32:33 GMT
Call-ID: 58F8F7D6-F86A11DD-8013D591-5694EF79
Supported: 100rel,timer,resource-priority
Min-SE:  86400
Cisco-Guid: 1492551325-4167700957-2148586897-1452601209
User-Agent: Cisco-SIPGateway/IOS-12.x
Allow: INVITE, OPTIONS, BYE, CANCEL, ACK, PRACK, UPDATE, REFER, SUBSCRIBE, NOTIFY, INFO, REGISTER
CSeq: 101 INVITE
Max-Forwards: 70
Timestamp: 1234463553
Contact: <tel:+1234;ext=1>
Contact: <sip:tester@$LOCAL_IP:$LOCAL_PORT>
Record-Route: <sip:tester@$LOCAL_IP:$LOCAL_PORT;lr>
Expires: 180
Allow-Events: telephone-event
Content-Type: application/sdp
Content-Disposition: session;handling=required
Content-Length: 265

v=0
o=CiscoSystemsSIP-GW-UserAgent 1296 9529 IN IP4 X.X.X.X
s=SIP Call
c=IN IP4 $LOCAL_IP
t=0 0
m=audio 18676 RTP/AVP 0 101 19
c=IN IP4 $LOCAL_IP
a=rtpmap:0 PCMU/8000
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-16
a=rtpmap:19 CN/8000
a=ptime:20
"""

sendto_cfg = sip.SendtoCfg( "tel: URI", "--null-audio --auto-answer 200", 
			    "", 200, complete_msg=complete_msg)

