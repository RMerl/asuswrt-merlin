# $Id: 203_reg_good_empty_realm.py 3150 2010-04-29 00:23:43Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

pjsua = "--null-audio --id=sip:CLIENT --registrar sip:127.0.0.1:$PORT " + \
	"--realm=* --user=username --password=password " + \
	"--auto-update-nat=0"

# 401 Response, missing realm value
req1 = sip.RecvfromTransaction("Initial request", 401,
				include=["REGISTER sip"], 
				exclude=[],
				resp_hdr=['WWW-Authenticate: Digest']
			  	)

# Client should retry, we giving it another 401 with empty realm
req2 = sip.RecvfromTransaction("REGISTER retry #1 of 2", 407,
				include=["REGISTER sip"], 
				exclude=[],
				resp_hdr=['Proxy-Authenticate: Digest realm=""']
			  	)

# Client should retry
req3 = sip.RecvfromTransaction("REGISTER retry #2 of 2", 200,
				include=[], 
				exclude=[],
				expect="registration success"
			  	)

recvfrom_cfg = sip.RecvfromCfg("Registration with empty realm",
			       pjsua, [req1, req2, req3])
