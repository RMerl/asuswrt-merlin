# $Id: 209b_reg_handle_423_bad_min_expires1.py 3105 2010-02-23 11:03:07Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

pjsua = "--null-audio --id=sip:CLIENT --registrar sip:127.0.0.1:$PORT " + \
	"--realm=python --user=username --password=password " + \
	"--auto-update-nat=0 --reg-timeout 300"

# 423 Response with Min-Expires header that is lower than what the client
# had requested
req1 = sip.RecvfromTransaction("Initial request", 423,
				include=["REGISTER sip"], 
				exclude=[],
				resp_hdr=["Min-Expires: 250"],
				expect="invalid Min-Expires"

			  	)

recvfrom_cfg = sip.RecvfromCfg("Invalid 423 response to REGISTER",
			       pjsua, [req1])
