# $Id: 202_reg_good_ok_wildcard.py 2392 2008-12-22 18:54:58Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

pjsua = "--null-audio --id=sip:CLIENT --registrar sip:127.0.0.1:$PORT " + \
	"--username user --realm \"*\" --password passwd --auto-update-nat=0"

req1 = sip.RecvfromTransaction("Initial registration", 401,
				include=["REGISTER sip"], 
				exclude=["Authorization"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"1234\""],
				expect="SIP/2.0 401"
			  )

req2 = sip.RecvfromTransaction("Registration retry with auth", 200,
				include=["REGISTER sip", "Authorization:", 
					     "realm=\"python\"", "username=\"user\"", 
					     "nonce=\"1234\"", "response="],
				expect="registration success"	     
			  )

recvfrom_cfg = sip.RecvfromCfg("Successful registration with wildcard realm test",
			       pjsua, [req1, req2])
