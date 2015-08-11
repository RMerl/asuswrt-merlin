# $Id: 234_reg_bad_stale_ok.py 2392 2008-12-22 18:54:58Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# In this test we simulate broken server, where it wants to
# change the nonce, but it fails to set stale to true. In this
# case, we should expect pjsip to retry the authentication until
# PJSIP_MAX_STALE_COUNT is exceeded as it should have detected
# that that nonce has changed


pjsua = "--null-audio --id=sip:CLIENT --registrar sip:127.0.0.1:$PORT " + \
	"--realm=python --user=username --password=password " + \
	"--auto-update-nat=0"

req1 = sip.RecvfromTransaction("Initial request", 401,
				include=["REGISTER sip"], 
				exclude=["Authorization"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"1\""]
			  	)

req2 = sip.RecvfromTransaction("First retry", 401,
				include=["REGISTER sip", "Authorization", "nonce=\"1\""], 
				exclude=["Authorization:[\\s\\S]+Authorization:"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"2\""]
			  	)

req3 = sip.RecvfromTransaction("Second retry retry", 401,
				include=["REGISTER sip", "Authorization", "nonce=\"2\""], 
				exclude=["Authorization:[\\s\\S]+Authorization:"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"3\""]
				)

req4 = sip.RecvfromTransaction("Third retry", 200,
				include=["REGISTER sip", "Authorization", "nonce=\"3\""], 
				exclude=["Authorization:[\\s\\S]+Authorization:"],
				expect="registration success"
			  	)

recvfrom_cfg = sip.RecvfromCfg("Successful auth server changes nonce but with stale=false",
			       pjsua, [req1, req2, req3, req4])
