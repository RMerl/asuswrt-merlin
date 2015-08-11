# $Id: 206_reg_good_efailedcredential.py 2392 2008-12-22 18:54:58Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Authentication failure test with same nonce


pjsua = "--null-audio --id=sip:CLIENT --registrar sip:127.0.0.1:$PORT " + \
	"--realm=python --user=username --password=password"

req1 = sip.RecvfromTransaction("Initial request", 401,
				include=["REGISTER sip"], 
				exclude=["Authorization"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"1\""]
			  	)

req2 = sip.RecvfromTransaction("REGISTER retry", 401,
				include=["REGISTER sip", "Authorization", "nonce=\"1\""], 
				exclude=["Authorization:[\\s\\S]+Authorization:"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"1\""],
				expect="PJSIP_EFAILEDCREDENTIAL"
			  	)


recvfrom_cfg = sip.RecvfromCfg("Authentication failure with same nonce",
			       pjsua, [req1, req2])
