# $Id: 209c_reg_handle_423_bad_min_expires2.py 3105 2010-02-23 11:03:07Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

pjsua = "--null-audio --id=sip:CLIENT --registrar sip:127.0.0.1:$PORT " + \
	"--realm=python --user=username --password=password " + \
	"--auto-update-nat=0 --reg-timeout 300"

# 423 without Min-Expires. PJSIP would retry with Expires: 3601
req1 = sip.RecvfromTransaction("Initial request", 423,
				include=["REGISTER sip"], 
				exclude=[],
				resp_hdr=[]
			  	)

# Another 423, still without Min-Expires
req2 = sip.RecvfromTransaction("Retry with guessed Expires header", 423,
				include=["REGISTER sip", "Expires: 3601"], 
				exclude=[],
				resp_hdr=[],
				expect="without Min-Expires header is invalid"
			  	)

recvfrom_cfg = sip.RecvfromCfg("Invalid 423 response to REGISTER",
			       pjsua, [req1, req2])
