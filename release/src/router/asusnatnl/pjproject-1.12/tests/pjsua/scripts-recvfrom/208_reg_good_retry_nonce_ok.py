# $Id: 208_reg_good_retry_nonce_ok.py 2392 2008-12-22 18:54:58Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

pjsua = "--null-audio --id=sip:CLIENT --registrar sip:127.0.0.1:$PORT " + \
	"--realm=python --user=username --password=password " + \
	"--auto-update-nat=0"

req1 = sip.RecvfromTransaction("Initial request", 401,
				include=["REGISTER sip"], 
				exclude=["Authorization"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"1\""]
			  	)

req2 = sip.RecvfromTransaction("REGISTER first retry", 401,
				include=["REGISTER sip", "Authorization", "nonce=\"1\""], 
				exclude=["Authorization:[\\s\\S]+Authorization:"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"2\", stale=true"]
			  	)


req3 = sip.RecvfromTransaction("REGISTER retry with new nonce", 200,
				include=["REGISTER sip", "Authorization", "nonce=\"2\""], 
				exclude=["Authorization:[\\s\\S]+Authorization:"],
				expect="registration success"
			  	)

recvfrom_cfg = sip.RecvfromCfg("Authentication okay after retry with new nonce",
			       pjsua, [req1, req2, req3])
