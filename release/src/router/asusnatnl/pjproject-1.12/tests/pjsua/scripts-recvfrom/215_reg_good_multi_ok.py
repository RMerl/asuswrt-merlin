# $Id: 215_reg_good_multi_ok.py 2392 2008-12-22 18:54:58Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

pjsua = "--null-audio --id=sip:CLIENT --registrar sip:127.0.0.1:$PORT " + \
	"--username theuser1 --realm python1 --password passwd --next-cred " + \
	"--username theuser2 --realm python2 --password passwd " + \
	"--auto-update-nat=0"

req1 = sip.RecvfromTransaction("Initial registration", 401,
				include=["REGISTER sip"], 
				resp_hdr=["WWW-Authenticate: Digest realm=\"python1\", nonce=\"1234\"",
					  "WWW-Authenticate: Digest realm=\"python2\", nonce=\"6789\""],
				expect="SIP/2.0 401"
			  )

req2 = sip.RecvfromTransaction("Registration retry with auth", 200,
				include=["REGISTER sip", 
					 "Authorization:[\\s\\S]+Authorization:", # Must have 2 Auth hdrs
					 "realm=\"python1\"", "realm=\"python2\"", 
					 "username=\"theuser1\"", "username=\"theuser2\"", 
					 "nonce=\"1234\"", "nonce=\"6789\"", 
					 "response="],
				expect="registration success"	     
			  )

recvfrom_cfg = sip.RecvfromCfg("Multiple authentication challenges",
			       pjsua, [req1, req2])
