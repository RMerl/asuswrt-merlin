# $Id: 001_torture_4475_3_1_1_2.py 2505 2009-03-12 11:25:11Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Torture message from RFC 4475
# 3.1.1.  Valid Messages
# 3.1.1.2.  Wide Range of Valid Characters
complete_msg = \
"""!interesting-Method0123456789_*+`.%indeed'~ sip:1_unusual.URI~(to-be!sure)&isn't+it$/crazy?,/;;*:&it+has=1,weird!*pas$wo~d_too.(doesn't-it)@example.com SIP/2.0
Via: SIP/2.0/UDP host1.example.com;rport;branch=z9hG4bK-.!%66*_+`'~
To: "BEL:\\\x07 NUL:\\\x00 DEL:\\\x7F" <sip:1_unusual.URI~(to-be!sure)&isn't+it$/crazy?,/;;*@example.com>
From: token1~` token2'+_ token3*%!.- <sip:mundane@example.com> ;fromParam''~+*_!.-%="\xD1\x80\xD0\xB0\xD0\xB1\xD0\xBE\xD1\x82\xD0\xB0\xD1\x8E\xD1\x89\xD0\xB8\xD0\xB9";tag=_token~1'+`*%!-.
Call-ID: intmeth.word%ZK-!.*_+'@word`~)(><:\\/"][?}{
CSeq: 139122385 !interesting-Method0123456789_*+`.%indeed'~
Max-Forwards: 255
extensionHeader-!.%*+_`'~: \xEF\xBB\xBF\xE5\xA4\xA7\xE5\x81\x9C\xE9\x9B\xBB
Content-Length: 0

"""


sendto_cfg = sip.SendtoCfg( "RFC 4475 3.1.1.2", 
			    "--null-audio --auto-answer 200", 
			    "", 405, complete_msg=complete_msg)

