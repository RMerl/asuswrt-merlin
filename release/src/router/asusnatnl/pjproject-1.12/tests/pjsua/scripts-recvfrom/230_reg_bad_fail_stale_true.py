# $Id: 230_reg_bad_fail_stale_true.py 2392 2008-12-22 18:54:58Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# In this test we simulate broken server, where it always sends
# stale=true with all 401 responses. We should expect pjsip to
# retry the authentication until PJSIP_MAX_STALE_COUNT is
# exceeded. When pjsip retries the authentication, it should
# use the new nonce from server


pjsua = "--null-audio --id=sip:CLIENT --registrar sip:127.0.0.1:$PORT " + \
	"--realm=python --user=username --password=password"

req1 = sip.RecvfromTransaction("Initial request", 401,
				include=["REGISTER sip"], 
				exclude=["Authorization"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"1\""]
			  	)

req2 = sip.RecvfromTransaction("First retry", 401,
				include=["REGISTER sip", "Authorization", "nonce=\"1\""], 
				exclude=["Authorization:[\\s\\S]+Authorization:"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"2\", stale=true"]
			  	)

req3 = sip.RecvfromTransaction("Second retry retry", 401,
				include=["REGISTER sip", "Authorization", "nonce=\"2\""], 
				exclude=["Authorization:[\\s\\S]+Authorization:"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"3\", stale=true"]
				)

req4 = sip.RecvfromTransaction("Third retry", 401,
				include=["REGISTER sip", "Authorization", "nonce=\"3\""], 
				exclude=["Authorization:[\\s\\S]+Authorization:"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"4\", stale=true"],
				expect="PJSIP_EAUTHSTALECOUNT"
			  	)

recvfrom_cfg = sip.RecvfromCfg("Failed registration retry (server rejects with stale=true) ",
			       pjsua, [req1, req2, req3, req4])
