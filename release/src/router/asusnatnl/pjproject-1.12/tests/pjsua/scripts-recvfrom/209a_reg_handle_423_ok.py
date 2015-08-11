# $Id: 209a_reg_handle_423_ok.py 3105 2010-02-23 11:03:07Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

pjsua = "--null-audio --id=sip:CLIENT --registrar sip:127.0.0.1:$PORT " + \
	"--realm=python --user=username --password=password " + \
	"--auto-update-nat=0"

# 423 Response without Min-Expires header
req1 = sip.RecvfromTransaction("Initial request", 423,
				include=["REGISTER sip"], 
				exclude=[],
				resp_hdr=[]
			  	)

# Client should retry with Expires header containing special value (pjsip specific)
req2 = sip.RecvfromTransaction("REGISTER retry after 423 response without Min-Expires header", 423,
				include=["REGISTER sip", "Expires: 3601"], 
				exclude=[],
				resp_hdr=["Min-Expires: 3612"]
			  	)

# Client should retry with proper Expires header now
req3 = sip.RecvfromTransaction("REGISTER retry after proper 423", 200,
				include=["Expires: 3612"], 
				exclude=[],
				expect="registration success"
			  	)

recvfrom_cfg = sip.RecvfromCfg("Reregistration after 423 response",
			       pjsua, [req1, req2, req3])
